#include <Arduino.h>

#include <PubSubClient.h>
#include <WiFi.h>

WiFiClient netif;
PubSubClient mqttClient(netif);

void setup() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.begin(BAUD_RATE);

  

}

void loop() {
  // put your main code here, to run repeatedly:
}
