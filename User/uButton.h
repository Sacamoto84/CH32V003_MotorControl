#pragma once

#ifdef __cplusplus

#include "uButtonVirt.h"

// Для использования gpio_dt_spec нужно включить Zephyr заголовок
extern "C" {
   
}

class uButton : public uButtonVirt {
   public:
    uButton() {
    }

    // вызывать в loop. Вернёт true при смене состояния
    bool tick() {
        return uButtonVirt::pollDebounce(readButton());
    }

    // прочитать состояние кнопки
    bool readButton() {
        return !GPIO_ReadInputDataBit (GPIOC, GPIO_Pin_4);
    }

   
};

#endif // __cplusplus