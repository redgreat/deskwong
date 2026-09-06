idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES port_bsp lvgl_bsp app_config app_net app_services app_ui lvgl
)
