/**
 * @file oled.h
 * @author Anthony Yalong
 * @brief SSD1306 OLED driver using ESP-IDF I2C master API
 *
 * This driver assumes the I2C bus is initialized externally and a device
 * handle is provided. The driver manages an internal framebuffer and
 * provides basic drawing and text rendering functionality.
 */

#ifndef OLED_H
#define OLED_H

// ── Includes ──────────────────────────────────────────────────────────────────
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

// ── Configuration ─────────────────────────────────────────────────────────────

#define OLED_WIDTH    128
#define OLED_HEIGHT   64
#define OLED_BUF_SIZE (OLED_WIDTH * OLED_HEIGHT / 8)

// ── Font Type ─────────────────────────────────────────────────────────────────

typedef struct {
    uint8_t width;         ///< Character width in pixels
    uint8_t height;        ///< Character height in pixels
    const uint8_t *data;   ///< Font bitmap data
} oled_font_t;

// ── Types ─────────────────────────────────────────────────────────────────────

typedef struct {
    i2c_master_dev_handle_t dev;   ///< I2C device handle (provided by caller)
    uint8_t width;                 ///< Display width in pixels
    uint8_t height;                ///< Display height in pixels
    uint8_t buffer[OLED_BUF_SIZE]; ///< Framebuffer (1 bit per pixel)
} oled_t;

// ── Public API ────────────────────────────────────────────────────────────────

esp_err_t oled_init(oled_t *oled, i2c_master_dev_handle_t dev);

esp_err_t oled_flush(oled_t *oled);

void oled_clear(oled_t *oled);

void oled_draw_pixel(oled_t *oled, uint8_t x, uint8_t y, bool color);

/**
 * @brief Draw a single character
 *
 * @param oled Pointer to OLED handle
 * @param x    X coordinate (top-left)
 * @param y    Y coordinate (top-left)
 * @param c    ASCII character
 * @param font Font to use
 * @return ESP_OK on success
 */
esp_err_t oled_draw_char(oled_t *oled,
                         uint8_t x,
                         uint8_t y,
                         char c,
                         const oled_font_t *font);

/**
 * @brief Write a string to the display
 *
 * @param oled Pointer to OLED handle
 * @param x    Starting X coordinate
 * @param y    Starting Y coordinate
 * @param str  Null-terminated string
 * @param font Font to use
 * @return ESP_OK on success
 */
esp_err_t oled_write_string(oled_t *oled,
                            uint8_t x,
                            uint8_t y,
                            const char *str,
                            const oled_font_t *font);

#endif // OLED_H