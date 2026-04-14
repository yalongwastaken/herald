idf_component_register(
    SRCS "main.c" "mqtt_handler.c"
    INCLUDE_DIRS "."
    REQUIRES hcsr04 lcd rgb_strip buzzer servo
)