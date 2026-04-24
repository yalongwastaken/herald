/**
 * @file hcsr04.c
 * @author Anthony Yalong
 * @brief HC-SR04 ultrasonic distance sensor driver implementation.
 */

// ── Includes ──────────────────────────────────────────────────────────────────
#include "hcsr04.h"
#include "esp_rom_sys.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"

// ── Configuration ─────────────────────────────────────────────────────────────
static const char *TAG = "hcsr04";

// ── Public API ────────────────────────────────────────────────────────────────

esp_err_t hcsr04_init(gpio_num_t trig_pin, gpio_num_t echo_pin, uint32_t timeout, hcsr04_sensor_t *sensor) {
    esp_err_t ret;

    // sanity check
    if (sensor == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // configure trig pin
    gpio_config_t trig_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << trig_pin),
        .intr_type = GPIO_INTR_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    ret = gpio_config(&trig_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure trig gpio");
        return ret;
    }

    // configure echo pin
    gpio_config_t echo_config = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << echo_pin),
        .intr_type = GPIO_INTR_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    ret = gpio_config(&echo_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure echo gpio");
        return ret;
    }

    // ensure trig is low
    ret = gpio_set_level(trig_pin, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set trig gpio low");
        return ret;
    }

    // initialize sensor structure
    sensor->trig_pin = trig_pin;
    sensor->echo_pin = echo_pin;
    sensor->last_distance_cm = 0.0f;
    sensor->timeout_us = timeout;

    ESP_LOGD(TAG, "initialized on trig gpio %d, echo gpio %d", (int)trig_pin, (int)echo_pin);
    return ESP_OK;
}

esp_err_t hcsr04_measure(hcsr04_sensor_t *sensor) {
    // sanity check
    if (sensor == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // send 10us trigger pulse
    gpio_set_level(sensor->trig_pin, 0);
    esp_rom_delay_us(2);
    gpio_set_level(sensor->trig_pin, 1);
    esp_rom_delay_us(10);
    gpio_set_level(sensor->trig_pin, 0);

    // wait for echo high
    int64_t start_time = esp_timer_get_time();
    while (gpio_get_level(sensor->echo_pin) == 0) {
        if ((esp_timer_get_time() - start_time) > sensor->timeout_us) {
            ESP_LOGE(TAG, "timeout waiting for echo high");
            return ESP_ERR_TIMEOUT;
        }
    }

    // record echo start
    int64_t echo_start = esp_timer_get_time();

    // wait for echo low
    while (gpio_get_level(sensor->echo_pin) == 1) {
        if ((esp_timer_get_time() - echo_start) > sensor->timeout_us) {
            ESP_LOGE(TAG, "timeout waiting for echo low");
            return ESP_ERR_TIMEOUT;
        }
    }

    // calculate distance: speed of sound = 343 m/s = 0.0343 cm/us
    int64_t pulse_width = esp_timer_get_time() - echo_start;
    sensor->last_distance_cm = (pulse_width * 0.0343f) / 2.0f;

    ESP_LOGD(TAG, "distance: %.2f cm (pulse: %lld us)", sensor->last_distance_cm, pulse_width);
    return ESP_OK;
}

esp_err_t hcsr04_get_distance(float *distance, hcsr04_sensor_t *sensor) {
    // sanity check
    if (sensor == NULL || distance == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    *distance = sensor->last_distance_cm;

    ESP_LOGD(TAG, "distance read: %.2f cm", *distance);
    return ESP_OK;
}