/**
 * @file test_main.c
 * @author Anthony Yalong
 * @brief Node 1 component tests. Runs sequentially on boot, logs PASS/FAIL per case.
 */

// ── Includes ──────────────────────────────────────────────────────────────────
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#include "buzzer.h"
#include "servo.h"
#include "relay.h"
#include "dht11.h"
#include "oled.h"

// ── Configuration ─────────────────────────────────────────────────────────────
#define PIN_BUZZER              GPIO_NUM_18
#define PIN_SERVO               GPIO_NUM_19
#define PIN_RELAY               GPIO_NUM_14
#define PIN_DHT11               GPIO_NUM_5
#define PIN_OLED_SDA            GPIO_NUM_21
#define PIN_OLED_SCL            GPIO_NUM_22

#define OLED_I2C_ADDR           0x3C
#define I2C_MASTER_FREQ_HZ      400000

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

static void test_relay(void) {
    ESP_LOGI(TAG, "── relay ────────────────────────────────────────────────────");
    relay_t relay;
    uint32_t state;

    // init — relay starts LOW
    TEST_ASSERT(relay_init(PIN_RELAY, &relay) == ESP_OK, "init");
    TEST_ASSERT(relay_read(&relay, &state) == ESP_OK, "read after init");
    TEST_ASSERT(state == 0, "state == 0 after init");

    // set on
    TEST_ASSERT(relay_set(1, &relay) == ESP_OK, "set on");
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT(relay_read(&relay, &state) == ESP_OK, "read after set on");
    TEST_ASSERT(state == 1, "state == 1");

    // set off
    TEST_ASSERT(relay_set(0, &relay) == ESP_OK, "set off");
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT(relay_read(&relay, &state) == ESP_OK, "read after set off");
    TEST_ASSERT(state == 0, "state == 0");

    // toggle on
    TEST_ASSERT(relay_toggle(&relay) == ESP_OK, "toggle on");
    TEST_ASSERT(relay_read(&relay, &state) == ESP_OK, "read after toggle on");
    TEST_ASSERT(state == 1, "state == 1 after toggle");

    // toggle off
    TEST_ASSERT(relay_toggle(&relay) == ESP_OK, "toggle off");
    TEST_ASSERT(relay_read(&relay, &state) == ESP_OK, "read after toggle off");
    TEST_ASSERT(state == 0, "state == 0 after toggle");

    // null pointer handling
    TEST_ASSERT(relay_init(PIN_RELAY, NULL) == ESP_ERR_INVALID_ARG, "init null");
    TEST_ASSERT(relay_set(1, NULL) == ESP_ERR_INVALID_ARG, "set null");
    TEST_ASSERT(relay_toggle(NULL) == ESP_ERR_INVALID_ARG, "toggle null");
    TEST_ASSERT(relay_read(NULL, &state) == ESP_ERR_INVALID_ARG, "read null relay");
    TEST_ASSERT(relay_read(&relay, NULL) == ESP_ERR_INVALID_ARG, "read null state");
}

static void test_dht11(void) {
    ESP_LOGI(TAG, "── dht11 ────────────────────────────────────────────────────");
    dht11_t dht11;
    float temperature, humidity;

    // init
    TEST_ASSERT(dht11_init(PIN_DHT11, &dht11) == ESP_OK, "init");

    // read cached values before first update — should return -1.0
    TEST_ASSERT(dht11_read(&dht11, &temperature, &humidity) == ESP_OK, "read before update");
    TEST_ASSERT(temperature == -1.0f, "temperature == -1.0 before update");
    TEST_ASSERT(humidity == -1.0f, "humidity == -1.0 before update");

    // update — sensor needs 1s to stabilize after init, delay first
    vTaskDelay(pdMS_TO_TICKS(1500));
    TEST_ASSERT(dht11_update(&dht11) == ESP_OK, "update");

    // read updated values — sanity check ranges (DHT11: 0-50°C, 20-90% RH)
    TEST_ASSERT(dht11_read(&dht11, &temperature, &humidity) == ESP_OK, "read after update");
    TEST_ASSERT(temperature >= 0.0f && temperature <= 50.0f, "temperature in range");
    TEST_ASSERT(humidity >= 0.0f && humidity <= 100.0f, "humidity in range");
    ESP_LOGI(TAG, "  INFO: temperature=%.1f humidity=%.1f", temperature, humidity);

    // rate limit — second update immediately should return ESP_ERR_INVALID_STATE
    TEST_ASSERT(dht11_update(&dht11) == ESP_ERR_INVALID_STATE, "rate limit");

    // null pointer handling
    TEST_ASSERT(dht11_init(PIN_DHT11, NULL) == ESP_ERR_INVALID_ARG, "init null");
    TEST_ASSERT(dht11_update(NULL) == ESP_ERR_INVALID_ARG, "update null");
    TEST_ASSERT(dht11_read(NULL, &temperature, &humidity) == ESP_ERR_INVALID_ARG, "read null dht11");
    TEST_ASSERT(dht11_read(&dht11, NULL, &humidity) == ESP_ERR_INVALID_ARG, "read null temp");
    TEST_ASSERT(dht11_read(&dht11, &temperature, NULL) == ESP_ERR_INVALID_ARG, "read null humidity");
}

static void test_oled(void) {
    ESP_LOGI(TAG, "── oled ─────────────────────────────────────────────────────");
    oled_t oled;

    // initialize i2c bus
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_OLED_SDA,
        .scl_io_num = PIN_OLED_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus = NULL;
    TEST_ASSERT(i2c_new_master_bus(&bus_cfg, &bus) == ESP_OK, "i2c bus init");

    // initialize oled i2c device
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = OLED_I2C_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    i2c_master_dev_handle_t dev = NULL;
    TEST_ASSERT(i2c_master_bus_add_device(bus, &dev_cfg, &dev) == ESP_OK, "i2c device init");

    // oled init
    TEST_ASSERT(oled_init(dev, &oled) == ESP_OK, "init");

    // write string and flush — visually verify on hardware
    TEST_ASSERT(oled_clear(&oled) == ESP_OK, "clear");
    TEST_ASSERT(oled_write_string(&oled, 0, 0, "hello herald", &oled_font_5x8) == ESP_OK, "write line 1");
    TEST_ASSERT(oled_write_string(&oled, 0, 16, "node 1 test", &oled_font_5x8) == ESP_OK, "write line 2");
    TEST_ASSERT(oled_flush(&oled) == ESP_OK, "flush");
    vTaskDelay(pdMS_TO_TICKS(2000));

    // clear and flush — display should go blank
    TEST_ASSERT(oled_clear(&oled) == ESP_OK, "clear after write");
    TEST_ASSERT(oled_flush(&oled) == ESP_OK, "flush after clear");
    vTaskDelay(pdMS_TO_TICKS(1000));

    // out of bounds string — should not crash, just clip
    TEST_ASSERT(oled_write_string(&oled, 120, 56, "clip", &oled_font_5x8) == ESP_OK, "write near edge");
    TEST_ASSERT(oled_flush(&oled) == ESP_OK, "flush after edge write");

    // null pointer handling
    TEST_ASSERT(oled_init(dev, NULL) == ESP_ERR_INVALID_ARG, "init null");
    TEST_ASSERT(oled_clear(NULL) == ESP_ERR_INVALID_ARG, "clear null");
    TEST_ASSERT(oled_flush(NULL) == ESP_ERR_INVALID_ARG, "flush null");
    TEST_ASSERT(oled_write_string(NULL, 0, 0, "x", &oled_font_5x8) == ESP_ERR_INVALID_ARG, "write null oled");
    TEST_ASSERT(oled_write_string(&oled, 0, 0, NULL, &oled_font_5x8) == ESP_ERR_INVALID_ARG, "write null str");
    TEST_ASSERT(oled_write_string(&oled, 0, 0, "x", NULL) == ESP_ERR_INVALID_ARG, "write null font");
}

// ── Public API ────────────────────────────────────────────────────────────────

void app_main(void) {
    ESP_LOGI(TAG, "════ node1 component tests ════");

    test_buzzer();
    test_servo();
    test_relay();
    test_dht11();
    test_oled();

    ESP_LOGI(TAG, "════ done ════");
}