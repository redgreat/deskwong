idf_component_register(
    SRCS "lvgl_bsp.cpp"
    INCLUDE_DIRS "."
    REQUIRES lvgl freertos esp_timer log
)
