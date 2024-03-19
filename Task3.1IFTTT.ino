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
bool requestMade = false;


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
  if (!requestMade) {
    float lux = lightMeter.readLightLevel();
    Serial.print("");
    Serial.print("Lux: ");
    Serial.println(lux); // Print the lux value
    Serial.print("");
    if (lux > 50) {
      makeRequestON();
    }
  }
}

void makeRequestON() {
  // Send HTTP request
  client.println("GET " + PATH_NAME + queryString + " HTTP/1.1");
  client.println("Host: " + String(HOST_NAME));
  client.println("Connection: close");
  client.println();

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
  requestMade = true;
}
    


