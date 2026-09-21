idf_component_register(
    SRCS "main_screen.cpp" "sync_screen.cpp" "font_zh_10.c" "font_zh_14.c" "font_zh_20.c"
         "font_digits_18.c" "font_digits_28.c" "font_digits_36.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl lvgl_bsp app_services freertos log
)
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)
