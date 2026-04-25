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
#define WIFI_SSID               "herald"
#define WIFI_PASSWORD           "herald1234"

#define CMD_QUEUE_DEPTH         8
#define CMD_PAYLOAD_MAX_LEN     256

#define PIN_OLED_SDA            GPIO_NUM_21
#define PIN_OLED_SCL            GPIO_NUM_22
#define PIN_DHT11               GPIO_NUM_5
#define PIN_BUZZER              GPIO_NUM_18
#define PIN_SERVO               GPIO_NUM_19
#define PIN_RELAY               GPIO_NUM_14

#define I2C_MASTER_FREQ_HZ      400000
#define OLED_I2C_ADDR           0x3C

// ── OLED layout (5x8 font: 21 chars wide, 8 rows) ────────────────────────────
// Row 0 (y=0)  — "Herald Node 1"       static header
// Row 1 (y=8)  — "WiFi:-- MQTT:--"     connection status
// Row 2 (y=16) — "--------------------" separator
// Row 3 (y=24) — "Tool: <tool_name>"   last dispatched tool
// Row 4 (y=32) — "Args: <args>"        last dispatched args
// Row 5 (y=40) — "--------------------" separator
// Row 6 (y=48) — "T:--.-C  H:--.-%"   last DHT11 reading
// Row 7 (y=56) — spare

#define OLED_ROW_HEADER         0
#define OLED_ROW_STATUS         8
#define OLED_ROW_SEP1           16
#define OLED_ROW_TOOL           24
#define OLED_ROW_ARGS           32
#define OLED_ROW_SEP2           40
#define OLED_ROW_SENSOR         48

#define OLED_SEP                "---------------------"

static const char *TAG = "node1";

static QueueHandle_t s_cmd_queue = NULL;

static buzzer_t s_buzzer;
static servo_t  s_servo;
static relay_t  s_relay;
static dht11_t  s_dht11;
static oled_t   s_oled;

// ── Connection state (updated by wifi/mqtt callbacks) ─────────────────────────
static bool s_wifi_ok  = false;
static bool s_mqtt_ok  = false;

// ── Types ─────────────────────────────────────────────────────────────────────

/** @brief Command message passed through the FreeRTOS queue. */
typedef struct {
    char payload[CMD_PAYLOAD_MAX_LEN];  // raw JSON string from MQTT
} cmd_msg_t;

// ── OLED helpers ──────────────────────────────────────────────────────────────

/**
 * @brief Write a single row, padding with spaces to clear leftover characters.
 *
 * @param y     Y coordinate (pixel row)
 * @param str   String to write (max 21 chars)
 */
static void oled_write_row(uint8_t y, const char *str) {
    char buf[22];
    snprintf(buf, sizeof(buf), "%-21s", str);
    oled_write_string(&s_oled, 0, y, buf, &oled_font_5x8);
}

/**
 * @brief Redraw the status row from current s_wifi_ok / s_mqtt_ok state.
 */
static void oled_update_status(void) {
    char buf[22];
    snprintf(buf, sizeof(buf), "WiFi:%-2s MQTT:%-2s",
             s_wifi_ok ? "OK" : "--",
             s_mqtt_ok ? "OK" : "--");
    oled_write_row(OLED_ROW_STATUS, buf);
    oled_flush(&s_oled);
}

/**
 * @brief Redraw the command rows (tool + args).
 *
 * @param tool  Tool name string
 * @param args  Args string (caller formats this)
 */
static void oled_update_command(const char *tool, const char *args) {
    char buf[22];
    snprintf(buf, sizeof(buf), "Tool: %s", tool);
    oled_write_row(OLED_ROW_TOOL, buf);
    snprintf(buf, sizeof(buf), "Args: %s", args);
    oled_write_row(OLED_ROW_ARGS, buf);
    oled_flush(&s_oled);
}

/**
 * @brief Redraw the sensor row with latest temperature and humidity.
 *
 * @param temperature   Temperature in Celsius
 * @param humidity      Relative humidity in percent
 */
static void oled_update_sensor(float temperature, float humidity) {
    char buf[22];
    snprintf(buf, sizeof(buf), "T:%.1fC  H:%.0f%%", temperature, humidity);
    oled_write_row(OLED_ROW_SENSOR, buf);
    oled_flush(&s_oled);
}

/**
 * @brief Draw the static portions of the display (header + separators).
 *        Called once on boot after oled_init.
 */
static void oled_draw_static(void) {
    oled_clear(&s_oled);
    oled_write_row(OLED_ROW_HEADER, "Herald Node 1");
    oled_write_row(OLED_ROW_STATUS, "WiFi:-- MQTT:--");
    oled_write_row(OLED_ROW_SEP1,   OLED_SEP);
    oled_write_row(OLED_ROW_TOOL,   "Tool: --");
    oled_write_row(OLED_ROW_ARGS,   "Args: --");
    oled_write_row(OLED_ROW_SEP2,   OLED_SEP);
    oled_write_row(OLED_ROW_SENSOR, "T:--.-C  H:---%");
    oled_flush(&s_oled);
}

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
            char args_buf[16];
            snprintf(args_buf, sizeof(args_buf), "%dms", duration->valueint);
            oled_update_command(tool_name, args_buf);
            buzzer_buzz(&s_buzzer, (uint32_t)duration->valueint * 1000);
        } else {
            ESP_LOGE(TAG, "invalid or missing duration_ms for buzz");
        }
    }
    else if (strcmp(tool_name, "move_servo") == 0) {
        if (args == NULL) { ESP_LOGE(TAG, "missing arguments for move_servo"); cJSON_Delete(root); return; }
        cJSON *angle = cJSON_GetObjectItem(args, "angle");
        if (cJSON_IsNumber(angle)) {
            char args_buf[16];
            snprintf(args_buf, sizeof(args_buf), "%ddeg", angle->valueint);
            oled_update_command(tool_name, args_buf);
            servo_set_angle((uint32_t)angle->valueint, &s_servo);
        } else {
            ESP_LOGE(TAG, "invalid or missing angle for move_servo");
        }
    }
    else if (strcmp(tool_name, "set_relay") == 0) {
        if (args == NULL) { ESP_LOGE(TAG, "missing arguments for set_relay"); cJSON_Delete(root); return; }
        cJSON *state = cJSON_GetObjectItem(args, "state");
        if (cJSON_IsBool(state)) {
            bool on = cJSON_IsTrue(state);
            oled_update_command(tool_name, on ? "on" : "off");
            relay_set(on, &s_relay);
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
        // set_display takes over the full screen — redraw static frame after
        oled_clear(&s_oled);
        oled_write_string(&s_oled, 0, 0, message->valuestring, &oled_font_5x8);
        oled_flush(&s_oled);
        vTaskDelay(pdMS_TO_TICKS(3000));
        oled_draw_static();
        oled_update_status();
    }
    else if (strcmp(tool_name, "get_temp_humidity") == 0) {
        oled_update_command(tool_name, "reading...");

        esp_err_t ret = dht11_update(&s_dht11);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "dht11 update failed");
            oled_update_command(tool_name, "read failed");
            cJSON_Delete(root);
            return;
        }

        float temperature, humidity;
        ret = dht11_read(&s_dht11, &temperature, &humidity);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "dht11 read failed");
            oled_update_command(tool_name, "read failed");
            cJSON_Delete(root);
            return;
        }

        oled_update_sensor(temperature, humidity);
        oled_update_command(tool_name, "done");

        char buf[64];
        snprintf(buf, sizeof(buf), "{\"temperature\":%.1f,\"humidity\":%.1f}", temperature, humidity);
        mqtt_publish(MQTT_TOPIC_DATA, buf);
    }
    else if (strcmp(tool_name, "stop") == 0) {
        ESP_LOGW(TAG, "stop received — halting all actuators");
        oled_update_command(tool_name, "all stopped");
        relay_set(false, &s_relay);
        servo_set_angle(90, &s_servo);
        buzzer_stop(&s_buzzer);
    }
    else {
        ESP_LOGW(TAG, "unknown tool: %s", tool_name);
        oled_update_command(tool_name, "unknown");
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

/**
 * @brief Called by wifi component on connection state change.
 *
 * @param connected     true if connected, false if disconnected
 */
void wifi_on_state_change(bool connected) {
    s_wifi_ok = connected;
    oled_update_status();
}

/**
 * @brief Called by herald_mqtt_client on connection state change.
 *
 * @param connected     true if connected, false if disconnected
 */
void mqtt_on_state_change(bool connected) {
    s_mqtt_ok = connected;
    oled_update_status();
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

    // draw static layout immediately — visible during rest of init
    oled_draw_static();

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

    // connect to wi-fi (status row updates via wifi_on_state_change callback)
    ret = wifi_init(WIFI_SSID, WIFI_PASSWORD);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "wifi init failed — halting");
        return;
    }

    // connect to mqtt broker (status row updates via mqtt_on_state_change callback)
    ret = mqtt_client_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mqtt init failed — halting");
        return;
    }

    // start command dispatcher task
    xTaskCreate(command_task, "command_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "herald node1 ready");
}