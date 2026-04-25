/**
 * @file main.c
 * @author Anthony Yalong
 * @brief Herald Node 2 entry point. Initializes peripherals, connects to Wi-Fi
 *        and MQTT, and dispatches incoming commands via a FreeRTOS queue.
 */

// ── Includes ──────────────────────────────────────────────────────────────────
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "cJSON.h"

#include "wifi.h"
#include "herald_mqtt_client.h"
#include "buzzer.h"
#include "servo.h"
#include "rgb_strip.h"
#include "hcsr04.h"
#include "lcd.h"

// ── Configuration ─────────────────────────────────────────────────────────────
#define WIFI_SSID               "herald"
#define WIFI_PASSWORD           "herald1234"

#define CMD_QUEUE_DEPTH         8
#define CMD_PAYLOAD_MAX_LEN     256

#define NUM_LEDS                8
#define HCSR04_TIMEOUT_US       30000

#define PIN_BUZZER              GPIO_NUM_18
#define PIN_SERVO               GPIO_NUM_19
#define PIN_RGB_STRIP           GPIO_NUM_21
#define PIN_HCSR04_TRIG         GPIO_NUM_22
#define PIN_HCSR04_ECHO         GPIO_NUM_23
#define PIN_LCD_SDA             GPIO_NUM_26
#define PIN_LCD_SCL             GPIO_NUM_27

#define I2C_MASTER_FREQ_HZ      400000
#define LCD_I2C_ADDR            0x27
#define LCD_COLS                16
#define LCD_ROWS                2

static const char *TAG = "node2";

static QueueHandle_t   s_cmd_queue = NULL;
static buzzer_t        s_buzzer;
static servo_t         s_servo;
static rgb_strip_t     s_rgb_strip;
static hcsr04_sensor_t s_hcsr04;
static lcd_handle_t    s_lcd;

// ── Types ─────────────────────────────────────────────────────────────────────

/** @brief Command message passed through the FreeRTOS queue. */
typedef struct {
    char payload[CMD_PAYLOAD_MAX_LEN];
} cmd_msg_t;

// ── Private API ───────────────────────────────────────────────────────────────

/**
 * @brief Parse a JSON command payload and call the appropriate driver.
 *
 * Expected payload format:
 *   {"tool": "<tool_name>", "arguments": { ... }}
 *
 * @param payload   Null-terminated JSON string
 */
static void dispatch_command(const char *payload) {
    cJSON *root = cJSON_Parse(payload);
    if (root == NULL) {
        ESP_LOGE(TAG, "failed to parse JSON: %s", payload);
        return;
    }

    cJSON *tool = cJSON_GetObjectItem(root, "tool");
    cJSON *args = cJSON_GetObjectItem(root, "arguments");

    if (!cJSON_IsString(tool)) {
        ESP_LOGE(TAG, "missing or invalid 'tool' field");
        cJSON_Delete(root);
        return;
    }

    const char *tool_name = tool->valuestring;
    ESP_LOGI(TAG, "dispatching tool: %s", tool_name);

    if (strcmp(tool_name, "buzz") == 0) {
        if (args == NULL) { ESP_LOGE(TAG, "missing arguments for buzz"); cJSON_Delete(root); return; }
        cJSON *duration = cJSON_GetObjectItem(args, "duration_ms");
        if (cJSON_IsNumber(duration) && duration->valueint > 0) {
            buzzer_buzz(&s_buzzer, (uint32_t)duration->valueint * 1000);
        } else {
            ESP_LOGE(TAG, "invalid or missing duration_ms for buzz");
        }
    }
    else if (strcmp(tool_name, "move_servo") == 0) {
        if (args == NULL) { ESP_LOGE(TAG, "missing arguments for move_servo"); cJSON_Delete(root); return; }
        cJSON *angle = cJSON_GetObjectItem(args, "angle");
        if (cJSON_IsNumber(angle)) {
            servo_set_angle((uint32_t)angle->valueint, &s_servo);
        } else {
            ESP_LOGE(TAG, "invalid or missing angle for move_servo");
        }
    }
    else if (strcmp(tool_name, "set_rgb_strip") == 0) {
        if (args == NULL) { ESP_LOGE(TAG, "missing arguments for set_rgb_strip"); cJSON_Delete(root); return; }
        cJSON *r = cJSON_GetObjectItem(args, "r");
        cJSON *g = cJSON_GetObjectItem(args, "g");
        cJSON *b = cJSON_GetObjectItem(args, "b");
        if (cJSON_IsNumber(r) && cJSON_IsNumber(g) && cJSON_IsNumber(b)) {
            // clamp to 0-255
            uint8_t rv = (uint8_t)(r->valueint < 0 ? 0 : r->valueint > 255 ? 255 : r->valueint);
            uint8_t gv = (uint8_t)(g->valueint < 0 ? 0 : g->valueint > 255 ? 255 : g->valueint);
            uint8_t bv = (uint8_t)(b->valueint < 0 ? 0 : b->valueint > 255 ? 255 : b->valueint);
            rgb_strip_set_color(rv, gv, bv, &s_rgb_strip);
        } else {
            ESP_LOGE(TAG, "invalid or missing r/g/b for set_rgb_strip");
        }
    }
    else if (strcmp(tool_name, "set_display") == 0) {
        if (args == NULL) { ESP_LOGE(TAG, "missing arguments for set_display"); cJSON_Delete(root); return; }
        cJSON *message = cJSON_GetObjectItem(args, "message");
        if (!cJSON_IsString(message)) {
            ESP_LOGE(TAG, "invalid or missing message for set_display");
            cJSON_Delete(root);
            return;
        }
        lcd_clear(&s_lcd);
        lcd_set_cursor(0, 0, &s_lcd);
        lcd_print(message->valuestring, &s_lcd);
    }
    else if (strcmp(tool_name, "get_distance") == 0) {
        esp_err_t ret = hcsr04_measure(&s_hcsr04);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "hcsr04 measure failed");
            cJSON_Delete(root);
            return;
        }

        float distance;
        ret = hcsr04_get_distance(&distance, &s_hcsr04);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "hcsr04 get distance failed");
            cJSON_Delete(root);
            return;
        }

        char buf[64];
        snprintf(buf, sizeof(buf), "{\"distance\":%.2f}", distance);
        mqtt_publish(MQTT_TOPIC_DATA, buf);
    }
    else if (strcmp(tool_name, "stop") == 0) {
        ESP_LOGW(TAG, "stop received — halting all actuators");
        rgb_strip_clear(&s_rgb_strip);
        servo_set_angle(90, &s_servo);
        buzzer_stop(&s_buzzer);
    }
    else {
        ESP_LOGW(TAG, "unknown tool: %s", tool_name);
    }

    cJSON_Delete(root);
}

/**
 * @brief FreeRTOS task that drains the command queue and dispatches each command.
 *
 * @param arg   Unused task argument
 */
static void command_task(void *arg) {
    cmd_msg_t msg;

    while (1) {
        if (xQueueReceive(s_cmd_queue, &msg, portMAX_DELAY) == pdTRUE) {
            dispatch_command(msg.payload);
        }
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * @brief Called by herald_mqtt_client when a message arrives on a subscribed topic.
 *        Enqueues the raw JSON payload for processing by command_task.
 *
 * @param payload   Null-terminated JSON string
 * @param len       Length of payload
 */
void mqtt_on_data(const char *payload, int len) {
    if (s_cmd_queue == NULL || payload == NULL || len <= 0) {
        return;
    }

    cmd_msg_t msg = {0};
    int copy_len = len < CMD_PAYLOAD_MAX_LEN - 1 ? len : CMD_PAYLOAD_MAX_LEN - 1;
    memcpy(msg.payload, payload, copy_len);

    if (xQueueSend(s_cmd_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "command queue full, dropping message");
    }
}

void app_main(void) {
    esp_err_t ret;

    // initialize i2c bus for lcd
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_LCD_SDA,
        .scl_io_num = PIN_LCD_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t i2c_bus = NULL;
    ret = i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c bus init failed — halting");
        return;
    }

    // initialize lcd i2c device
    i2c_device_config_t lcd_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = LCD_I2C_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    i2c_master_dev_handle_t lcd_dev = NULL;
    ret = i2c_master_bus_add_device(i2c_bus, &lcd_dev_cfg, &lcd_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "lcd i2c device init failed — halting");
        return;
    }

    // initialize peripherals
    ret = lcd_init(lcd_dev, LCD_COLS, LCD_ROWS, &s_lcd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "lcd init failed — halting");
        return;
    }

    ret = hcsr04_init(PIN_HCSR04_TRIG, PIN_HCSR04_ECHO, HCSR04_TIMEOUT_US, &s_hcsr04);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "hcsr04 init failed — halting");
        return;
    }

    ret = buzzer_init(PIN_BUZZER, &s_buzzer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "buzzer init failed — halting");
        return;
    }

    ret = servo_init(PIN_SERVO, &s_servo);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "servo init failed — halting");
        return;
    }

    ret = rgb_strip_init(PIN_RGB_STRIP, NUM_LEDS, &s_rgb_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "rgb strip init failed — halting");
        return;
    }

    // create command queue
    s_cmd_queue = xQueueCreate(CMD_QUEUE_DEPTH, sizeof(cmd_msg_t));
    if (s_cmd_queue == NULL) {
        ESP_LOGE(TAG, "failed to create command queue");
        return;
    }

    // connect to wi-fi
    ret = wifi_init(WIFI_SSID, WIFI_PASSWORD);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "wifi init failed — halting");
        return;
    }

    // connect to mqtt broker
    ret = mqtt_client_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mqtt init failed — halting");
        return;
    }

    // start command dispatcher task
    xTaskCreate(command_task, "command_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "herald node2 ready");
}