/**
 * @file hcsr04.h
 * @author Anthony Yalong
 * @brief HC-SR04 ultrasonic distance sensor driver.
 */
#ifndef HCSR04_H
#define HCSR04_H

// ── Includes ──────────────────────────────────────────────────────────────────
#include <stdint.h>
#include "soc/gpio_num.h"
#include "esp_err.h"

// ── Types ─────────────────────────────────────────────────────────────────────

/** @brief HC-SR04 device handle. Holds pin configuration and last measured distance. */
typedef struct {
    gpio_num_t trig_pin;       // GPIO pin for trigger signal
    gpio_num_t echo_pin;       // GPIO pin for echo signal
    float last_distance_cm;    // last measured distance in cm
    uint32_t timeout_us;       // echo timeout in microseconds
} hcsr04_sensor_t;

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * @brief Initialize HC-SR04 ultrasonic sensor.
 *
 * Configures trigger pin as output and echo pin as input.
 *
 * @param trig_pin  GPIO pin number for trigger
 * @param echo_pin  GPIO pin number for echo
 * @param timeout   Timeout in microseconds to wait for echo pulse before aborting
 * @param sensor    Pointer to sensor handle to populate
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if sensor is NULL
 */
esp_err_t hcsr04_init(gpio_num_t trig_pin, gpio_num_t echo_pin, uint32_t timeout, hcsr04_sensor_t *sensor);

/**
 * @brief Trigger a measurement and store the result in the sensor handle.
 *
 * Sends a 10us trigger pulse and measures echo response time.
 * Blocks until echo completes or timeout expires.
 *
 * @param sensor    Pointer to initialized sensor handle
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if sensor is NULL,
 *                  ESP_ERR_TIMEOUT if echo pulse not received within timeout
 */
esp_err_t hcsr04_measure(hcsr04_sensor_t *sensor);

/**
 * @brief Read the last measured distance.
 *
 * @param distance  Pointer to variable where distance will be stored (in cm)
 * @param sensor    Pointer to initialized sensor handle
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if any pointer is NULL
 */
esp_err_t hcsr04_get_distance(float *distance, hcsr04_sensor_t *sensor);

#endif // HCSR04_H