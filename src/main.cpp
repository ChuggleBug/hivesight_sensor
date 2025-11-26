#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "time.h"

#include "esp_attr.h"
#include "freertos/FreeRTOS.h"

#include "main.h"

// main.h extern defined variables
String deviceName = "Uknown";
IPAddress brokerIP = IPAddress(0, 0, 0, 0);
uint16_t brokerPort = 0;
IPAddress coordinatorIP = IPAddress(0, 0, 0, 0);
;
uint16_t coordinatorPort = 0;

WiFiClient netif;
WiFiUDP netifUDP;

PubSubClient mqttClient(netif);
NTPClient timeClient(netifUDP);
HTTPClient http;

TaskHandle_t mqttNotifTask;

#define panic(fmt, ...)                                                        \
  do {                                                                         \
    Serial.printf(fmt, ##__VA_ARGS__);                                         \
    Serial.println();                                                          \
    vTaskSuspend(NULL);                                                        \
  } while (0)

// Functions
bool load_device_configs();
void coordinator_register_device();

// Interrupts
void IRAM_ATTR mqtt_svc_signal_event();
void IRAM_ATTR reset_wifi_man_configs();

// Additional Tasks (created with FreeRTOS)
void mqtt_notif_loop(void *args);

void setup() {
  Serial.begin(BAUD_RATE);

  if (!LittleFS.begin()) {
    panic("Failed to init filesystem");
  }

  if (!load_device_configs()) {
    panic("Failed to load configurations");
  }

  Serial.println("");
  Serial.println("Configurations");
  Serial.print("Device Name:      "); Serial.println(deviceName);
  Serial.print("Broker IP:        "); Serial.println(brokerIP);
  Serial.print("Broker Port:      "); Serial.println(brokerPort);
  Serial.print("Coordinator IP:   "); Serial.println(coordinatorIP);
  Serial.print("Coordinator Port: "); Serial.println(coordinatorPort);


  vTaskSuspend(NULL);

  // Enable wifi manager reset pin
  pinMode(CONFIG_WIFI_RST_PIN_NO, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(CONFIG_WIFI_RST_PIN_NO),
                  reset_wifi_man_configs, RISING);

  Serial.println("Configuring mqtt...");
  mqttClient.setServer(coordinatorIP, brokerPort);
  mqttClient.connect(deviceName.c_str());
  Serial.println("Connected to broker!");

  timeClient.begin();

  coordinator_register_device();

  // Bit of a cheat, but creates a task which is responsible
  // for publishing the message to a broker
  xTaskCreate(mqtt_notif_loop, "MQTT Notif Loop", 4096, NULL, 5,
              &mqttNotifTask);

  // Enable interrupt which indicates that a message should be published
  pinMode(CONFIG_APP_SENSOR_PIN_NO, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(CONFIG_APP_SENSOR_PIN_NO),
                  mqtt_svc_signal_event, RISING);
}

void loop() {
  mqttClient.loop();
  timeClient.update();

  if (!mqttClient.connected()) {
    Serial.println("Lost connection to broker");
    if (mqttClient.connect(deviceName.c_str())) {
      Serial.println("Reconnected to broker!");
    } else {
      Serial.println("Failed to reconnect...");
    }
  }

  delay(5000);
}

void mqtt_notif_loop(void *args) {
  String topic = "sensor/" + deviceName;
  ArduinoJson::JsonDocument json;
  static char buf[512];
  memset(buf, 0, 512);
  while (1) {
    // This task will block until something else notifies it
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Report the time of event and send a json
    json["time"] = timeClient.getEpochTime();
    serializeJson(json, buf);

    Serial.println("Sending topic " + topic);
    mqttClient.publish(topic.c_str(), buf);
  }
}

void IRAM_ATTR mqtt_svc_signal_event() {
  // This simply sends a "notification" to whatever task is
  // "mqttNotifTask". If that task was waiting on a task
  // notification, it is then unblocked
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(mqttNotifTask, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void IRAM_ATTR reset_wifi_man_configs() {
  Serial.println("Resetting preferences and core...");
  Serial.flush();
  ESP.restart();
}

void coordinator_register_device() {
  ArduinoJson::JsonDocument json;
  int resp_code;
  static char buf[256];
  memset(buf, 0, 256);

  json["name"] = deviceName;
  json["type"] = "sensor";

  Serial.println(coordinatorIP.toString());
  Serial.println(coordinatorPort);
  http.begin(coordinatorIP.toString(), coordinatorPort, "/api/device/register");
  http.addHeader("Content-Type", "application/json");
  serializeJson(json, buf);
  resp_code = http.PUT(buf);

  Serial.printf("HTTP Response: %d", resp_code);
  Serial.println();
}

bool load_device_configs() {
  ArduinoJson::JsonDocument json;
  fs::File file = LittleFS.open("/config.json");
  char buf[1024];
  memset(buf, 0, 1024);
  file.readBytes(buf, 1023);
  if (deserializeJson(json, buf) != DeserializationError::Ok) {
    return false;
  }

  // Error checking
  if (!json["DeviceName"].is<const char *>() ||
      !json["MQTT"]["IP"].is<const char *>() ||
      !json["MQTT"]["Port"].is<int>() ||
      !json["HTTP"]["IP"].is<const char *>() ||
      !json["HTTP"]["Port"].is<int>()) {
    return false;
  }


  deviceName = json["DeviceName"].as<String>();

  brokerIP.fromString((json["MQTT"]["IP"].as<const char *>()));
  brokerPort = json["MQTT"]["Port"].as<uint16_t>();
  
  coordinatorIP.fromString((json["HTTP"]["IP"].as<const char *>()));
  coordinatorPort = json["HTTP"]["Port"].as<uint16_t>();

  return true;
}