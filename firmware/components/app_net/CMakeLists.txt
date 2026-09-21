idf_component_register(
    SRCS "wifi_sta.cpp" "http_server.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_netif esp_event nvs_flash esp_http_server mdns spiffs app_update app_config app_services json log
)
