#include <WiFiNINA.h>
#include "secrets.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 20, 4); // set the LCD address to 0x27 for a 20 chars and 4 line display

// please enter your sensitive data in the Secret tab
char ssid[] = SECRET_SSID; // your network SSID (name)
char pass[] = SECRET_PASS; // your network password (use for WPA, or use as key for WEP)

WiFiClient client;

char HOST_NAME[] = "maker.ifttt.com";
String PATH_NAME = "/trigger/BatteryCapacityTestComplete/with/key/lv8ZbcFFWAfx6Vwizyzw1"; // change your EVENT-NAME and YOUR-KEY
String queryString = "?value1=57&value2=25";
bool requestMade = false;

unsigned long startMillis = 0;
unsigned long displayMinutes = 0;
unsigned long displaySeconds = 0;

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

  // Record the start time of the test
  startMillis = millis();
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

void lcdDisplay()
{
  lcd.setCursor(0,0);
  lcd.print("Time: ");
  lcd.print(displayMinutes);
  lcd.print("m ");
  lcd.print(displaySeconds);
  lcd.print("s ");
  lcd.setCursor(0,2);
  lcd.print("Voltage");
} 

void loop() 
{
  elapasedTime();
  lcdDisplay();
  //makeRequest(PATH_NAME, queryString);
}
