idf_component_register(
    SRCS "elf_loader.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_hw_support heap minios_api minios_app minios_config minios_device minios_fs minios_net
)
