#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "freertos/FreeRTOS.h"
#include "esp_attr.h"

#include "main.h"
#include "wifi_man.h"

// main.h extern defined variables
const uint16_t brokerPort = BROKER_PORT;
Preferences prefs;

// Application specific objects
// (exclusive to this file)
WiFiClient netif;
PubSubClient mqttClient(netif);
TaskHandle_t mqttNotifTask;
bool wifi_man_complete = false;

// Interrupts
void IRAM_ATTR mqtt_svc_signal_event();
void IRAM_ATTR reset_wifi_man_configs();

// Additional Tasks (created with FreeRTOS)
void mqtt_notif_loop(void* args);

void setup() {
  Serial.begin(BAUD_RATE);
  prefs.begin("app");

  // Enable wifi manager reset pin
  pinMode(CONFIG_WIFI_RST_PIN_NO, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(CONFIG_WIFI_RST_PIN_NO),
                  reset_wifi_man_configs, RISING);

  wifi_man_svc_start();
  wifi_man_complete = true;

  Serial.println("Configuring mqtt...");
  mqttClient.setServer(brokerIP, brokerPort);
  mqttClient.connect(prefs.getString("device_name").c_str());
  Serial.println("Connected to broker!");

  // Bit of a cheat, but creates a task which is responsible 
  // for publishing the message to a broker
  xTaskCreate(mqtt_notif_loop, "MQTT Notif Loop", 4096, NULL, 5, &mqttNotifTask);

  // Enable interrupt which indicates that a message should be published
  pinMode(CONFIG_APP_SENSOR_PIN_NO, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(CONFIG_APP_SENSOR_PIN_NO),
                  mqtt_svc_signal_event, RISING);
}

void loop() {
  mqttClient.loop();
  
  if (!mqttClient.connected()) {
    mqttClient.connect(prefs.getString("device_name").c_str());
    Serial.println("Reconnected to broker!");
  }

  delay(5000);
}

void mqtt_notif_loop(void* args) {
  String topic = "sensor/" + deviceName;
  while (1) {
    // This task will block until something else notifies it
    ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
    Serial.println("Sending topic " + topic);
    mqttClient.publish(topic.c_str(), "hello world");
  }
}

void IRAM_ATTR mqtt_svc_signal_event() {
  // This simply sends a "notification" to whatever task is 
  // "mqttNotifTask". If that task was waiting on a task 
  // notification, it is then unblocked
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;  
  vTaskNotifyGiveFromISR( mqttNotifTask, &xHigherPriorityTaskWoken );
  portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

void IRAM_ATTR reset_wifi_man_configs() {
  Serial.println("Resetting preferences and core...");
  Serial.flush();
  if (wifi_man_complete) {
    wifi_man_reset();
  }
  ESP.restart();
}