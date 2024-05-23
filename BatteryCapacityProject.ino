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

unsigned long startMillis = 0;
unsigned long displayMinutes = 0;
unsigned long displaySeconds = 0;

unsigned long previousMillis = 0; // Store the last time the voltage was updated
const long interval = 20000; // Interval at which to update ThingSpeak (20 secs)
unsigned long previousDisplayMillis = 0; // Store the last time the display was updated
const long displayInterval = 1000; // Interval at which to update the display (1 second)

bool buttonPressed = false;
float valPot; // Variable to store the analog value read
float mAh = 0.0;
float batteryVoltage = 0.0;
float shuntRes = 2.5; // In Ohms – Shunt resistor resistance
float current = 0.0;
float battLow = 3.0;
float startingVoltage;

void setup() 
{
  pinMode(BUTTON_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
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
    for (;;);
  }

  display.display();
  delay(2000); // Pause for 2 seconds
  display.clearDisplay();
  batteryVoltage = readVoltage(); 
  startingVoltage = batteryVoltage;
  
  // Display the initial message
  helloLcd();

  // Clear ThingSpeak channel data before starting a new test
  clearThingSpeakChannel();

  ThingSpeak.begin(client);  // Initialize ThingSpeak 
}

void clearThingSpeakChannel()
{
  if (client.connect("api.thingspeak.com", 80)) 
  {
    Serial.println("Connected to ThingSpeak API");
    client.print("DELETE /channels/");
    client.print(myChannelNumber);
    client.print("/feeds HTTP/1.1\r\n");
    client.print("Host: api.thingspeak.com\r\n");
    client.print("Connection: close\r\n");
    client.print("X-THINGSPEAKAPIKEY: " + String(myAPIKey) + "\r\n\r\n");

    unsigned long timeout = millis();
    while (client.available() == 0) 
    {
      if (millis() - timeout > 5000) 
      {
        Serial.println("Timeout occurred while waiting for response");
        client.stop();
        return;
      }
    }

    while (client.available()) 
    {
      char c = client.read();
      Serial.print(c);
    }

    client.stop();
    Serial.println();
    Serial.println("ThingSpeak channel cleared.");
  } 
  else 
  {
    Serial.println("Connection to ThingSpeak API failed");
  }
}

void makeRequest(String path, String query) 
{
  if (client.connect(HOST_NAME, 80)) 
  {
    Serial.println("Connected to server");
    client.print("GET " + path + query + " HTTP/1.1\r\n");
    client.print("Host: " + String(HOST_NAME) + "\r\n");
    client.print("Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) 
    {
      if (millis() - timeout > 5000) 
      {
        Serial.println("Timeout occurred while waiting for response");
        client.stop();
        return;
      }
    }
  } 
  else 
  {
    Serial.println("Connection to server failed");
    return;
  }

  while (client.connected()) 
  {
    if (client.available()) 
    {
      char c = client.read();
      Serial.print(c);
    }
  }

  client.stop();
  Serial.println();
  Serial.println("Disconnected");
}

void elapasedTime()
{
  unsigned long currentMillis = millis();
  unsigned long elapsedMillis = currentMillis - startMillis;
  unsigned long elapsedSeconds = elapsedMillis / 1000;
  unsigned long elapsedMinutes = elapsedSeconds / 60;
  displayMinutes = elapsedMinutes % 60;
  displaySeconds = elapsedSeconds % 60;
}

float readVoltage() 
{
  float dividVolt = readPotent();
  float actualVolt = dividVolt * 2;
  return actualVolt;
}

float readCurrent()
{
  float batteryVolt = readVoltage(); // Get the battery voltage
  shuntRes = 2.5; // Load resistor in ohms
  float current = batteryVolt / shuntRes; // Calculate current using Ohm's Law
  return current;
}

float readPotent() 
{
  valPot = analogRead(A1);
  Serial.print("The digital value is: ");
  float dividVolt = valPot / 313.0; // Ensure float division
  Serial.println(dividVolt);
  return dividVolt;
}

void updateThingSpeak()
{
  if(WiFi.status() != WL_CONNECTED)
  {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(SECRET_SSID);
    while(WiFi.status() != WL_CONNECTED)
    {
      WiFi.begin(ssid, pass);
      Serial.print(".");
      delay(5000);     
    } 
    Serial.println("\nConnected.");
  }

  batteryVoltage = readVoltage();
  ThingSpeak.setField(1, batteryVoltage);
  ThingSpeak.setStatus(myStatus);
  
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if(x == 200){
    Serial.println("Channel update successful.");
  }
  else{
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

void goodbyeLcd(unsigned long minutes, unsigned long seconds)
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Test Complete");
  display.setCursor(0, 10);
  display.print("Elapsed Time: ");
  display.print(minutes);
  display.print("m ");
  display.print(seconds);
  display.print("s");
  display.setCursor(0, 20);
  display.print("Capacity: ");
  display.print(mAh);
  display.print(" mAh");
  display.setCursor(0, 30);
  display.print("Starting Voltage: ");
  display.setCursor(0, 40);
  display.print(startingVoltage);
  display.print("V");
  display.display();
}

void helloLcd()
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
  if (!buttonPressed && digitalRead(BUTTON_PIN) == LOW) 
  {
    Serial.println("Button pressed");
    buttonPressed = true;
    delay(1000);
    startMillis = millis();
    updateThingSpeak();
    relayOn();
  }
  
  if (buttonPressed)
  {
    unsigned long currentMillis = millis();

    if (currentMillis - previousDisplayMillis >= displayInterval)
    {
      batteryVoltage = readVoltage(); // Update voltage before displaying
      current = readCurrent(); // Implement readCurrent() to read current value
      mAh += (current / 3.6); // Update mAh based on current and time interval
      previousDisplayMillis = currentMillis;
      elapasedTime();
      lcdDisplay();

    }

    if (currentMillis - previousMillis >= interval) 
    {
      previousMillis = currentMillis;
      updateThingSpeak();
    }

    if (batteryVoltage <= battLow && !requestMade) 
    {
      makeRequest(PATH_NAME, queryString);
      requestMade = true;
      goodbyeLcd(displayMinutes, displaySeconds);
      relayOff();
      while (true); // Stop the loop
    }
  }
}
