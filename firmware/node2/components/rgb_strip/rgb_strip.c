/**
 * @file rgb_strip.c
 * @author Anthony Yalong
 * @brief WS2812B RGB LED strip driver implementation.
 */

// ── Includes ──────────────────────────────────────────────────────────────────
#include "rgb_strip.h"
#include "esp_log.h"
#include "led_strip.h"

// ── Configuration ─────────────────────────────────────────────────────────────
static const char *TAG = "rgb_strip";

// ── Public API ────────────────────────────────────────────────────────────────

esp_err_t rgb_strip_init(gpio_num_t pin, uint32_t num_leds, rgb_strip_t *strip) {
    esp_err_t ret;

    // sanity check
    if (strip == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // rmt backend config — 10MHz gives 100ns resolution for WS2812B timing
    led_strip_rmt_config_t rmt_cfg = {
        .clk_src       = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };

    // strip device config
    led_strip_config_t strip_cfg = {
        .strip_gpio_num   = pin,
        .max_leds         = num_leds,
        .led_model        = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    // create rmt-backed led strip device
    ret = led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &strip->handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to init led strip");
        return ret;
    }

    // clear on init so strip starts off
    ret = led_strip_clear(strip->handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to clear strip on init");
        return ret;
    }

    // initialize strip structure
    strip->num_leds = num_leds;

    ESP_LOGD(TAG, "initialized %lu leds on gpio %d", num_leds, (int)pin);
    return ESP_OK;
}

esp_err_t rgb_strip_set_color(uint8_t r, uint8_t g, uint8_t b, rgb_strip_t *strip) {
    esp_err_t ret;

    // sanity check
    if (strip == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // write color to every pixel in buffer
    for (uint32_t i = 0; i < strip->num_leds; i++) {
        ret = led_strip_set_pixel(strip->handle, i, r, g, b);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to set pixel %lu", i);
            return ret;
        }
    }

    // flush buffer to physical strip over RMT
    ret = led_strip_refresh(strip->handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to refresh strip");
        return ret;
    }

    ESP_LOGD(TAG, "color set to r=%d g=%d b=%d", r, g, b);
    return ESP_OK;
}

esp_err_t rgb_strip_clear(rgb_strip_t *strip) {
    esp_err_t ret;

    // sanity check
    if (strip == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // turn off all leds
    ret = led_strip_clear(strip->handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to clear strip");
        return ret;
    }

    ESP_LOGD(TAG, "cleared");
    return ESP_OK;
}