#include <WiFiNINA.h>
#include "secrets.h"
#include <ThingSpeak.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Display Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pin Definitions
#define BUTTON_PIN 2
#define RELAY_PIN 14

// WiFi Credentials from secrets.h
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

WiFiClient client;
String myStatus = "";

unsigned long myChannelNumber = SECRET_CH_ID;
const char* myWriteAPIKey = SECRET_WRITE_APIKEY;
const char* myAPIKey = APIKEY;

// IFTTT Settings
char HOST_NAME[] = "maker.ifttt.com";
String PATH_NAME = "/trigger/BatteryCapacityTestComplete/with/key/lv8ZbcFFWAfx6Vwizyzw1";
String queryString = "?value1=57&value2=25";
bool requestMade = false;

unsigned long startMillis = 0; // Start time of the test
unsigned long displayHours = 0; // Hours for elapsed time display
unsigned long displayMinutes = 0; // Minutes for elapsed time display
unsigned long displaySeconds = 0; // Seconds for elapsed time display

unsigned long previousMillis = 0; // Store the last time ThingSpeak was updated
const long interval = 60000; // Interval to update ThingSpeak (1 minute)
unsigned long previousDisplayMillis = 0; // Store the last time the display was updated
const long displayInterval = 1000; // Interval to update the display (1 second)

bool buttonPressed = false; // Button press flag
float valBatt; // Variable to store the analog value read
float mAh = 0.0; // Capacity in mAh
float batteryVoltage = 0.0; // Battery voltage
float shuntRes = 2.5; // Shunt resistor resistance in Ohms
float current = 0.0; // Current in Amperes
float battLow = 3.0; // Low battery voltage threshold

void setup() 
{
  pinMode(BUTTON_PIN, INPUT); // Set button pin as input
  pinMode(RELAY_PIN, OUTPUT); // Set relay pin as output
  digitalWrite(RELAY_PIN, LOW); // Ensure the relay is off initially

  // Initialize WiFi connection
  WiFi.begin(ssid, pass);
  
  // Initialize the I2C bus
  Wire.begin();
  
  Serial.begin(9600);
  while (!Serial);

  // Initialize OLED Display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) 
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Stay in loop if OLED allocation fails
  }

  display.display();
  delay(2000); // Pause for 2 seconds
  display.clearDisplay();
  batteryVoltage = calcVoltage(); 
  startingVoltage = batteryVoltage;
  
  // Display the initial message
  startupLcd();

  // Clear ThingSpeak channel data before starting a new test
  clearThingSpeakChannel();

  ThingSpeak.begin(client);  // Initialize ThingSpeak 
}

void clearThingSpeakChannel()
{
  // Attempt to connect to the ThingSpeak API server
  if (client.connect("api.thingspeak.com", 80)) 
  {
    Serial.println("Connected to ThingSpeak API");
    
    // Send a DELETE request to clear the ThingSpeak channel feeds
    client.print("DELETE /channels/");
    client.print(myChannelNumber); // Include the channel number in the request URL
    client.print("/feeds HTTP/1.1\r\n");
    client.print("Host: api.thingspeak.com\r\n"); // Specify the host
    client.print("Connection: close\r\n"); // Close the connection after the request
    client.print("X-THINGSPEAKAPIKEY: " + String(myAPIKey) + "\r\n\r\n"); // Include the API key for authentication

    // Wait for the response from the server, with a timeout of 5 seconds
    unsigned long timeout = millis();
    while (client.available() == 0) 
    {
      if (millis() - timeout > 5000) 
      {
        Serial.println("Timeout occurred while waiting for response");
        client.stop(); // Stop the client if timeout occurs
        return;
      }
    }

    // Read and print the response from the server
    while (client.available()) 
    {
      char c = client.read();
      Serial.print(c);
    }

    // Close the connection
    client.stop();
    Serial.println();
    Serial.println("ThingSpeak channel cleared."); // Indicate that the channel has been cleared
  } 
  else 
  {
    // Print an error message if the connection to the ThingSpeak API failed
    Serial.println("Connection to ThingSpeak API failed");
  }
}

void makeRequest(String path, String query) 
{
  // Attempt to connect to the specified server using the provided hostname and port 80
  if (client.connect(HOST_NAME, 80)) 
  {
    Serial.println("Connected to server");
    
    // Send an HTTP GET request to the server
    client.print("GET " + path + query + " HTTP/1.1\r\n");
    client.print("Host: " + String(HOST_NAME) + "\r\n"); // Specify the host header
    client.print("Connection: close\r\n\r\n"); // Close the connection after the request
    
    // Wait for the server response with a timeout of 5 seconds
    unsigned long timeout = millis();
    while (client.available() == 0) 
    {
      if (millis() - timeout > 5000) 
      {
        Serial.println("Timeout occurred while waiting for response");
        client.stop(); // Stop the client if timeout occurs
        return;
      }
    }
  } 
  else 
  {
    // Print an error message if the connection to the server failed
    Serial.println("Connection to server failed");
    return;
  }

  // Read and print the response from the server while the connection is still open
  while (client.connected()) 
  {
    if (client.available()) 
    {
      char c = client.read();
      Serial.print(c); // Print each character of the response to the Serial Monitor
    }
  }

  // Close the connection to the server
  client.stop();
  Serial.println();
  Serial.println("Disconnected"); // Indicate that the client has disconnected
}

void elapasedTime()
{
  // Get the current time in milliseconds
  unsigned long currentMillis = millis();
  // Calculate the elapsed time in milliseconds since the start of the test
  unsigned long elapsedMillis = currentMillis - startMillis;
  // Convert the elapsed time from milliseconds to seconds
  unsigned long elapsedSeconds = elapsedMillis / 1000;
  // Convert the elapsed time from seconds to minutes
  unsigned long elapsedMinutes = elapsedSeconds / 60;
  // Convert the elapsed time from minutes to hours
  unsigned long elapsedHours = elapsedMinutes / 60;
  // Calculate the number of hours that should be displayed (mod 60 to get the remainder)
  displayHours = elapsedHours % 60;
  // Calculate the number of minutes that should be displayed (mod 60 to get the remainder)
  displayMinutes = elapsedMinutes % 60;
  // Calculate the number of seconds that should be displayed (mod 60 to get the remainder)
  displaySeconds = elapsedSeconds % 60;
}

float calcVoltage() 
{
  // Initialize a variable to store the total voltage sum
  float totalVoltage = 0.0;
  // Define the number of readings to average
  const int numReadings = 4;
  // Loop to take multiple voltage readings
  for (int i = 0; i < numReadings; i++) 
  {
    // Read the divided battery voltage
    float dividVolt = readBattVolt();
    // Convert the divided voltage to the actual battery voltage
    float actualVolt = dividVolt * 2;
    // Add the actual voltage to the total voltage sum
    totalVoltage += actualVolt;
    // Small delay between readings to stabilize the readings
    delay(10);
  }
  // Calculate the average voltage from the total sum of readings
  float averageVoltage = totalVoltage / numReadings;
  // Return the average voltage
  return averageVoltage;
}

float calcCurrent()
{
  float batteryVolt = calcVoltage(); // Get the battery voltage
  shuntRes = 2.5; // Load resistor in ohms
  float current = batteryVolt / shuntRes; // Calculate current using Ohm's Law
  return current;
}

float readBattVolt() 
{
  // Read the analog value from pin A1
  valBatt = analogRead(A1);
  // Print the raw digital value to the serial monitor
  Serial.print("The digital value is: ");
  // Convert the raw digital value to the corresponding voltage
  // Assuming a 10-bit ADC with a reference voltage of 5V, 
  // the factor 313.0 is derived to map the raw value to the voltage
  float dividVolt = valBatt / 313.0; // Ensure float division
  // Print the calculated divided voltage to the serial monitor
  Serial.println(dividVolt);
  // Return the divided voltage
  return dividVolt;
}

void updateThingSpeak()
{
  // Check if the WiFi connection is lost
  if(WiFi.status() != WL_CONNECTED)
  {
    // Print a message to the serial monitor indicating an attempt to reconnect
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(SECRET_SSID);
    // Loop until the WiFi is reconnected
    while(WiFi.status() != WL_CONNECTED)
    {
      // Attempt to connect to the WiFi network with the specified credentials
      WiFi.begin(ssid, pass);
      // Print a dot to indicate an ongoing connection attempt
      Serial.print(".");
      // Wait for 5 seconds before attempting to connect again
      delay(5000);     
    }
    // Print a message indicating successful connection
    Serial.println("\nConnected.");
  }
  // Calculate the battery voltage by averaging multiple readings
  batteryVoltage = calcVoltage();
  // Set the first field of the ThingSpeak channel to the battery voltage
  ThingSpeak.setField(1, batteryVoltage);
  // Set the status message to be sent to ThingSpeak
  ThingSpeak.setStatus(myStatus);
  // Attempt to write the data to ThingSpeak and get the response code
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  // Check if the update was successful
  if(x == 200)
  {
    // Print a success message to the serial monitor
    Serial.println("Channel update successful.");
  }
  else
  {
    // Print an error message with the HTTP error code if the update failed
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }
}


void lcdDisplay()
{
  //Time
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Time: ");
  display.print(displayHours);
  display.print("h ");
  display.print(displayMinutes);
  display.print("m ");
  display.print(displaySeconds);
  display.print("s");

  //Voltage 
  display.setCursor(0, 10);
  display.print("Voltage: ");
  display.print(batteryVoltage, 2);
  display.print("V");
  
  // Add current and mAh display
  display.setCursor(0, 20);
  display.print("Current: ");
  display.print(current, 2);
  display.print(" A");
  
  display.setCursor(0, 30);
  display.print("Capacity: ");
  display.print(mAh);
  display.print(" mAh");
  
  display.display();
}

void finishLcd(unsigned long hours, unsigned long minutes, unsigned long seconds)
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Test Complete");
  display.setCursor(0, 10);
  display.print("Elapsed Time: ");
  display.setCursor(0, 20);
  display.print(displayHours);
  display.print("h ");
  display.print(minutes);
  display.print("m ");
  display.print(seconds);
  display.print("s");
  display.setCursor(0, 30);
  display.print("Capacity: ");
  display.print(mAh);
  display.print(" mAh");
  display.setCursor(0, 40);
  display.print("Voltage: ");
  display.setCursor(0, 50);
  display.print(batteryVoltage);
  display.print("V");
  display.display();
}

void startupLcd()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Press Button To Start");
  display.setCursor(0, 10);
  display.print("Voltage: ");
  display.print(batteryVoltage, 2);
  display.print("V");
  display.display();
}

void relayOn()
{
  digitalWrite(RELAY_PIN, HIGH);
  Serial.println("Relay turned on");
}

void relayOff()
{
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("Relay turned off");
}

void loop() 
{
  // Get the current time in milliseconds since the program started
  unsigned long currentMillis = millis();

  // If the button has not been pressed yet
  if (!buttonPressed) 
  {
    // Continuously read and display battery voltage before the button is pressed
    if (currentMillis - previousDisplayMillis >= displayInterval) 
    {
      previousDisplayMillis = currentMillis; // Update the previous display time
      batteryVoltage = calcVoltage(); // Update voltage by averaging multiple readings
      startupLcd(); // Display the startup message and battery voltage
    }

    // Check if the button is pressed
    if (digitalRead(BUTTON_PIN) == LOW) 
    {
      Serial.println("Button pressed");
      buttonPressed = true; // Set buttonPressed to true
      delay(1000); // Debounce delay to avoid multiple readings from one press
      startMillis = millis(); // Record the start time of the test
      updateThingSpeak(); // Update ThingSpeak with initial data
      relayOn(); // Turn on the relay to start the test
    }
  }
  else 
  {
    // Main loop after the button is pressed
    if (currentMillis - previousDisplayMillis >= displayInterval) 
    {
      previousDisplayMillis = currentMillis; // Update the previous display time
      batteryVoltage = calcVoltage(); // Update voltage before displaying
      current = calcCurrent(); // Calculate current using voltage and shunt resistor value
      mAh += (current / 3.6); // Update mAh based on current and time interval (mA*h = (A * 1000) * (h / 1000))
      elapasedTime(); // Update the elapsed time
      lcdDisplay(); // Update the OLED display with current data
    }

    // Update ThingSpeak data at regular intervals
    if (currentMillis - previousMillis >= interval) 
    {
      previousMillis = currentMillis; // Update the previous ThingSpeak update time
      updateThingSpeak(); // Send data to ThingSpeak
    }

    // Check if the battery voltage drops below the threshold and the IFTTT request has not been made
    if (batteryVoltage <= battLow && !requestMade) 
    {
      makeRequest(PATH_NAME, queryString); // Make a request to IFTTT
      requestMade = true; // Set requestMade to true to avoid multiple requests
      relayOff(); // Turn off the relay to stop the test

      // Loop to keep reading and displaying voltage after the test is complete
      while (true) 
      {
        batteryVoltage = calcVoltage(); // Continuously update voltage
        finishLcd(displayHours, displayMinutes, displaySeconds); // Display the test complete message and results
        delay(displayInterval); // Maintain display update interval
      }
    }
  }
}

