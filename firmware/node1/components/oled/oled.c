/**
 * @file oled.c
 * @author Anthony Yalong
 * @brief SSD1306 OLED display driver implementation using ESP-IDF I2C master API.
 */

// ── Includes ──────────────────────────────────────────────────────────────────
#include <string.h>
#include "font.h"
#include "oled.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

// ── Configuration ─────────────────────────────────────────────────────────────
#define OLED_CTRL_CMD                   0x00
#define OLED_CTRL_DATA                  0x40

#define OLED_CMD_SET_COLUMN_ADDR        0x21
#define OLED_CMD_SET_PAGE_ADDR          0x22

#define OLED_CMD_DISPLAY_OFF            0xAE
#define OLED_CMD_DISPLAY_ON             0xAF

#define OLED_CMD_SET_MEM_ADDR_MODE      0x20
#define OLED_ADDR_MODE_HORIZONTAL       0x00

#define OLED_CMD_SET_PAGE_START_ADDR    0xB0

#define OLED_CMD_SET_COM_SCAN_DIR       0xC8

#define OLED_CMD_SET_LOW_COLUMN         0x00
#define OLED_CMD_SET_HIGH_COLUMN        0x10

#define OLED_CMD_SET_START_LINE         0x40

#define OLED_CMD_SET_CONTRAST           0x81

#define OLED_CMD_SEG_REMAP              0xA1
#define OLED_CMD_NORMAL_DISPLAY         0xA6
#define OLED_CMD_DISPLAY_FOLLOW_RAM     0xA4

#define OLED_CMD_SET_MUX_RATIO          0xA8
#define OLED_MUX_64                     0x3F

#define OLED_CMD_SET_DISPLAY_OFFSET     0xD3
#define OLED_OFFSET_NONE                0x00

#define OLED_CMD_SET_CLOCK_DIV          0xD5
#define OLED_CLOCK_DIV_DEFAULT          0xF0

#define OLED_CMD_SET_PRECHARGE          0xD9
#define OLED_PRECHARGE_DEFAULT          0x22

#define OLED_CMD_SET_COM_PINS           0xDA
#define OLED_COM_PINS_CONFIG            0x12

#define OLED_CMD_SET_VCOM_DETECT        0xDB
#define OLED_VCOM_DEFAULT               0x20

#define OLED_CMD_CHARGE_PUMP            0x8D
#define OLED_CHARGE_PUMP_ENABLE         0x14

static const char *TAG = "oled";

// ── Private API ───────────────────────────────────────────────────────────────

/**
 * @brief Send a single command byte to the OLED controller over I2C.
 *
 * @param oled      Pointer to initialized OLED handle
 * @param cmd       Command byte to send
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if oled is NULL
 */
static esp_err_t oled_send_cmd(oled_t *oled, uint8_t cmd) {
    if (oled == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buffer[2] = {
        OLED_CTRL_CMD,
        cmd
    };

    return i2c_master_transmit(oled->dev, buffer, sizeof(buffer), -1);
}

/**
 * @brief Send one or more data bytes to the OLED controller over I2C.
 *
 * @param oled      Pointer to initialized OLED handle
 * @param data      Pointer to buffer containing data bytes
 * @param len       Number of bytes to transmit
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if oled or data is NULL
 */
static esp_err_t oled_send_data(oled_t *oled, uint8_t *data, size_t len) {
    if (oled == NULL || data == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t ctrl_byte = OLED_CTRL_DATA;

    i2c_master_transmit_multi_buffer_info_t data_buffer[2] = {
        {.write_buffer = &ctrl_byte, .buffer_size = 1},
        {.write_buffer = data, .buffer_size = len}
    };

    return i2c_master_multi_buffer_transmit(oled->dev, data_buffer, sizeof(data_buffer) / sizeof(i2c_master_transmit_multi_buffer_info_t), -1);
}

/**
 * @brief Send the full SSD1306 initialization command sequence.
 *
 * @param oled      Pointer to initialized OLED handle
 * @return          ESP_OK on success, error code if any command transmission fails
 */
static esp_err_t oled_run_init_sequence(oled_t *oled) {
    esp_err_t ret;

    const uint8_t cmds[] = {
        OLED_CMD_DISPLAY_OFF,
        OLED_CMD_SET_MEM_ADDR_MODE,  OLED_ADDR_MODE_HORIZONTAL,
        OLED_CMD_SET_PAGE_START_ADDR,
        OLED_CMD_SET_COM_SCAN_DIR,
        OLED_CMD_SET_LOW_COLUMN,
        OLED_CMD_SET_HIGH_COLUMN,
        OLED_CMD_SET_START_LINE,
        OLED_CMD_SET_CONTRAST,       0xFF,
        OLED_CMD_SEG_REMAP,
        OLED_CMD_NORMAL_DISPLAY,
        OLED_CMD_SET_MUX_RATIO,      OLED_MUX_64,
        OLED_CMD_DISPLAY_FOLLOW_RAM,
        OLED_CMD_SET_DISPLAY_OFFSET, OLED_OFFSET_NONE,
        OLED_CMD_SET_CLOCK_DIV,      OLED_CLOCK_DIV_DEFAULT,
        OLED_CMD_SET_PRECHARGE,      OLED_PRECHARGE_DEFAULT,
        OLED_CMD_SET_COM_PINS,       OLED_COM_PINS_CONFIG,
        OLED_CMD_SET_VCOM_DETECT,    OLED_VCOM_DEFAULT,
        OLED_CMD_CHARGE_PUMP,        OLED_CHARGE_PUMP_ENABLE,
        OLED_CMD_DISPLAY_ON,
    };

    for (size_t i = 0; i < sizeof(cmds); i++) {
        ret = oled_send_cmd(oled, cmds[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "init sequence failed at command index %d", (int)i);
            return ret;
        }
    }

    return ESP_OK;
}

/**
 * @brief Draw a single pixel in the framebuffer.
 *
 * @param oled      Pointer to initialized OLED handle
 * @param x         X coordinate (0 to width-1)
 * @param y         Y coordinate (0 to height-1)
 * @param color     true = pixel ON, false = pixel OFF
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if any pointer is NULL
 */
static esp_err_t oled_draw_pixel(oled_t *oled, uint8_t x, uint8_t y, bool color) {
    if (oled == NULL || x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t index = x + (y / 8) * OLED_WIDTH;
    uint8_t bit = y % 8;

    if (color) {
        oled->buffer[index] |= (1 << bit);
    } else {
        oled->buffer[index] &= ~(1 << bit);
    }

    return ESP_OK;
}

/**
 * @brief Draw a single character using a font.
 *
 * @param oled      Pointer to initialized OLED handle
 * @param x         X coordinate (top-left of character)
 * @param y         Y coordinate (top-left of character)
 * @param c         ASCII character to draw
 * @param font      Pointer to font definition
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if any pointer is NULL
 */
static esp_err_t oled_draw_char(oled_t *oled, uint8_t x, uint8_t y, char c, const oled_font_t *font) {
    if (oled == NULL || font == NULL || font->data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return ESP_ERR_INVALID_ARG;
    }

    // only support printable ASCII (32–127)
    if (c < 32 || c > 127) {
        c = '?';
    }

    // compute glyph offset
    uint16_t offset = (c - 32) * font->width;

    // draw glyph
    for (uint8_t col = 0; col < font->width; col++) {
        uint8_t column_data = font->data[offset + col];

        for (uint8_t row = 0; row < font->height; row++) {
            bool pixel_on = (column_data >> row) & 0x01;

            if ((x + col) < OLED_WIDTH && (y + row) < OLED_HEIGHT) {
                oled_draw_pixel(oled, x + col, y + row, pixel_on);
            }
        }
    }

    return ESP_OK;
}

// ── Public API ────────────────────────────────────────────────────────────────

esp_err_t oled_init(i2c_master_dev_handle_t dev, oled_t *oled) {
    esp_err_t ret;

    // sanity check
    if (oled == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // store device handle
    oled->dev = dev;
    memset(oled->buffer, 0, sizeof(oled->buffer));

    // run initialization sequence
    ret = oled_run_init_sequence(oled);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to run init sequence");
        return ret;
    }

    ESP_LOGD(TAG, "initialized");
    return ESP_OK;
}

esp_err_t oled_flush(oled_t *oled) {
    esp_err_t ret;

    // sanity check
    if (oled == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // define column range
    ret = oled_send_cmd(oled, OLED_CMD_SET_COLUMN_ADDR);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "failed to set column addr cmd"); return ret; }
    ret = oled_send_cmd(oled, 0);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "failed to set column start"); return ret; }
    ret = oled_send_cmd(oled, OLED_WIDTH - 1);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "failed to set column end"); return ret; }

    // define page range
    ret = oled_send_cmd(oled, OLED_CMD_SET_PAGE_ADDR);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "failed to set page addr cmd"); return ret; }
    ret = oled_send_cmd(oled, 0);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "failed to set page start"); return ret; }
    ret = oled_send_cmd(oled, (OLED_HEIGHT / 8) - 1);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "failed to set page end"); return ret; }

    // flush buffer
    ret = oled_send_data(oled, oled->buffer, sizeof(oled->buffer));
    if (ret != ESP_OK) { ESP_LOGE(TAG, "failed to transmit framebuffer"); return ret; }

    ESP_LOGD(TAG, "flushed");
    return ESP_OK;
}

esp_err_t oled_clear(oled_t *oled) {
    // sanity check
    if (oled == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    memset(oled->buffer, 0, sizeof(oled->buffer));

    ESP_LOGD(TAG, "cleared");
    return ESP_OK;
}

esp_err_t oled_write_string(oled_t *oled, uint8_t x, uint8_t y, const char *str, const oled_font_t *font) {
    esp_err_t ret;

    // sanity check
    if (oled == NULL || str == NULL || font == NULL) {
        ESP_LOGE(TAG, "invalid argument");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t cursor_x = x;

    for (size_t i = 0; str[i] != '\0'; i++) {
        ret = oled_draw_char(oled, cursor_x, y, str[i], font);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to draw char at index %d", (int)i);
            return ret;
        }

        cursor_x += font->width + 1;

        if (cursor_x >= OLED_WIDTH) {
            break;
        }
    }

    ESP_LOGD(TAG, "wrote string at (%d, %d)", x, y);
    return ESP_OK;
}