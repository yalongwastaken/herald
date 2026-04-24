/**
 * @file main.c
 * @author Anthony Yalong
 * @brief Herald Node 1 entry point. Initializes peripherals, connects to Wi-Fi
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
#include "relay.h"
#include "dht11.h"
#include "oled.h"

// ── Configuration ─────────────────────────────────────────────────────────────
#define WIFI_SSID               "herald"        // PLACEHOLDER
#define WIFI_PASSWORD           "herald123"     // PLACEHOLDER

#define CMD_QUEUE_DEPTH         8
#define CMD_PAYLOAD_MAX_LEN     256

#define PIN_OLED_SDA            GPIO_NUM_21
#define PIN_OLED_SCL            GPIO_NUM_22
#define PIN_DHT11               GPIO_NUM_4
#define PIN_BUZZER              GPIO_NUM_18
#define PIN_SERVO               GPIO_NUM_19
#define PIN_RELAY               GPIO_NUM_14

#define I2C_MASTER_FREQ_HZ      400000
#define OLED_I2C_ADDR           0x3C

static const char *TAG = "node1";

static QueueHandle_t s_cmd_queue = NULL;

static buzzer_t s_buzzer;
static servo_t  s_servo;
static relay_t  s_relay;
static dht11_t  s_dht11;
static oled_t   s_oled;

// ── Types ─────────────────────────────────────────────────────────────────────

/** @brief Command message passed through the FreeRTOS queue. */
typedef struct {
    char payload[CMD_PAYLOAD_MAX_LEN];  // raw JSON string from MQTT
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
    else if (strcmp(tool_name, "set_relay") == 0) {
        if (args == NULL) { ESP_LOGE(TAG, "missing arguments for set_relay"); cJSON_Delete(root); return; }
        cJSON *state = cJSON_GetObjectItem(args, "state");
        if (cJSON_IsBool(state)) {
            relay_set(cJSON_IsTrue(state), &s_relay);
        } else {
            ESP_LOGE(TAG, "invalid or missing state for set_relay");
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
        oled_clear(&s_oled);
        oled_write_string(&s_oled, 0, 0, message->valuestring, &oled_font_5x8);
        oled_flush(&s_oled);
    }
    else if (strcmp(tool_name, "get_temp_humidity") == 0) {
        esp_err_t ret = dht11_update(&s_dht11);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "dht11 update failed");
            cJSON_Delete(root);
            return;
        }

        float temperature, humidity;
        ret = dht11_read(&s_dht11, &temperature, &humidity);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "dht11 read failed");
            cJSON_Delete(root);
            return;
        }

        char buf[64];
        snprintf(buf, sizeof(buf), "{\"temperature\":%.1f,\"humidity\":%.1f}", temperature, humidity);
        mqtt_publish(MQTT_TOPIC_DATA, buf);
    }
    else if (strcmp(tool_name, "stop") == 0) {
        ESP_LOGW(TAG, "stop received — halting all actuators");
        relay_set(false, &s_relay);
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

    // initialize i2c bus
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_OLED_SDA,
        .scl_io_num = PIN_OLED_SCL,
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

    // initialize oled i2c device
    i2c_device_config_t oled_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = OLED_I2C_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    i2c_master_dev_handle_t oled_dev = NULL;
    ret = i2c_master_bus_add_device(i2c_bus, &oled_dev_cfg, &oled_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "oled i2c device init failed — halting");
        return;
    }

    // initialize peripherals
    ret = oled_init(oled_dev, &s_oled);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "oled init failed — halting");
        return;
    }

    ret = dht11_init(PIN_DHT11, &s_dht11);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "dht11 init failed — halting");
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

    ret = relay_init(PIN_RELAY, &s_relay);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "relay init failed — halting");
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

    ESP_LOGI(TAG, "herald node1 ready");
}