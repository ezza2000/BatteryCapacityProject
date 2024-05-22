#include <WiFiNINA.h>
#include "secrets.h"
#include <ThingSpeak.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 20, 4); // set the LCD address to 0x27 for a 20 chars and 4 line display

// please enter your sensitive data in the Secret tab
char ssid[] = SECRET_SSID; // your network SSID (name)
char pass[] = SECRET_PASS; // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;       
WiFiClient client;
String myStatus = "";

unsigned long myChannelNumber = SECRET_CH_ID; 
const char* myWriteAPIKey = SECRET_WRITE_APIKEY; 
const char* myAPIKey = APIKEY;

char HOST_NAME[] = "maker.ifttt.com";
String PATH_NAME = "/trigger/BatteryCapacityTestComplete/with/key/lv8ZbcFFWAfx6Vwizyzw1"; // change your EVENT-NAME and YOUR-KEY
String queryString = "?value1=57&value2=25";
bool requestMade = false;

unsigned long startMillis = 0;
unsigned long displayMinutes = 0;
unsigned long displaySeconds = 0;

float voltage = 4.2; // Initial voltage value
unsigned long previousMillis = 0; // Store the last time the voltage was updated
const long interval = 60000; // Interval at which to update ThingSpeak (1 minute)
unsigned long previousDisplayMillis = 0; // Store the last time the display was updated
const long displayInterval = 1000; // Interval at which to update the display (1 second)

void setup() 
{
  // initialize WiFi connection
  WiFi.begin(ssid, pass);
  // Initialize the I2C bus
  Wire.begin();
  // On esp8266 you can select SCL and SDA pins using Wire.begin(D4, D3);
  // For Wemos / Lolin D1 Mini Pro and the Ambient Light shield use
  // Wire.begin(D2, D1);

  Serial.begin(9600);
  while (!Serial);

  // connect to web server on port 80:
  if (client.connect(HOST_NAME, 80)) 
  {
    // if connected:
    Serial.println("Connected to server");
  }
  else // if not connected:
  {
    Serial.println("connection failed");
  }

  // initialize the lcd 
  lcd.init();                     
  // Print a message to the LCD.
  lcd.backlight();

  // Clear ThingSpeak channel data before starting a new test
  clearThingSpeakChannel();

  // Record the start time of the test
  startMillis = millis();

  ThingSpeak.begin(client);  // Initialize ThingSpeak 
}
void clearThingSpeakChannel()
{
  // Clear the ThingSpeak channel data
  if (client.connect("api.thingspeak.com", 80)) {
    Serial.println("Connected to ThingSpeak API");
    client.print("DELETE /channels/");
    client.print(myChannelNumber);
    client.print("/feeds HTTP/1.1\r\n");
    client.print("Host: api.thingspeak.com\r\n");
    client.print("Connection: close\r\n");
    client.print("X-THINGSPEAKAPIKEY: " + String(myAPIKey) + "\r\n\r\n");

    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println("Timeout occurred while waiting for response");
        client.stop();
        return;
      }
    }

    while (client.available()) {
      char c = client.read();
      Serial.print(c);
    }

    // Close connection
    client.stop();
    Serial.println();
    Serial.println("ThingSpeak channel cleared.");
  } else {
    Serial.println("Connection to ThingSpeak API failed");
  }
}

void makeRequest(String path, String query) 
{
  // Send HTTP request
  if (client.connect(HOST_NAME, 80)) {
    Serial.println("Connected to server");
    client.print("GET " + path + query + " HTTP/1.1\r\n");
    client.print("Host: " + String(HOST_NAME) + "\r\n");
    client.print("Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) { // 5-second timeout
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

  // Read response from server
  while (client.connected()) 
  {
    if (client.available()) 
    {
      char c = client.read();
      Serial.print(c);
    }
  }

  // Close connection
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

float readVoltage() {
  // Hypothetical voltage drop simulation
  if (voltage > 3.0) { // Ensure voltage does not drop below 3.0V
    voltage -= 0.01; // Decrease voltage by 0.01V per call
  }
  return voltage;
}

void voltageDrop()
{
  // Connect or reconnect to WiFi
  if(WiFi.status() != WL_CONNECTED){
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(SECRET_SSID);
    while(WiFi.status() != WL_CONNECTED){
      WiFi.begin(ssid, pass);  // Connect to WPA/WPA2 network. Change this line if using open or WEP network
      Serial.print(".");
      delay(5000);     
    } 
    Serial.println("\nConnected.");
  }

  // Read the voltage
  voltage = readVoltage();

  // set the fields with the values
  ThingSpeak.setField(1, voltage);

  // set the status
  ThingSpeak.setStatus(myStatus);
  
  // write to the ThingSpeak channel 
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
  lcd.setCursor(0, 0);
  lcd.print("Time: ");
  lcd.print(displayMinutes);
  lcd.print("m ");
  lcd.print(displaySeconds);
  lcd.print("s ");
  lcd.setCursor(0, 2);
  lcd.print("Voltage: ");
  lcd.print(voltage, 2);
  lcd.print("V");
}

void loop() 
{
  unsigned long currentMillis = millis();

  // Update elapsed time and display every second
  if (currentMillis - previousDisplayMillis >= displayInterval) {
    previousDisplayMillis = currentMillis;
    elapasedTime();
    lcdDisplay();
  }

  // Update ThingSpeak every minute
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    voltageDrop(); // Send voltage to ThingSpeak
  }
}
