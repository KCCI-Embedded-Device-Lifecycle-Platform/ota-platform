#ifndef EVSE_BOOT_HW__HW_GPIO_H
#define EVSE_BOOT_HW__HW_GPIO_H

#include <stdint.h>

#include "HW/hw_common.h"
#include "stm32f4xx_hal.h"

// tables
typedef struct{
    GPIO_TypeDef *port;
    uint16_t pin;
} hw_gpio_t;

// functions
hw_status_t HwGpio_Attach(hw_gpio_t *gpio, GPIO_TypeDef *port, uint16_t pin);
hw_status_t HwGpio_Read(const hw_gpio_t *gpio, GPIO_PinState *state);
hw_status_t HwGpio_Write(const hw_gpio_t *gpio, GPIO_PinState state);
hw_status_t HwGpio_Toggle(const hw_gpio_t *gpio);

#endif // EVSE_BOOT_HW__HW_GPIO_H