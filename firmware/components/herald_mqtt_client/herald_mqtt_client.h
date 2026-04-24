/**
 * @file herald_mqtt_client.h
 * @author Anthony Yalong
 * @brief MQTT client interface for herald actuator nodes.
 */
#ifndef HERALD_MQTT_CLIENT_H
#define HERALD_MQTT_CLIENT_H

// ── Includes ──────────────────────────────────────────────────────────────────
#include "esp_err.h"

#define MQTT_BROKER_URI         "mqtt://192.168.4.1:1883"

#if defined(NODE1)
    #define MQTT_CLIENT_ID      "herald-node1"
    #define MQTT_TOPIC_CMD      "herald/cmd/node1"
    #define MQTT_TOPIC_POLL     "herald/poll/node1/temp_humidity"
    #define MQTT_TOPIC_ACK      "herald/ack/node1"
    #define MQTT_TOPIC_DATA     "herald/data/node1/temp_humidity"
#elif defined(NODE2)
    #define MQTT_CLIENT_ID      "herald-node2"
    #define MQTT_TOPIC_CMD      "herald/cmd/node2"
    #define MQTT_TOPIC_POLL     "herald/poll/node2/distance"
    #define MQTT_TOPIC_ACK      "herald/ack/node2"
    #define MQTT_TOPIC_DATA     "herald/data/node2/distance"
#else
    #error "No node defined. Set NODE1 or NODE2 in CMakeLists.txt."
#endif

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * @brief Initialize and start the MQTT client.
 *
 * Connects to the broker, subscribes to command and poll topics,
 * and registers the event handler. Blocks until connected.
 *
 * @return ESP_OK on success, ESP_FAIL if connection could not be established
 */
esp_err_t mqtt_client_init(void);

/**
 * @brief Publish a JSON payload to a topic.
 *
 * @param topic     Null-terminated topic string
 * @param payload   Null-terminated JSON string
 * @return          ESP_OK on success, ESP_ERR_INVALID_ARG if any pointer is NULL
 */
esp_err_t mqtt_publish(const char *topic, const char *payload);

#endif // HERALD_MQTT_CLIENT_H