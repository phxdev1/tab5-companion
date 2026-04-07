idf_component_register(
    SRCS "main.cpp"
         "hal/display.cpp"
         "hal/ble.cpp"
         "hal/touch.cpp"
         "hal/audio.cpp"
         "hal/wifi.cpp"
         "hal/http.cpp"
         "hal/kvstore.cpp"
         "cmd/cmd.cpp"
         "ui/standby.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash bt json esp_http_server esp_wifi esp_netif esp_timer
)
