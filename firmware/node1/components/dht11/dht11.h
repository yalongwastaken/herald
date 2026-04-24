/**
 * @file dht11.h
 * @author Anthony Yalong
 * @brief DHT11 temperature and humidity sensor component using single-wire timing protocol.
 */
#ifndef DHT11_H
#define DHT11_H

// ── Includes ──────────────────────────────────────────────────────────────────
#include "esp_err.h"
#include "soc/gpio_num.h"

#define DHT11_START_SIGNAL_LOW_US     18000
#define DHT11_BIT_THRESHOLD_US        50
#define DHT11_BIT_TIMEOUT_US          120
#define DHT11_TIMEOUT_US              1000
#define DHT11_MIN_INTERVAL_US         1000000
#define DHT11_TOTAL_TIMEOUT_US        25000

// ── Types ─────────────────────────────────────────────────────────────────────

/** @brief DHT11 device handle. Holds hardware configuration and cached sensor data. */
typedef struct {
    gpio_num_t pin;      // GPIO pin connected to DHT11 data line
    float temperature;   // last measured temperature (°C)
    float humidity;      // last measured humidity (%)
    int64_t last_update; // last read time
} dht11_t;

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * @brief Initialize the DHT11 GPIO pin and driver state.
 *
 * @param pin       GPIO number the DHT11 is connected to
 * @param dht11     Pointer to DHT11 handle to populate
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if dht11 is NULL or pin is invalid
 */
esp_err_t dht11_init(gpio_num_t pin, dht11_t *dht11);

/**
 * @brief Perform a sensor read and update cached temperature and humidity.
 *
 * @param dht11     Pointer to initialized DHT11 handle
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if dht11 is NULL,
 *                  or error code if communication fails or times out
 */
esp_err_t dht11_update(dht11_t *dht11);

/**
 * @brief Read the last cached temperature and humidity values.
 *
 * @param dht11         Pointer to initialized DHT11 handle
 * @param temperature   Pointer to variable where temperature will be stored
 * @param humidity      Pointer to variable where humidity will be stored
 * @return              ESP_OK on success, ESP_ERR_INVALID_ARG if any pointer is NULL
 */
esp_err_t dht11_read(dht11_t *dht11, float *temperature, float *humidity);

#endif // DHT11_H