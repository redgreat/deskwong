idf_component_register(
    SRCS "time_service.cpp" "sensor_service.cpp" "http_util.c"
         "weather_service.c" "worktime_service.c" "aiusage_service.c"
         "calendar_service.c" "reminder_service.c" "holiday_service.c"
         "app_mqtt.c" "racebox_service.c" "racebox_ble.c" "audio_service.c"
    INCLUDE_DIRS "."
    REQUIRES port_bsp codec_board esp_netif esp_timer esp_http_client mqtt bt json freertos log
)
