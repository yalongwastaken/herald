/**
 * @file test_main.c
 * @author Anthony Yalong
 * @brief Node 2 component tests. Runs sequentially on boot, logs PASS/FAIL per case.
 */

// ── Includes ──────────────────────────────────────────────────────────────────
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#include "buzzer.h"
#include "servo.h"
#include "rgb_strip.h"
#include "hcsr04.h"
#include "lcd.h"

// ── Configuration ─────────────────────────────────────────────────────────────
#define PIN_BUZZER              GPIO_NUM_18
#define PIN_SERVO               GPIO_NUM_19
#define PIN_RGB_STRIP           GPIO_NUM_21
#define PIN_HCSR04_TRIG         GPIO_NUM_22
#define PIN_HCSR04_ECHO         GPIO_NUM_23
#define PIN_LCD_SDA             GPIO_NUM_26
#define PIN_LCD_SCL             GPIO_NUM_27

#define HCSR04_TIMEOUT_US       30000
#define LCD_I2C_ADDR            0x27
#define LCD_COLS                16
#define LCD_ROWS                2
#define I2C_MASTER_FREQ_HZ      400000
#define NUM_LEDS                8

static const char *TAG = "test";

#define TEST_ASSERT(expr, name) do { \
    if (expr) { ESP_LOGI(TAG, "  PASS: %s", name); } \
    else      { ESP_LOGE(TAG, "  FAIL: %s", name); } \
} while(0)

// ── Private API ───────────────────────────────────────────────────────────────

static void test_buzzer(void) {
    ESP_LOGI(TAG, "── buzzer ───────────────────────────────────────────────────");
    buzzer_t buzzer;

    // init
    TEST_ASSERT(buzzer_init(PIN_BUZZER, &buzzer) == ESP_OK, "init");

    // buzz 500ms, wait for auto-stop via timer
    TEST_ASSERT(buzzer_buzz(&buzzer, 500000) == ESP_OK, "buzz 500ms");
    vTaskDelay(pdMS_TO_TICKS(600));

    // manual stop while active
    TEST_ASSERT(buzzer_buzz(&buzzer, 5000000) == ESP_OK, "buzz 5s");
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT(buzzer_stop(&buzzer) == ESP_OK, "manual stop");

    // stop when already idle
    TEST_ASSERT(buzzer_stop(&buzzer) == ESP_OK, "stop when idle");

    // null pointer handling
    TEST_ASSERT(buzzer_init(PIN_BUZZER, NULL) == ESP_ERR_INVALID_ARG, "init null");
    TEST_ASSERT(buzzer_buzz(NULL, 1000) == ESP_ERR_INVALID_ARG, "buzz null");
    TEST_ASSERT(buzzer_stop(NULL) == ESP_ERR_INVALID_ARG, "stop null");
}

static void test_servo(void) {
    ESP_LOGI(TAG, "── servo ────────────────────────────────────────────────────");
    servo_t servo;
    uint32_t angle;

    // init
    TEST_ASSERT(servo_init(PIN_SERVO, &servo) == ESP_OK, "init");

    // set and read 0 degrees
    TEST_ASSERT(servo_set_angle(0, &servo) == ESP_OK, "set 0");
    vTaskDelay(pdMS_TO_TICKS(500));
    TEST_ASSERT(servo_read_angle(&angle, &servo) == ESP_OK, "read after 0");
    TEST_ASSERT(angle == 0, "angle == 0");

    // set and read 90 degrees
    TEST_ASSERT(servo_set_angle(90, &servo) == ESP_OK, "set 90");
    vTaskDelay(pdMS_TO_TICKS(500));
    TEST_ASSERT(servo_read_angle(&angle, &servo) == ESP_OK, "read after 90");
    TEST_ASSERT(angle == 90, "angle == 90");

    // set and read 180 degrees
    TEST_ASSERT(servo_set_angle(180, &servo) == ESP_OK, "set 180");
    vTaskDelay(pdMS_TO_TICKS(500));
    TEST_ASSERT(servo_read_angle(&angle, &servo) == ESP_OK, "read after 180");
    TEST_ASSERT(angle == 180, "angle == 180");

    // clamp above 180 — driver clamps to 180, returns ESP_OK
    TEST_ASSERT(servo_set_angle(270, &servo) == ESP_OK, "set 270 (clamped)");
    TEST_ASSERT(servo_read_angle(&angle, &servo) == ESP_OK, "read after clamp");
    TEST_ASSERT(angle == 180, "angle clamped to 180");

    // return to neutral
    servo_set_angle(90, &servo);

    // null pointer handling
    TEST_ASSERT(servo_init(PIN_SERVO, NULL) == ESP_ERR_INVALID_ARG, "init null");
    TEST_ASSERT(servo_set_angle(90, NULL) == ESP_ERR_INVALID_ARG, "set null");
    TEST_ASSERT(servo_read_angle(NULL, &servo) == ESP_ERR_INVALID_ARG, "read null angle");
    TEST_ASSERT(servo_read_angle(&angle, NULL) == ESP_ERR_INVALID_ARG, "read null servo");
}

static void test_rgb_strip(void) {
    ESP_LOGI(TAG, "── rgb_strip ────────────────────────────────────────────────");
    rgb_strip_t strip;

    // init
    TEST_ASSERT(rgb_strip_init(PIN_RGB_STRIP, NUM_LEDS, &strip) == ESP_OK, "init");

    // red
    TEST_ASSERT(rgb_strip_set_color(255, 0, 0, &strip) == ESP_OK, "set red");
    vTaskDelay(pdMS_TO_TICKS(1000));

    // green
    TEST_ASSERT(rgb_strip_set_color(0, 255, 0, &strip) == ESP_OK, "set green");
    vTaskDelay(pdMS_TO_TICKS(1000));

    // blue
    TEST_ASSERT(rgb_strip_set_color(0, 0, 255, &strip) == ESP_OK, "set blue");
    vTaskDelay(pdMS_TO_TICKS(1000));

    // white
    TEST_ASSERT(rgb_strip_set_color(255, 255, 255, &strip) == ESP_OK, "set white");
    vTaskDelay(pdMS_TO_TICKS(1000));

    // clear
    TEST_ASSERT(rgb_strip_clear(&strip) == ESP_OK, "clear");
    vTaskDelay(pdMS_TO_TICKS(500));

    // null pointer handling
    TEST_ASSERT(rgb_strip_init(PIN_RGB_STRIP, NUM_LEDS, NULL) == ESP_ERR_INVALID_ARG, "init null");
    TEST_ASSERT(rgb_strip_set_color(255, 0, 0, NULL) == ESP_ERR_INVALID_ARG, "set null");
    TEST_ASSERT(rgb_strip_clear(NULL) == ESP_ERR_INVALID_ARG, "clear null");
}

static void test_hcsr04(void) {
    ESP_LOGI(TAG, "── hcsr04 ───────────────────────────────────────────────────");
    hcsr04_sensor_t sensor;
    float distance;

    // init
    TEST_ASSERT(hcsr04_init(PIN_HCSR04_TRIG, PIN_HCSR04_ECHO, HCSR04_TIMEOUT_US, &sensor) == ESP_OK, "init");

    // read cached distance before first measure — should be 0.0
    TEST_ASSERT(hcsr04_get_distance(&distance, &sensor) == ESP_OK, "get before measure");
    TEST_ASSERT(distance == 0.0f, "distance == 0.0 before measure");

    // measure — place object ~20cm in front of sensor
    ESP_LOGI(TAG, "  INFO: place object ~20cm in front of sensor");
    vTaskDelay(pdMS_TO_TICKS(1000));
    TEST_ASSERT(hcsr04_measure(&sensor) == ESP_OK, "measure");
    TEST_ASSERT(hcsr04_get_distance(&distance, &sensor) == ESP_OK, "get after measure");
    TEST_ASSERT(distance > 0.0f && distance < 400.0f, "distance in range");
    ESP_LOGI(TAG, "  INFO: distance=%.2f cm", distance);

    // three consecutive readings — verify stable
    for (int i = 0; i < 3; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
        TEST_ASSERT(hcsr04_measure(&sensor) == ESP_OK, "measure stable");
        hcsr04_get_distance(&distance, &sensor);
        ESP_LOGI(TAG, "  INFO: reading %d = %.2f cm", i + 1, distance);
    }

    // timeout — cover sensor so no echo returns
    ESP_LOGI(TAG, "  INFO: cover sensor for timeout test");
    vTaskDelay(pdMS_TO_TICKS(2000));
    TEST_ASSERT(hcsr04_measure(&sensor) == ESP_ERR_TIMEOUT, "timeout on blocked sensor");

    // null pointer handling
    TEST_ASSERT(hcsr04_init(PIN_HCSR04_TRIG, PIN_HCSR04_ECHO, HCSR04_TIMEOUT_US, NULL) == ESP_ERR_INVALID_ARG, "init null");
    TEST_ASSERT(hcsr04_measure(NULL) == ESP_ERR_INVALID_ARG, "measure null");
    TEST_ASSERT(hcsr04_get_distance(NULL, &sensor) == ESP_ERR_INVALID_ARG, "get null distance");
    TEST_ASSERT(hcsr04_get_distance(&distance, NULL) == ESP_ERR_INVALID_ARG, "get null sensor");
}

static void test_lcd(void) {
    ESP_LOGI(TAG, "── lcd ──────────────────────────────────────────────────────");
    lcd_handle_t lcd;

    // initialize i2c bus
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_LCD_SDA,
        .scl_io_num = PIN_LCD_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus = NULL;
    TEST_ASSERT(i2c_new_master_bus(&bus_cfg, &bus) == ESP_OK, "i2c bus init");

    // initialize lcd i2c device
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = LCD_I2C_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    i2c_master_dev_handle_t dev = NULL;
    TEST_ASSERT(i2c_master_bus_add_device(bus, &dev_cfg, &dev) == ESP_OK, "i2c device init");

    // lcd init
    TEST_ASSERT(lcd_init(dev, LCD_COLS, LCD_ROWS, &lcd) == ESP_OK, "init");

    // print two lines — visually verify on hardware
    TEST_ASSERT(lcd_set_cursor(0, 0, &lcd) == ESP_OK, "set cursor row 0");
    TEST_ASSERT(lcd_print("hello herald", &lcd) == ESP_OK, "print line 1");
    TEST_ASSERT(lcd_set_cursor(0, 1, &lcd) == ESP_OK, "set cursor row 1");
    TEST_ASSERT(lcd_print("node 2 test", &lcd) == ESP_OK, "print line 2");
    vTaskDelay(pdMS_TO_TICKS(2000));

    // clear
    TEST_ASSERT(lcd_clear(&lcd) == ESP_OK, "clear");
    vTaskDelay(pdMS_TO_TICKS(1000));

    // backlight off then on
    TEST_ASSERT(lcd_backlight(false, &lcd) == ESP_OK, "backlight off");
    vTaskDelay(pdMS_TO_TICKS(1000));
    TEST_ASSERT(lcd_backlight(true, &lcd) == ESP_OK, "backlight on");

    // out of bounds cursor — should return ESP_ERR_INVALID_ARG
    TEST_ASSERT(lcd_set_cursor(LCD_COLS, 0, &lcd) == ESP_ERR_INVALID_ARG, "cursor col oob");
    TEST_ASSERT(lcd_set_cursor(0, LCD_ROWS, &lcd) == ESP_ERR_INVALID_ARG, "cursor row oob");

    // null pointer handling
    TEST_ASSERT(lcd_init(dev, LCD_COLS, LCD_ROWS, NULL) == ESP_ERR_INVALID_ARG, "init null");
    TEST_ASSERT(lcd_clear(NULL) == ESP_ERR_INVALID_ARG, "clear null");
    TEST_ASSERT(lcd_set_cursor(0, 0, NULL) == ESP_ERR_INVALID_ARG, "cursor null");
    TEST_ASSERT(lcd_print("x", NULL) == ESP_ERR_INVALID_ARG, "print null lcd");
    TEST_ASSERT(lcd_print(NULL, &lcd) == ESP_ERR_INVALID_ARG, "print null str");
    TEST_ASSERT(lcd_backlight(true, NULL) == ESP_ERR_INVALID_ARG, "backlight null");
}

// ── Public API ────────────────────────────────────────────────────────────────

void app_main(void) {
    ESP_LOGI(TAG, "════ node2 component tests ════");

    test_buzzer();
    test_servo();
    test_rgb_strip();
    test_hcsr04();
    test_lcd();
    
    ESP_LOGI(TAG, "════ done ════");
}