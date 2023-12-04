idf_component_register(SRCS "ble_tests_impl.c" "BLeTest.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver bt freertos nvs_flash
                    )
