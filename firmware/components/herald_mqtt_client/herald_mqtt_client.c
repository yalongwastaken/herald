/**
 * @file herald_mqtt_client.c
 * @author Anthony Yalong
 * @brief MQTT client implementation for herald actuator nodes.
 */

// ── Includes ──────────────────────────────────────────────────────────────────
#include <string.h>
#include "esp_log.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "herald_mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// ── Configuration ─────────────────────────────────────────────────────────────
#define MQTT_CONNECTED_BIT      BIT0
#define MQTT_FAIL_BIT           BIT1

static const char *TAG = "mqtt";
static esp_mqtt_client_handle_t s_client = NULL;
static EventGroupHandle_t s_mqtt_event_group = NULL;

extern void mqtt_on_data(const char *payload, int len);

// ── Private API ───────────────────────────────────────────────────────────────

/**
 * @brief MQTT event handler.
 *
 * Handles connected, disconnected, and data events. On connect, subscribes
 * to command and poll topics. On data, dispatches to mqtt_on_data.
 *
 * @param handler_args   User data registered with the event
 * @param base           Event base
 * @param event_id       Event ID
 * @param event_data     Pointer to esp_mqtt_event_t
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            esp_mqtt_client_subscribe(s_client, MQTT_TOPIC_CMD, 0);
            esp_mqtt_client_subscribe(s_client, MQTT_TOPIC_POLL, 1);
            xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
            break;

        case MQTT_EVENT_DISCONNECTED:
            xEventGroupSetBits(s_mqtt_event_group, MQTT_FAIL_BIT);
            break;

        case MQTT_EVENT_DATA:
            mqtt_on_data(event->data, event->data_len);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "mqtt error");
            break;

        default:
            break;
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

esp_err_t mqtt_client_init(void) {
    s_mqtt_event_group = xEventGroupCreate();
    if (s_mqtt_event_group == NULL) {
        ESP_LOGE(TAG, "failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.client_id = MQTT_CLIENT_ID,
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "failed to init mqtt client");
        return ESP_FAIL;
    }

    esp_err_t ret = esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to register event handler");
        return ret;
    }

    ret = esp_mqtt_client_start(s_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to start mqtt client");
        return ret;
    }

    // block until connected or failed
    EventBits_t bits = xEventGroupWaitBits(s_mqtt_event_group,
                                           MQTT_CONNECTED_BIT | MQTT_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & MQTT_CONNECTED_BIT) {
        ESP_LOGD(TAG, "mqtt connected");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "mqtt connection failed");
    return ESP_FAIL;
}

esp_err_t mqtt_publish(const char *topic, const char *payload) {
    if (topic == NULL || payload == NULL) {
        ESP_LOGE(TAG, "invalid argument");
        return ESP_ERR_INVALID_ARG;
    }

    int msg_id = esp_mqtt_client_publish(s_client, topic, payload, 0, 0, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "publish failed on topic: %s", topic);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "published to %s: %s", topic, payload);
    return ESP_OK;
}