set(srcs
    "esp_osc.c"
    "tinyosc.c"
)

idf_component_register(SRCS ${srcs}
    INCLUDE_DIRS "."
)
