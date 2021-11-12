#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <sensitive.h>

void setupWiFi() {
  Serial.println("Setting up Wifi.");
  
  WiFi.disconnect();
  delay(5 * 1000);

  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  WiFi.config(0U, 0U, 0U); // use DHCP
  WiFi.setHostname("iotSumpPump");
  WiFi.begin(WIFI_NETWORK, WIFI_PASSWORD);
  
  delay(15 * 1000);
  for(int n = 0; n < 8; n++) {
    if(WiFi.status() == WL_CONNECTED) {
        break;
    }
    delay(5 * 1000);
  }

  Serial.print("Setup done. "); Serial.print(WiFi.localIP()); Serial.print(" "); Serial.println(WiFi.macAddress());
}

bool ensureWiFi() {
  bool wifiOK = (WiFi.status() == WL_CONNECTED);
  if(!wifiOK) {
    setupWiFi();
  }
  return (WiFi.status() == WL_CONNECTED);
}