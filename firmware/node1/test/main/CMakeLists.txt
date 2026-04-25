idf_component_register(
    SRCS "test_main.c"
    INCLUDE_DIRS "."
    REQUIRES dht11 oled relay buzzer servo
)