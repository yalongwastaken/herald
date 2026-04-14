/**
 * @file buzzer.h
 * @author Anthony Yalong
 * @brief Active buzzer component for herald actuator nodes.
 *        Provides initialization and timed activation of a GPIO-driven active buzzer.
 */
#ifndef BUZZER_H
#define BUZZER_H

// ── Includes ──────────────────────────────────────────────────────────────────
#include "esp_err.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ── Types ─────────────────────────────────────────────────────────────────────

/** @brief Buzzer device handle. Holds hardware configuration for a single buzzer instance. */
typedef struct {
    gpio_num_t pin;     // GPIO pin connected to buzzer
} buzzer_t;

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * @brief Initialize the buzzer GPIO pin.
 *
 * @param pin       GPIO number the buzzer is connected to
 * @param buzzer    Pointer to buzzer handle to populate
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if buzzer is NULL
 */
esp_err_t buzzer_init(gpio_num_t pin, buzzer_t *buzzer);

/**
 * @brief Activate the buzzer for a specified duration.
 *        Blocks for the duration of the buzz.
 *
 * @param buzzer        Pointer to initialized buzzer handle
 * @param duration_ms   Duration to activate buzzer in milliseconds
 * @return              ESP_OK on success, ESP_ERR_INVALID_ARG if buzzer is NULL
 */
esp_err_t buzzer_buzz(buzzer_t *buzzer, uint32_t duration_ms);

#endif // BUZZER_H