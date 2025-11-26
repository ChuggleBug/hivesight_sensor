#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

/**
 * @brief UART Baud Rate
 * 
 */
#define CONFIG_BAUD_RATE        (PIO_CONFIG_BAUD_RATE)

// Assigned pin to trigger a software reset
// as well as a wifi manager config reset
#define CONFIG_WIFI_RST_PIN_NO  (5)

// Assigned pin to trigger sending a
// mqtt message
#define CONFIG_APP_SENSOR_PIN_NO (16)

// Port for broker
#define BROKER_PORT             (1883)

// Port for coordinator http communication
#define COORDINATOR_HTTP_PORT   (3030)

#endif // __APP_CONFIG_H