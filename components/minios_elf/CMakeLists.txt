if(CONFIG_MINIOS_ENABLE_ELF_LOADER)
    set(MINIOS_ELF_SOURCE "elf_loader.c")
else()
    set(MINIOS_ELF_SOURCE "elf_loader_stub.c")
endif()

idf_component_register(
    SRCS ${MINIOS_ELF_SOURCE}
    INCLUDE_DIRS "include"
    REQUIRES esp_hw_support heap minios_api minios_app minios_config minios_device minios_fs minios_net
)
