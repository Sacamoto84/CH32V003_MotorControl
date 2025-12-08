#pragma once

#ifdef __cplusplus

// #ifdef __cplusplus
// extern "C" {
// #endif

/* Includes ------------------------------------------------------------------*/
#include "ch32v00x_flash.h"

/* Конфигурация отладки ------------------------------------------------------*/
/* Главный переключатель отладки - включает/выключает ВСЕ логи */
#ifndef EEPROM_DEBUG
#define EEPROM_DEBUG 1  // 0 = отключить, 1 = включить отладочные сообщения
#endif

/* Макросы для логирования */
#if EEPROM_DEBUG
#include <stdio.h>
#define EEPROM_LOG(fmt, ...) printf ("[EEPROM] " fmt "\n", ##__VA_ARGS__)
#else
#define EEPROM_LOG(fmt, ...) ((void)0)
#endif

/* Начальный адрес EEPROM в Flash-памяти */
#define EEPROM_START_ADDRESS ((uint32_t)0x08003C00) /* Начало эмуляции EEPROM: начинается после 2КБ используемой Flash-памяти */

class uEeprom {
  public:
    uEeprom (uint16_t _index, int16_t _min, int16_t _max, int16_t _define) {
        index = _index;
        min = _min;
        max = _max;
        define = _define;
    }

    uint16_t index = 0;
    int16_t min = 0;
    int16_t max = 100;
    int16_t define = 100;

    void set (int16_t i) {
        value = i;
    }

    int16_t get (void) {
        return value;
    }

    FLASH_Status save () {

        uint32_t address = EEPROM_START_ADDRESS + (index * 64);
        if (*(volatile uint16_t *)address == value) {
            return FLASH_COMPLETE;
        }
        FLASH_Unlock();
        FLASH_ErasePage_Fast (address);
        FLASH_Status res = FLASH_ProgramHalfWord (address, value);
        FLASH_Lock();

        return res;
    }

    int16_t readEEPROM (void) {
        return *(volatile int16_t *)(EEPROM_START_ADDRESS + (index * 64));
    }


  private:
    int16_t value;
};

// #ifdef __cplusplus
// }
// #endif

#endif /* __EEPROM_H */

       /******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/