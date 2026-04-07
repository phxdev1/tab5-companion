idf_component_register(
    SRCS "main.cpp"
         "hal/display.cpp"
         "hal/ble.cpp"
         "hal/touch.cpp"
         "hal/audio.cpp"
         "cmd/cmd.cpp"
         "ui/standby.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash bt json
)
