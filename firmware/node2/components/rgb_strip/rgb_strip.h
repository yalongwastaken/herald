/**
 * @file rgb_strip.h
 * @author Anthony Yalong
 * @brief WS2812B RGB LED strip driver interface.
 *
 * Uses the ESP-IDF LED strip component (esp_driver_led_strip) which internally
 * uses the RMT peripheral to generate the precise WS2812B timing protocol.
 * Each LED requires a 24-bit GRB signal at specific pulse widths that cannot
 * be reliably bit-banged — RMT handles this in hardware.
 */
#ifndef RGB_STRIP_H
#define RGB_STRIP_H

// ── Includes ──────────────────────────────────────────────────────────────────
#include "esp_err.h"
#include "soc/gpio_num.h"
#include "led_strip.h"

// ── Types ─────────────────────────────────────────────────────────────────────

/** @brief Handle and configuration for a WS2812B LED strip. */
typedef struct {
    led_strip_handle_t handle;  // ESP-IDF LED strip handle
    uint32_t num_leds;          // number of LEDs in the strip
} rgb_strip_t;

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * @brief Initialize the WS2812B LED strip.
 *
 * Configures the RMT peripheral and LED strip driver for the given GPIO pin.
 *
 * @param pin       GPIO pin connected to the strip's data line
 * @param num_leds  Number of LEDs in the strip
 * @param strip     Pointer to an rgb_strip_t to initialize
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if strip is NULL
 */
esp_err_t rgb_strip_init(gpio_num_t pin, uint32_t num_leds, rgb_strip_t *strip);

/**
 * @brief Set all LEDs in the strip to a single RGB color.
 *
 * @param r     Red value (0-255)
 * @param g     Green value (0-255)
 * @param b     Blue value (0-255)
 * @param strip Pointer to an initialized rgb_strip_t
 * @return      ESP_OK on success
 */
esp_err_t rgb_strip_set_color(uint8_t r, uint8_t g, uint8_t b, rgb_strip_t *strip);

/**
 * @brief Turn off all LEDs in the strip.
 *
 * @param strip Pointer to an initialized rgb_strip_t
 * @return      ESP_OK on success
 */
esp_err_t rgb_strip_clear(rgb_strip_t *strip);

#endif // RGB_STRIP_H