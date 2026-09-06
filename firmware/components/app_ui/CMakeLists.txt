idf_component_register(
    SRCS "main_screen.cpp" "font_zh_14.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl lvgl_bsp app_services freertos log
)
