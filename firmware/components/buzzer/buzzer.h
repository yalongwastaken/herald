/**
 * @file buzzer.h
 * @author Anthony Yalong
 * @brief Active buzzer component for herald actuator nodes.
 */
#ifndef BUZZER_H
#define BUZZER_H

// ── Includes ──────────────────────────────────────────────────────────────────
#include "esp_err.h"
#include "esp_timer.h"
#include "soc/gpio_num.h"

// ── Types ─────────────────────────────────────────────────────────────────────

/** @brief Buzzer device handle. Holds hardware configuration for a single buzzer instance. */
typedef struct {
    gpio_num_t pin;               // GPIO pin connected to buzzer
    esp_timer_handle_t timer;     // Handle to ESP timer used to drive buzzer timing
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
 *        Non-blocking: returns immediately while timer handles shutdown.
 *
 * @param buzzer        Pointer to initialized buzzer handle
 * @param duration_us   Duration to activate buzzer in microseconds
 * @return              ESP_OK on success, ESP_ERR_INVALID_ARG if buzzer is NULL
 */
esp_err_t buzzer_buzz(buzzer_t *buzzer, uint32_t duration_us);

/**
 * @brief Stop the buzzer regardless of state.
 *
 * @param buzzer        Pointer to initialized buzzer handle
 * @return              ESP_OK on success, ESP_ERR_INVALID_ARG if buzzer is NULL
 */
esp_err_t buzzer_stop(buzzer_t *buzzer);

#endif // BUZZER_H