idf_component_register(
    SRCS "test_main.c"
    INCLUDE_DIRS "."
    REQUIRES buzzer servo rgb_strip hcsr04 lcd esp_driver_i2c wifi herald_mqtt_client
)