idf_component_register(
    SRC_DIRS
        "coap/"
        "coap/er-coap-13/"
        "core/"
        "data/"

    PRIV_INCLUDE_DIRS
        "core"
        "coap/"

    INCLUDE_DIRS
        "include"
    )

target_compile_definitions(${COMPONENT_TARGET}
                    PUBLIC
                    "LWM2M_CLIENT_MODE"
                    "LWM2M_COAP_DEFAULT_BLOCK_SIZE=256"
                    "LWM2M_SUPPORT_SENML_JSON"
                    "LWM2M_WITH_LOGS"
                    )

set(USES_TERMINAL_DOWNLOAD TRUE)
set(USES_TERMINAL_CONFIGURE TRUE)
set(USES_TERMINAL_BUILD TRUE)
