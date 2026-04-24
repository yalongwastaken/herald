/**
 * @file lcd.c
 * @author Anthony Yalong
 * @brief I2C LCD1602 display driver implementation using ESP-IDF v5.x I2C master API.
 */

// ── Includes ──────────────────────────────────────────────────────────────────
#include "lcd.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

// ── Configuration ─────────────────────────────────────────────────────────────
static const char *TAG = "lcd";

// ── Private API ───────────────────────────────────────────────────────────────

/**
 * @brief Write a single byte to the PCF8574 I/O expander over I2C.
 *
 * @param lcd       Pointer to initialized LCD handle
 * @param data      Byte to write
 * @return          ESP_OK on success, error code on I2C failure
 */
static esp_err_t lcd_write_i2c(lcd_handle_t *lcd, uint8_t data) {
    return i2c_master_transmit(lcd->dev, &data, 1, I2C_TIMEOUT_MS);
}

/**
 * @brief Pulse the enable pin to latch a nibble into the LCD controller.
 *
 * @param lcd       Pointer to initialized LCD handle
 * @param data      Nibble byte with control bits set
 * @return          ESP_OK on success, error code on I2C failure
 */
static esp_err_t lcd_pulse_enable(lcd_handle_t *lcd, uint8_t data) {
    esp_err_t ret;

    // EN high
    ret = lcd_write_i2c(lcd, data | LCD_EN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set enable high");
        return ret;
    }
    esp_rom_delay_us(1);

    // EN low
    ret = lcd_write_i2c(lcd, data & ~LCD_EN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set enable low");
        return ret;
    }
    esp_rom_delay_us(50);

    return ESP_OK;
}

/**
 * @brief Write one nibble (4 bits) to the LCD with mode and backlight bits set.
 *
 * @param lcd       Pointer to initialized LCD handle
 * @param nibble    Upper 4 bits contain the data (lower 4 bits ignored)
 * @param mode      LCD_RS for data, 0 for command
 * @return          ESP_OK on success, error code on failure
 */
static esp_err_t lcd_write_nibble(lcd_handle_t *lcd, uint8_t nibble, uint8_t mode) {
    uint8_t data = (nibble & 0xF0) | mode | lcd->backlight_state;
    return lcd_pulse_enable(lcd, data);
}

/**
 * @brief Write a full byte to the LCD as two nibbles in 4-bit mode.
 *
 * @param lcd       Pointer to initialized LCD handle
 * @param byte      Byte to write
 * @param mode      LCD_RS for data, 0 for command
 * @return          ESP_OK on success, error code on failure
 */
static esp_err_t lcd_write_byte(lcd_handle_t *lcd, uint8_t byte, uint8_t mode) {
    esp_err_t ret;

    ret = lcd_write_nibble(lcd, byte & 0xF0, mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to write high nibble");
        return ret;
    }

    ret = lcd_write_nibble(lcd, (byte << 4) & 0xF0, mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to write low nibble");
        return ret;
    }

    return ESP_OK;
}

/**
 * @brief Send a command byte to the LCD controller.
 *
 * @param lcd       Pointer to initialized LCD handle
 * @param cmd       Command byte
 * @return          ESP_OK on success, error code on failure
 */
static esp_err_t lcd_send_command(lcd_handle_t *lcd, uint8_t cmd) {
    return lcd_write_byte(lcd, cmd, 0);
}

/**
 * @brief Send a data byte to the LCD controller (writes a character).
 *
 * @param lcd       Pointer to initialized LCD handle
 * @param data      Character byte
 * @return          ESP_OK on success, error code on failure
 */
static esp_err_t lcd_send_data(lcd_handle_t *lcd, uint8_t data) {
    return lcd_write_byte(lcd, data, LCD_RS);
}

// ── Public API ────────────────────────────────────────────────────────────────

esp_err_t lcd_init(i2c_master_dev_handle_t dev, uint8_t cols, uint8_t rows, lcd_handle_t *lcd) {
    esp_err_t ret;

    // sanity check
    if (lcd == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // populate handle
    lcd->dev = dev;
    lcd->cols = cols;
    lcd->rows = rows;
    lcd->backlight_state = LCD_BACKLIGHT;

    // wait for lcd power-up
    vTaskDelay(pdMS_TO_TICKS(50));

    // initialization sequence: send 0x30 three times to ensure 8-bit reset
    ret = lcd_write_nibble(lcd, 0x30, 0);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "init sequence step 1 failed"); return ret; }
    vTaskDelay(pdMS_TO_TICKS(5));

    ret = lcd_write_nibble(lcd, 0x30, 0);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "init sequence step 2 failed"); return ret; }
    vTaskDelay(pdMS_TO_TICKS(1));

    ret = lcd_write_nibble(lcd, 0x30, 0);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "init sequence step 3 failed"); return ret; }
    vTaskDelay(pdMS_TO_TICKS(1));

    // switch to 4-bit mode
    ret = lcd_write_nibble(lcd, 0x20, 0);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "failed to set 4-bit mode"); return ret; }
    vTaskDelay(pdMS_TO_TICKS(1));

    // function set: 4-bit, 2 lines, 5x8
    ret = lcd_send_command(lcd, LCD_CMD_FUNCTION_SET | LCD_4BIT_MODE | LCD_2_LINE | LCD_5x8_DOTS);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "function set failed"); return ret; }

    // display on
    ret = lcd_send_command(lcd, LCD_CMD_DISPLAY_CTRL | LCD_DISPLAY_ON);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "display on failed"); return ret; }

    // clear display
    ret = lcd_clear(lcd);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "initial clear failed"); return ret; }

    // entry mode: left to right, no shift
    ret = lcd_send_command(lcd, LCD_CMD_ENTRY_MODE | LCD_ENTRY_LEFT);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "entry mode set failed"); return ret; }

    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_LOGD(TAG, "initialized (%dx%d)", cols, rows);
    return ESP_OK;
}

esp_err_t lcd_clear(lcd_handle_t *lcd) {
    esp_err_t ret;

    // sanity check
    if (lcd == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    ret = lcd_send_command(lcd, LCD_CMD_CLEAR);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to send clear command");
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(2));

    ESP_LOGD(TAG, "cleared");
    return ESP_OK;
}

esp_err_t lcd_set_cursor(uint8_t col, uint8_t row, lcd_handle_t *lcd) {
    esp_err_t ret;

    // sanity check
    if (lcd == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    if (col >= lcd->cols || row >= lcd->rows) {
        ESP_LOGE(TAG, "cursor position out of bounds: col=%d row=%d", col, row);
        return ESP_ERR_INVALID_ARG;
    }

    static const uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};
    uint8_t addr = col + row_offsets[row];

    ret = lcd_send_command(lcd, LCD_CMD_DDRAM_ADDR | addr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set cursor");
        return ret;
    }

    ESP_LOGD(TAG, "cursor set to col=%d row=%d", col, row);
    return ESP_OK;
}

esp_err_t lcd_backlight(bool state, lcd_handle_t *lcd) {
    esp_err_t ret;

    // sanity check
    if (lcd == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    lcd->backlight_state = state ? LCD_BACKLIGHT : LCD_NO_BACKLIGHT;

    ret = lcd_write_i2c(lcd, lcd->backlight_state);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set backlight");
        return ret;
    }

    ESP_LOGD(TAG, "backlight %s", state ? "on" : "off");
    return ESP_OK;
}

esp_err_t lcd_print(const char *str, lcd_handle_t *lcd) {
    esp_err_t ret;

    // sanity check
    if (lcd == NULL || str == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    while (*str) {
        ret = lcd_send_data(lcd, (uint8_t)*str);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to write character");
            return ret;
        }
        str++;
    }

    ESP_LOGD(TAG, "printed string");
    return ESP_OK;
}