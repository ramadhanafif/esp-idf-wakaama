idf_component_register(SRCS "udp_client.c"
                    INCLUDE_DIRS "."
                    # PRIV_REQUIRES "components"
                    # "esp_event" esp_netif nvs_flash
                    )


# set(EXTRA_COMPONENT_DIRS ${EXTRA_COMPONENT_DIRS} ${CMAKE_CURRENT_LIST_DIR}/components)
