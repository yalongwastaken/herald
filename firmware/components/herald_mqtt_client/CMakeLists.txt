idf_component_register(
    SRCS "herald_mqtt_client.c"
    INCLUDE_DIRS "."
    REQUIRES esp_event mqtt
)