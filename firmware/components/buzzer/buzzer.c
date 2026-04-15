/**
 * @file buzzer.c
 * @author Anthony Yalong
 * @brief Active buzzer component for herald actuator nodes.
 *        Provides initialization and timed activation of a GPIO-driven active buzzer.
 */

// ── Includes ──────────────────────────────────────────────────────────────────
#include "buzzer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ── Configuration ─────────────────────────────────────────────────────────────
const static char *TAG = "buzzer";

// ── Public API ────────────────────────────────────────────────────────────────

esp_err_t buzzer_init(gpio_num_t pin, buzzer_t *buzzer) {
    // error management
    esp_err_t ret;

    // saniuty check
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

    // structure initializaton
    buzzer->pin = pin;
    ESP_LOGI(TAG, "initialized on GPIO %d", (int)pin);
    return ESP_OK;
}

esp_err_t buzzer_buzz(buzzer_t *buzzer, uint32_t duration_ms) {
    // error management
    esp_err_t ret;

    // sanity check
    if (buzzer == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    ret = gpio_set_level(buzzer->pin, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set GPIO %d", (int)buzzer->pin);
        return ret;
    }
    ESP_LOGD(TAG, "GPIO %d high", (int)buzzer->pin);

    vTaskDelay(pdMS_TO_TICKS(duration_ms));

    ret = gpio_set_level(buzzer->pin, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set GPIO %d", (int)buzzer->pin);
        return ret;
    }
    ESP_LOGD(TAG, "GPIO %d low", (int)buzzer->pin);

    return ESP_OK;
}