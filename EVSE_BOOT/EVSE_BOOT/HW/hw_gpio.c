#include "hw_gpio.h"

#include <stddef.h>

// functions
hw_status_t HwGpio_Attach(hw_gpio_t *gpio, GPIO_TypeDef *port, uint16_t pin) {
    // HW GPIO 객체에 연결
    if ((gpio == NULL) || (port == NULL) || (pin == 0U)) return HW_STATUS_INVALID_ARGUMENT;

    gpio->port = port;
    gpio->pin = pin;

    return HW_STATUS_OK;
}

hw_status_t HwGpio_Read(const hw_gpio_t *gpio, GPIO_PinState *state) {
    if ((gpio == NULL) || (state == NULL)) return HW_STATUS_INVALID_ARGUMENT;

    if ((gpio->port == NULL) || (gpio->pin == 0U)) return HW_STATUS_NOT_INITIALIZED;

    *state = HAL_GPIO_ReadPin(gpio->port, gpio->pin);

    return HW_STATUS_OK;
}

hw_status_t HwGpio_Write(const hw_gpio_t *gpio, GPIO_PinState state) {
    if (gpio == NULL) return HW_STATUS_INVALID_ARGUMENT;

    if ((gpio->port == NULL) || (gpio->pin == 0U)) return HW_STATUS_NOT_INITIALIZED;

    HAL_GPIO_WritePin(gpio->port, gpio->pin, state);

    return HW_STATUS_OK;
}

hw_status_t HwGpio_Toggle(const hw_gpio_t *gpio) {
    if (gpio == NULL) return HW_STATUS_INVALID_ARGUMENT;

    if ((gpio->port == NULL) || (gpio->pin == 0U)) return HW_STATUS_NOT_INITIALIZED;

    HAL_GPIO_TogglePin(gpio->port, gpio->pin);

    return HW_STATUS_OK;
}