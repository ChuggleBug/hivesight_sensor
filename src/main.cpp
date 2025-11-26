#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "main.h"
#include "time.h"

// === Macros ===
#define JSON_MAX_SIZE 1024
#define panic(fmt, ...)                                                        \
  do {                                                                         \
    Serial.printf(fmt, ##__VA_ARGS__);                                         \
    Serial.println();                                                          \
    vTaskSuspend(NULL);                                                        \
  } while (0)

// === Externally Defined Variables ===
String deviceName;
IPAddress brokerIP;
uint16_t brokerPort;
IPAddress coordinatorIP;
uint16_t coordinatorPort;

// === Static / Local Variables ===
static String wifiSSID;
static String wifiPass;

WiFiClient netif;
WiFiUDP netifUDP;

PubSubClient mqttClient(netif);
NTPClient timeClient(netifUDP);
HTTPClient http;

TaskHandle_t mqttNotifTask;

// === Function Declarations ===
bool load_device_configs();
void coordinator_register_device();

void IRAM_ATTR mqtt_svc_signal_event();
void mqtt_notif_loop(void *args);

// === Code Begin ===
void setup() {
  Serial.begin(CONFIG_BAUD_RATE);

  if (!LittleFS.begin()) {
    panic("Failed to init filesystem");
  }

  if (!load_device_configs()) {
    panic("Failed to load configurations");
  }

  Serial.println();
  Serial.println("Configurations");
  Serial.print("Wifi SSID:        ");
  Serial.println(wifiSSID);
  Serial.print("Wifi password:    ");
  for (char c : wifiPass) {
    Serial.print('*');
  }
  Serial.println();
  Serial.print("Device Name:      ");
  Serial.println(deviceName);
  Serial.print("Broker IP:        ");
  Serial.println(brokerIP);
  Serial.print("Broker Port:      ");
  Serial.println(brokerPort);
  Serial.print("Coordinator IP:   ");
  Serial.println(coordinatorIP);
  Serial.print("Coordinator Port: ");
  Serial.println(coordinatorPort);
  Serial.println();

  Serial.print("Connecting to network over Wi-Fi");
  WiFi.begin(wifiSSID, wifiPass);
  while (!WiFi.isConnected()) {
    Serial.print('.');
    delay(100);
  }
  Serial.println();

  Serial.println("Connected!");

  Serial.println("Configuring mqtt...");
  mqttClient.setServer(brokerIP, brokerPort);
  mqttClient.connect(deviceName.c_str());
  Serial.println("Connected to broker!");

  timeClient.begin();

  coordinator_register_device();

  xTaskCreate(mqtt_notif_loop, "MQTT Notif Loop", 4096, NULL, 5,
              &mqttNotifTask);

  pinMode(APP_SENSOR_PIN_NO, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(APP_SENSOR_PIN_NO),
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

void coordinator_register_device() {
  ArduinoJson::JsonDocument json;
  int resp_code;
  static char buf[256];
  memset(buf, 0, 256);

  json["name"] = deviceName;
  json["type"] = DEVICE_TYPE;

  http.begin(coordinatorIP.toString(), coordinatorPort, "/api/device/register");
  http.addHeader("Content-Type", "application/json");
  serializeJson(json, buf);
  resp_code = http.PUT(buf);

  Serial.printf("HTTP Response: %d", resp_code);
  Serial.println();
}

bool load_device_configs() {
  ArduinoJson::JsonDocument json;
  static char buf[JSON_MAX_SIZE];

  fs::File file = LittleFS.open("/config.json");
  if (!file) {
    return false;
  }

  memset(buf, 0, JSON_MAX_SIZE);
  file.readBytes(buf, JSON_MAX_SIZE - 1);
  if (deserializeJson(json, buf) != DeserializationError::Ok) {
    return false;
  }

  // Error checking
  if (!json["WiFiSSID"].is<const char *>() ||
      !json["WiFiPassword"].is<const char *>() ||
      !json["DeviceName"].is<const char *>() ||
      !json["MQTT"]["IP"].is<const char *>() ||
      !json["MQTT"]["Port"].is<uint16_t>() ||
      !json["HTTP"]["IP"].is<const char *>() ||
      !json["HTTP"]["Port"].is<uint16_t>()) {
    return false;
  }

  wifiSSID = json["WiFiSSID"].as<String>();
  wifiPass = json["WiFiPassword"].as<String>();

  deviceName = json["DeviceName"].as<String>();

  brokerIP.fromString((json["MQTT"]["IP"].as<const char *>()));
  brokerPort = json["MQTT"]["Port"].as<uint16_t>();

  coordinatorIP.fromString((json["HTTP"]["IP"].as<const char *>()));
  coordinatorPort = json["HTTP"]["Port"].as<uint16_t>();

  return true;
}

void mqtt_notif_loop(void *args) {
  // This task will block until something else notifies it
  String topic = "sensor/" + deviceName;
  ArduinoJson::JsonDocument json;
  static char buf[512];
  memset(buf, 0, 512);
  while (1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Report the time of event and send a JSON
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