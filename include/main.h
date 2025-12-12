#ifndef __MAIN_H
#define __MAIN_H

#include <Arduino.h>
#include <WiFi.h>

#include "app_config.h"

#define DEVICE_TYPE     "sensor"

/**
 * @brief Wi-Fi SSID to use when connecting to internet
 * 
 */
extern String wifiSSID;

/**
 * @brief Wi-Fi password to use when connecting to internet
 * @note This value gets cleared after connected to wifif
 */
extern String wifiPass;

/**
 * @brief Name of the deivce. This name will be used
 * inside of the network
 * 
 */
extern String deviceName;

/**
 * @brief IP address of the MQTT broker
 * @note This is typically the same as `coordinatorIP`
 * 
 */
extern IPAddress brokerIP;
/**
 * @brief Port to use for the MQTT broker
 * 
 */
extern uint16_t brokerPort;

/**
 * @brief IP address of the coordinator HTTP endpoint
 * @note This is typically the same as `brokerIP`
 */
extern IPAddress coordinatorIP;
/**
 * @brief Port to use for the coordinator HTTP endpoint
 * 
 */
extern uint16_t coordinatorPort;



#endif // __MAIN_H