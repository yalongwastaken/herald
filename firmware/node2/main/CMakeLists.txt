idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES hcsr04 lcd rgb_strip buzzer servo wifi herald_mqtt_client json
)