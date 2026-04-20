/**
 * @file relay.c
 * @author Anthony Yalong
 * @brief GPIO relay driver for state control and toggling.
 */

// ── Includes ──────────────────────────────────────────────────────────────────
#include "esp_err.h"
#include "esp_log.h"
#include "relay.h"
#include "driver/gpio.h"

// ── Configuration ─────────────────────────────────────────────────────────────
static const char *TAG = "relay";

// ── Public API ────────────────────────────────────────────────────────────────
esp_err_t relay_init(gpio_num_t pin, relay_t *relay) {
    // error management
    esp_err_t ret;
    
    // sanity check
    if (relay == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // configure gpio
    gpio_config_t config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << pin),
        .intr_type = GPIO_INTR_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };
    ret = gpio_config(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure GPIO %d", (int)pin);
        return ret;
    }

    // set initial state
    ret = gpio_set_level(pin, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set initial state for GPIO %d", (int)pin);
        return ret;
    }

    // initialize relay structure
    relay->pin = pin;
    relay->current_state = 0;
    ESP_LOGI(TAG, "initialized on GPIO %d", (int)pin);
    return ESP_OK;
}

esp_err_t relay_set(uint32_t state, relay_t *relay) {
    // error management
    esp_err_t ret;

    // sanity check
    if (relay == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // set level
    ret = gpio_set_level(relay->pin, state);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set state for GPIO %d", (int)relay->pin);
        return ret;
    }

    // update relay structure
    relay->current_state = state;
    ESP_LOGI(TAG, "successfully set relay state");
    return ESP_OK;
}

esp_err_t relay_toggle(relay_t *relay) {
    // error management
    esp_err_t ret;

    // sanity check
    if (relay == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // update state
    uint32_t new_state = !relay->current_state;
    ret = gpio_set_level(relay->pin, new_state);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set GPIO %d", (int)relay->pin);
        return ret;
    }

    // update relay structure
    relay->current_state = new_state;
    return ESP_OK;
}

esp_err_t relay_read(relay_t *relay, uint32_t *state) {
    // sanity check
    if (relay == NULL || state == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    *state = relay->current_state;
    return ESP_OK;
}
