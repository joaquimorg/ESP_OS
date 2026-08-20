#include "minios_hal.h"

#include "sdkconfig.h"
#include "esp_private/esp_gpio_reserve.h"
#include "soc/soc_caps.h"

#if SOC_USB_SERIAL_JTAG_SUPPORTED
#include "soc/io_mux_reg.h"
#endif

int minios_hal_init(void)
{
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG && SOC_USB_SERIAL_JTAG_SUPPORTED
    esp_gpio_reserve((UINT64_C(1) << USB_INT_PHY0_DM_GPIO_NUM) |
                     (UINT64_C(1) << USB_INT_PHY0_DP_GPIO_NUM));
#endif
    return MINIOS_HAL_OK;
}
