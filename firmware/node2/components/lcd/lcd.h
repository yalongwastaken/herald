/**
 * @file lcd.h
 * @author Anthony Yalong
 * @brief I2C LCD1602 display driver.
 */
#ifndef LCD_H
#define LCD_H

// ── Includes ──────────────────────────────────────────────────────────────────
#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c_master.h"
#include "esp_err.h"

#define LCD_CMD_CLEAR           0x01
#define LCD_CMD_HOME            0x02
#define LCD_CMD_ENTRY_MODE      0x04
#define LCD_CMD_DISPLAY_CTRL    0x08
#define LCD_CMD_FUNCTION_SET    0x20
#define LCD_CMD_DDRAM_ADDR      0x80

#define LCD_ENTRY_LEFT          0x02
#define LCD_DISPLAY_ON          0x04
#define LCD_4BIT_MODE           0x00
#define LCD_2_LINE              0x08
#define LCD_5x8_DOTS            0x00

#define LCD_BACKLIGHT           0x08
#define LCD_NO_BACKLIGHT        0x00
#define LCD_EN                  0x04
#define LCD_RW                  0x02
#define LCD_RS                  0x01

#define I2C_TIMEOUT_MS          1000

// ── Types ─────────────────────────────────────────────────────────────────────

/** @brief LCD device handle. Holds I2C device handle and display configuration. */
typedef struct {
    i2c_master_dev_handle_t dev;
    uint8_t cols;
    uint8_t rows;
    uint8_t backlight_state;
} lcd_handle_t;

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * @brief Initialize the LCD display.
 *
 * @param dev       Pre-configured I2C device handle
 * @param cols      Number of columns (typically 16)
 * @param rows      Number of rows (typically 2)
 * @param lcd       Pointer to LCD handle to populate
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if lcd is NULL
 */
esp_err_t lcd_init(i2c_master_dev_handle_t dev, uint8_t cols, uint8_t rows, lcd_handle_t *lcd);

/**
 * @brief Clear the LCD display and return cursor to home.
 *
 * @param lcd       Pointer to initialized LCD handle
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if lcd is NULL
 */
esp_err_t lcd_clear(lcd_handle_t *lcd);

/**
 * @brief Set the cursor position.
 *
 * @param col       Column (0 to cols-1)
 * @param row       Row (0 to rows-1)
 * @param lcd       Pointer to initialized LCD handle
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if lcd is NULL or position is out of bounds
 */
esp_err_t lcd_set_cursor(uint8_t col, uint8_t row, lcd_handle_t *lcd);

/**
 * @brief Control the LCD backlight.
 *
 * @param state     true = on, false = off
 * @param lcd       Pointer to initialized LCD handle
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if lcd is NULL
 */
esp_err_t lcd_backlight(bool state, lcd_handle_t *lcd);

/**
 * @brief Print a null-terminated string at the current cursor position.
 *
 * @param str       Null-terminated string to print
 * @param lcd       Pointer to initialized LCD handle
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if any pointer is NULL
 */
esp_err_t lcd_print(const char *str, lcd_handle_t *lcd);

#endif // LCD_I2C_H