#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

/**
 * @brief UART Baud Rate
 * 
 */
#define CONFIG_BAUD_RATE        (PIO_CONFIG_BAUD_RATE)

/**
 * @brief Defined pin number to set as ther
 * sensor pin
 * @note This pin shall be capable of generating
 * and interrupt
 */
#define APP_SENSOR_PIN_NO       (PIO_APP_SENSOR_PIN_NO)

#endif // __APP_CONFIG_H