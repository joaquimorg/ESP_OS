if(CONFIG_MINIOS_ENABLE_NETWORK)
    set(MINIOS_NET_SOURCE "network.c")
else()
    set(MINIOS_NET_SOURCE "network_stub.c")
endif()

idf_component_register(
    SRCS ${MINIOS_NET_SOURCE}
    INCLUDE_DIRS "include"
    REQUIRES minios_api minios_config esp_wifi esp_netif esp_event lwip
)
