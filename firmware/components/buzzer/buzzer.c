/**
 * @file buzzer.c
 * @author Anthony Yalong
 * @brief Active buzzer component for herald actuator nodes.
 */

// ── Includes ──────────────────────────────────────────────────────────────────
#include "buzzer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "driver/gpio.h"

// ── Configuration ─────────────────────────────────────────────────────────────
static const char *TAG = "buzzer";

// ── Private API ───────────────────────────────────────────────────────────────

/**
 * @brief Timer callback to turn the buzzer OFF.
 *
 * @param args  GPIO number (passed as void*) identifying the buzzer pin
 */
static void buzzer_off_callback(void *args) {
    gpio_num_t pin = (gpio_num_t)(intptr_t)args;

    esp_err_t ret = gpio_set_level(pin, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set GPIO %d", (int)pin);
        return;
    }
    ESP_LOGD(TAG, "GPIO %d low", (int)pin);
}

// ── Public API ────────────────────────────────────────────────────────────────

esp_err_t buzzer_init(gpio_num_t pin, buzzer_t *buzzer) {
    // error management
    esp_err_t ret;

    // sanity check
    if (buzzer == NULL) {
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

    // callback timer
    esp_timer_handle_t esp_timer = NULL;
    esp_timer_create_args_t esp_timer_create_args = {
        .arg = (void *)(intptr_t)pin,
        .callback = buzzer_off_callback,
        .name = "buzzer_off",
    };
    ret = esp_timer_create(&esp_timer_create_args, &esp_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to create esp timer");
        return ret;
    }

    // structure initialization
    buzzer->pin = pin;
    buzzer->timer = esp_timer;
    ESP_LOGI(TAG, "initialized on GPIO %d", (int)pin);
    return ESP_OK;
}

esp_err_t buzzer_buzz(buzzer_t *buzzer, uint32_t duration_us) {
    // error management
    esp_err_t ret;

    // sanity check
    if (buzzer == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // set GPIO high (activate buzzer)
    ret = gpio_set_level(buzzer->pin, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set GPIO %d", (int)buzzer->pin);
        return ret;
    }
    ESP_LOGD(TAG, "GPIO %d high", (int)buzzer->pin);

    // ensure timer is not already running
    ret = esp_timer_stop(buzzer->timer);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "failed to stop callback timer");

        // rollback: turn buzzer OFF
        gpio_set_level(buzzer->pin, 0);
        ESP_LOGD(TAG, "GPIO %d low (rollback)", (int)buzzer->pin);

        return ret;
    }
    ESP_LOGD(TAG, "callback timer stopped/reset");

    // start timer
    ret = esp_timer_start_once(buzzer->timer, duration_us);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to start callback timer");

        // rollback: turn buzzer OFF
        gpio_set_level(buzzer->pin, 0);
        ESP_LOGD(TAG, "GPIO %d low (rollback)", (int)buzzer->pin);

        return ret;
    }
    ESP_LOGD(TAG, "callback timer started");

    return ESP_OK;
}