/**
 * @file servo.h
 * @author Anthony Yalong
 * @brief SG90 servo motor component. MCPWM-driven absolute angle positioning for actuator nodes.
 */
#ifndef SERVO_H
#define SERVO_H

// ── Includes ──────────────────────────────────────────────────────────────────
#include "esp_err.h"
#include "soc/gpio_num.h"
#include "driver/mcpwm_prelude.h"

// ── Types ─────────────────────────────────────────────────────────────────────

/** @brief Servo device handle. Holds hardware configuration for a single servo instance. */
typedef struct {
    gpio_num_t pin;
    uint32_t current_angle;
    mcpwm_cmpr_handle_t comparator;
} servo_t;

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * @brief Initialize the servo GPIO pin and MCPWM resources.
 * 
 * @param pin       GPIO number the servo is connected to
 * @param servo     Pointer to servo handle to populate
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if servo is NULL or pin is invalid
 */
esp_err_t servo_init(gpio_num_t pin, servo_t *servo);

/**
 * @brief Set the servo to a specified angle.
 * 
 * @param angle     Target angle in degrees (typically 0–180 for SG90)
 * @param servo     Pointer to an initialized servo handle
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if servo is NULL or angle is invalid
 */
esp_err_t servo_set_angle(uint32_t angle, servo_t *servo);

/**
 * @brief Read the current servo angle.
 * 
 * @param servo     Pointer to an initialized servo handle
 * @param angle     Pointer to variable where the angle will be stored
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if servo or angle is NULL
 */
esp_err_t servo_read_angle(servo_t *servo, uint32_t *angle);

#endif // SERVO_H