#include <WiFiNINA.h>
#include "secrets.h"
#include <BH1750.h>
#include <Wire.h>
BH1750 lightMeter;

//please enter your sensitive data in the Secret tab
char ssid[] = SECRET_SSID;                // your network SSID (name)
char pass[] = SECRET_PASS;                // your network password (use for WPA, or use as key for WEP)

WiFiClient client;

char   HOST_NAME[] = "maker.ifttt.com";
String PATH_NAME   = "/trigger/sunlight_sensed/with/key/lv8ZbcFFWAfx6Vwizyzw1"; // change your EVENT-NAME and YOUR-KEY
String queryString = "?value1=57&value2=25";
String PATH_NAME_OFF = "/trigger/sunlight_absent/with/key/lv8ZbcFFWAfx6Vwizyzw1";
String queryStringOff = "?value1=57&value2=25";
bool requestMade = false;
unsigned long previousMillis = 0; // Variable to store the time since last check
const long interval = 29000;      // Interval between checks in milliseconds
const long duration = 60000;     // Total duration of the loop in milliseconds


void setup() {
  // initialize WiFi connection
  WiFi.begin(ssid, pass);
    // Initialize the I2C bus (BH1750 library doesn't do this automatically)
  Wire.begin();
  // On esp8266 you can select SCL and SDA pins using Wire.begin(D4, D3);
  // For Wemos / Lolin D1 Mini Pro and the Ambient Light shield use
  // Wire.begin(D2, D1);

  lightMeter.begin();

  Serial.println(F("BH1750 Test begin"));

  Serial.begin(9600);
  while (!Serial);

  // connect to web server on port 80:
  if (client.connect(HOST_NAME, 80)) {
    // if connected:
    Serial.println("Connected to server");
  }
  else {// if not connected:
    Serial.println("connection failed");
  }
}

void loop() {
  unsigned long currentMillis = millis(); // Get the current time

  // Check if 2 minutes have elapsed
  if (currentMillis < duration) {
    // Check if it's time to check sunlight
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;// Save the last time sunlight was checked
      float lux = lightMeter.readLightLevel();
      Serial.print("Lux: ");
      Serial.println(lux); // Print the lux value

      // Reset requestMade flag before making a new request
      requestMade = false;

      if (lux > 50 && !requestMade) {
        makeRequest(PATH_NAME, queryString);
        requestMade = true; // Set flag to indicate that a request has been made
      }
      if (lux < 50 && !requestMade){
        makeRequest(PATH_NAME_OFF, queryStringOff);
        requestMade = true; // Set flag to indicate that a request has been made
      }
    }
  } else {
    // End the loop after 2 minutes
    Serial.println("Loop ended after 2 minutes");
    while (true) {} // Infinite loop to halt the program
  }
}

void makeRequest(String path, String query) {
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
  } else {
    Serial.println("Connection to server failed");
    return;
  }

  // Read response from server
  while (client.connected()) {
    if (client.available()) {
      char c = client.read();
      Serial.print(c);
    }
  }

  // Close connection
  client.stop();
  Serial.println();
  Serial.println("Disconnected");
}

    


