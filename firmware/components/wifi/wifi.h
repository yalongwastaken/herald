/**
 * @file wifi.h
 * @author Anthony Yalong
 * @brief Wi-Fi initialization interface.
 */
#ifndef WIFI_H
#define WIFI_H

// ── Includes ──────────────────────────────────────────────────────────────────
#include "esp_err.h"

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * @brief Initialize and connect to a Wi-Fi network.
 *
 * @param ssid        Null-terminated string containing Wi-Fi network SSID
 * @param password    Null-terminated string containing Wi-Fi password
 * @return            ESP_OK on success, ESP_ERR_INVALID_ARG if any pointer is NULL
 */
esp_err_t wifi_init(const char *ssid, const char *password);

#endif // WIFI_H