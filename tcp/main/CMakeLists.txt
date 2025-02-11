idf_component_register(SRCS "app_main.c" "ahtxx.c"
                    INCLUDE_DIRS "."
                     
                    )


# Ajoutez un dossier de composants supplémentaires si le compilateur ne trouve pas driver/i2c
# set(EXTRA_COMPONENT_DIRS "C:/Espressif/frameworks/esp-idf-v5.3.1/components")

# REQUIRES  esp_timer driver