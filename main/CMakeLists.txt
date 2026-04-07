idf_component_register(
    SRCS "main.cpp"
         "hal/display.cpp"
         "hal/ble.cpp"
         "cmd/cmd.cpp"
         "ui/standby.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash bt json
)
