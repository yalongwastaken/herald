/**
 * @file wifi.c
 * @author Anthony Yalong
 * @brief Wi-Fi initialization and connection management.
 */

// ── Includes ──────────────────────────────────────────────────────────────────
#include <string.h>
#include "wifi.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// ── Configuration ─────────────────────────────────────────────────────────────
#define WIFI_MAX_RETRY          5
#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT           BIT1

static const char *TAG = "wifi";
static int s_retry_count = 0;
static EventGroupHandle_t s_wifi_event_group = NULL;

// ── Private Function Prototypes ───────────────────────────────────────────────

/**
 * @brief Wi-Fi and IP event handler.
 *
 * @param arg         User argument passed during handler registration
 * @param event_base  Event base (WIFI_EVENT or IP_EVENT)
 * @param event_id    Specific event ID
 * @param event_data  Pointer to event-specific data
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

// ── Private API ───────────────────────────────────────────────────────────────

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_count++;
            ESP_LOGI(TAG, "retrying Wi-Fi connection (%d/%d)...",
                     s_retry_count, WIFI_MAX_RETRY);
        }
        else {
            ESP_LOGE(TAG, "failed to connect after %d attempts", WIFI_MAX_RETRY);
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        if (event_data == NULL) {
            ESP_LOGE(TAG, "event_data is null");
            return;
        }

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        s_retry_count = 0;
        ESP_LOGI(TAG, "connected! ip: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

esp_err_t wifi_init(const char *ssid, const char *password) {
    esp_err_t ret;

    if (ssid == NULL || password == NULL) {
        ESP_LOGE(TAG, "invalid argument");
        return ESP_ERR_INVALID_ARG;
    }

    // initialize nvs
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "nvs erase failed");
            return ret;
        }
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs init failed");
        return ret;
    }

    // create event group before starting wifi
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    // network interface init
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "netif init failed");
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event loop create failed");
        return ret;
    }

    if (esp_netif_create_default_wifi_sta() == NULL) {
        ESP_LOGE(TAG, "failed to create default wifi sta");
        return ESP_FAIL;
    }

    // wifi driver init
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "wifi init failed");
        return ret;
    }

    // register event handlers
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              &wifi_event_handler, NULL,
                                              &instance_any_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to register wifi event handler");
        return ret;
    }

    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              &wifi_event_handler, NULL,
                                              &instance_got_ip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to register ip event handler");
        return ret;
    }

    // set ssid and password
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set wifi mode");
        return ret;
    }

    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set wifi config");
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to start wifi");
        return ret;
    }

    // block until connected or failed
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,   // don't clear bits
                                           pdFALSE,   // wait for either bit
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        return ESP_OK;
    }

    ESP_LOGE(TAG, "wifi connection failed");
    return ESP_FAIL;
}