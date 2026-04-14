idf_component_register(
    SRCS "main.c" "mqtt_handler.c"
    INCLUDE_DIRS "."
    REQUIRES dht11 oled relay buzzer servo
)