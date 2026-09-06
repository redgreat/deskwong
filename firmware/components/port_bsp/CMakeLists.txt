idf_component_register(
    SRCS "display_bsp.cpp" "i2c_bsp.cpp" "i2c_equipment.cpp" "adc_bsp.cpp" "button_bsp.c" "multi_button.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_lcd esp_adc esp_timer freertos heap log
)
