/**
 * @file oled.h
 * @author Anthony Yalong
 * @brief SSD1306 OLED display driver using ESP-IDF I2C master API.
 */
#ifndef OLED_H
#define OLED_H

// ── Includes ──────────────────────────────────────────────────────────────────
#include "esp_err.h"
#include "driver/i2c_master.h"
#include <stdbool.h>
#include "font.h"

// ── Configuration ─────────────────────────────────────────────────────────────
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_BUF_SIZE (OLED_WIDTH * OLED_HEIGHT / 8)

// ── Types ─────────────────────────────────────────────────────────────────────

/** @brief OLED device handle. Holds I2C device and framebuffer. */
typedef struct {
    i2c_master_dev_handle_t dev;   // I2C device handle
    uint8_t buffer[OLED_BUF_SIZE]; // framebuffer (1 bit per pixel)
} oled_t;

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * @brief Initialize the OLED driver and internal state.
 *
 * @param dev       Pre-configured I2C device handle
 * @param oled      Pointer to OLED handle to populate
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if oled is NULL
 */
esp_err_t oled_init(i2c_master_dev_handle_t dev, oled_t *oled);

/**
 * @brief Flush the framebuffer to the display.
 *
 * @param oled      Pointer to initialized OLED handle
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if oled is NULL
 */
esp_err_t oled_flush(oled_t *oled);

/**
 * @brief Clear the framebuffer.
 *
 * @param oled      Pointer to initialized OLED handle
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if any pointer is NULL
 */
esp_err_t oled_clear(oled_t *oled);

/**
 * @brief Write a null-terminated string to the display.
 *
 * @param oled      Pointer to initialized OLED handle
 * @param x         Starting X coordinate
 * @param y         Starting Y coordinate
 * @param str       Null-terminated string
 * @param font      Pointer to font definition
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if any pointer is NULL
 */
esp_err_t oled_write_string(oled_t *oled, uint8_t x, uint8_t y, const char *str, const oled_font_t *font);

#endif // OLED_H