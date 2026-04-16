/**
 * @file servo.c
 * @author Anthony Yalong
 * @brief 
 */

// ── Includes ──────────────────────────────────────────────────────────────────
#include "esp_err.h"
#include "esp_log.h"
#include "servo.h"
#include "soc/gpio_num.h"
#include "driver/mcpwm_prelude.h"
#include "freertos/FreeRTOS.h"

// ── Configuration ─────────────────────────────────────────────────────────────
#define MINIMUM_PULSE_WIDTH_TICKS 1000    // 1ms
#define MAXIMUM_PULSE_WIDTH_TICKS 2000    // 2ms
static const char *TAG = "servo";

// ── Public API ────────────────────────────────────────────────────────────────

esp_err_t servo_init(gpio_num_t pin, servo_t *servo) {
    // error management
    esp_err_t ret;

    // mcpwm config
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000, // 1MHz, 1us per tick
        .period_ticks = 20000,    // 20ms period (50Hz)
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ret = mcpwm_new_timer(&timer_config, &timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed two configure mcpwm timer");
        return ret;
    }

    // mcpwm operator
    mcpwm_oper_handle_t operator = NULL;
    mcpwm_operator_config_t operator_config = {
        .group_id = 0,
    };
    ret = mcpwm_new_operator(&operator_config, &operator);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure mcpwm operator");
        return ret;
    }

    // mcpwm operator & timer connection
    ret = mcpwm_operator_connect_timer(operator, timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to connect mcpwm operator & mcpwm timer");
        return ret;
    }

    // mcpwm comparator
    mcpwm_cmpr_handle_t comparator = NULL;
    mcpwm_comparator_config_t comparator_config = {
        .flags = {
            .update_cmp_on_tez = true,
        },
    };
    ret = mcpwm_new_comparator(operator, &comparator_config, &comparator);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure mcpwm comperator");
        return ret;
    }

    // mcpwm generator
    mcpwm_gen_handle_t generator = NULL;
    mcpwm_generator_config_t generator_config = {
        .gen_gpio_num = pin,
    };
    ret = mcpwm_new_generator(operator, &generator_config, &generator);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure mcpwm generator");
        return ret;
    }

    // set generator output HIGH at the start of each PWM period
    ret = mcpwm_generator_set_action_on_timer_event(
        generator,
        MCPWM_GEN_TIMER_EVENT_ACTION(
            MCPWM_TIMER_DIRECTION_UP,
            MCPWM_TIMER_EVENT_EMPTY,
            MCPWM_GEN_ACTION_HIGH
        )
    );
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set generator HIGH on timer EMPTY event");
        return ret;
    }

    // set generator output LOW when timer reaches comparator value
    ret = mcpwm_generator_set_action_on_compare_event(
        generator,
        MCPWM_GEN_COMPARE_EVENT_ACTION(
            MCPWM_TIMER_DIRECTION_UP,
            comparator,
            MCPWM_GEN_ACTION_LOW
        )
    );
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set generator LOW on compare event");
        return ret;
    }

    // enable timer
    ret = mcpwm_timer_enable(timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to enable timer");
        return ret;
    }

    // define timer start & stop
    ret = mcpwm_timer_start_stop(
        timer,
        MCPWM_TIMER_START_NO_STOP
    );
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to enable timer start & stop");
        return ret;
    }

    // initialize servo structure
    servo->comparator = comparator;
    servo->current_angle = 0;
    servo->pin = pin;
    ESP_LOGI(TAG, "initialized on GPIO %d", (int)pin);

    return ESP_OK;
}

esp_err_t servo_set_angle(uint32_t angle, servo_t *servo) {
    // sanity check
    if (servo == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // clamp angle
    if (angle > 180) {
        angle = 180;
    }

    // convert angle to pulse width
    uint32_t pulse_width_ticks = MINIMUM_PULSE_WIDTH_TICKS + (angle * (MAXIMUM_PULSE_WIDTH_TICKS - MINIMUM_PULSE_WIDTH_TICKS)) / 180;

    // set pulse width
    esp_err_t ret = mcpwm_comparator_set_compare_value(servo->comparator, pulse_width_ticks);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set angle");
        return ret;
    }

    // update state
    servo->current_angle = angle;
    return ESP_OK;
}

esp_err_t servo_read_angle(servo_t *servo, uint32_t *angle) {
    // sanity check
    if (servo == NULL) {
        ESP_LOGE(TAG, "null structure pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // return
    *angle = servo->current_angle;
    ESP_LOGD(TAG, "successully read angle");
    return ESP_OK;
}