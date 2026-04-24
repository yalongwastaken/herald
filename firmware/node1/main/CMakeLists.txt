idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES dht11 oled relay buzzer servo wifi herald_mqtt_client json
)