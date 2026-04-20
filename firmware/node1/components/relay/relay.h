/**
 * @file relay.h
 * @author Anthony Yalong
 * @brief Relay driver for GPIO-controlled relay modules.
 */
#ifndef RELAY_H
#define RELAY_H

// ── Includes ──────────────────────────────────────────────────────────────────
#include "esp_err.h"
#include "soc/gpio_num.h"

// ── Types ─────────────────────────────────────────────────────────────────────

/** @brief Relay device handle. Holds hardware configuration for a single relay instance. */
typedef struct {
    gpio_num_t pin;              // GPIO pin connected to relay control
    uint32_t current_state;      // cached relay state (true = 1, false = 0)
} relay_t;

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * @brief Initialize the relay GPIO pin and instance.
 *
 * @param pin       GPIO number the relay is connected to
 * @param relay     Pointer to relay handle to populate
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if relay is NULL or pin is invalid
 */
esp_err_t relay_init(gpio_num_t pin, relay_t *relay);

/**
 * @brief Set relay state.
 *
 * @param state     Desired relay state (true = 1, false = 0)
 * @param relay     Pointer to initialized relay handle
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if relay is NULL
 */
esp_err_t relay_set(uint32_t state, relay_t *relay);

/**
 * @brief Toggle relay state.
 *
 * @param relay     Pointer to initialized relay handle
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if relay is NULL
 */
esp_err_t relay_toggle(relay_t *relay);

/**
 * @brief Read cached relay state.
 *
 * @param relay     Pointer to initialized relay handle
 * @param state     Pointer to boolean output variable
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if relay or state is NULL
 */
esp_err_t relay_read(relay_t *relay, uint32_t *state);

#endif // RELAY_H