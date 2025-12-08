#pragma once

#ifdef __cplusplus

/* Includes ------------------------------------------------------------------*/
#include "ch32v00x_flash.h"

/* Конфигурация отладки ------------------------------------------------------*/
/* Главный переключатель отладки - включает/выключает ВСЕ логи */
#ifndef EEPROM_DEBUG
#define EEPROM_DEBUG 1  // 0 = отключить, 1 = включить отладочные сообщения
#endif

/* Цветовые макросы */
#define CLRscr "\033[2J\033[H"
#define FG(color) "\033[38;5;" #color "m"
#define BG(color) "\033[48;5;" #color "m"
#define RESET1 "\033[0m"
#define BOLD "\033[1m"
#define UNDERLINE "\033[4m"

/* Макросы для логирования */
#if EEPROM_DEBUG
#include <stdio.h>
#define EEPROM_LOG(fmt, ...) printf (FG(51) "[EEPROM] " RESET1 fmt "\n", ##__VA_ARGS__)
#define EEPROM_LOG_OK(fmt, ...) printf (FG(46) "[EEPROM] ✓ " RESET1 fmt "\n", ##__VA_ARGS__)
#define EEPROM_LOG_WARN(fmt, ...) printf (FG(226) "[EEPROM] ⚠ " RESET1 fmt "\n", ##__VA_ARGS__)
#define EEPROM_LOG_ERROR(fmt, ...) printf (FG(196) "[EEPROM] ✗ " RESET1 fmt "\n", ##__VA_ARGS__)
#define EEPROM_LOG_INFO(fmt, ...) printf (FG(141) "[EEPROM] ℹ " RESET1 fmt "\n", ##__VA_ARGS__)
#define EEPROM_LOG_DEBUG(fmt, ...) printf (FG(93) "[EEPROM] 🔧 " RESET1 fmt "\n", ##__VA_ARGS__)
#else
#define EEPROM_LOG(fmt, ...) ((void)0)
#define EEPROM_LOG_OK(fmt, ...) ((void)0)
#define EEPROM_LOG_WARN(fmt, ...) ((void)0)
#define EEPROM_LOG_ERROR(fmt, ...) ((void)0)
#define EEPROM_LOG_INFO(fmt, ...) ((void)0)
#define EEPROM_LOG_DEBUG(fmt, ...) ((void)0)
#endif

/* Начальный адрес EEPROM в Flash-памяти */
#define EEPROM_START_ADDRESS ((uint32_t)0x08003C00) /* Начало эмуляции EEPROM: начинается после 2КБ используемой Flash-памяти */

class uEeprom {
  public:
    // Конструктор по умолчанию - создает пустой объект
    uEeprom() 
        : index(0), min(0), max(0), define(0), title(nullptr), value(0), initialized(false) {
        // Никаких действий - объект пустой
    }

    uint16_t index;
    uint16_t min;
    uint16_t max;
    uint16_t define;
    const char * title;
    bool initialized;

    void set (uint16_t i) {
        if (!initialized) {
            EEPROM_LOG_ERROR("ОШИБКА! Попытка set() до вызова init()!");
            return;
        }
        
        if (i < min || i > max) {
            EEPROM_LOG_WARN(FG(82) "\"%s\"" RESET1 ": Значение " FG(196) "%u" RESET1 
                           " выходит за границы [%u..%u], ограничено", title, i, min, max);
            i = (i < min) ? min : max;
        }
        
        EEPROM_LOG(FG(82) "\"%s\"" RESET1 ": Установка значения: " FG(226) "индекс=%u" RESET1 
                  ", старое=%u → " FG(82) "новое=%u" RESET1, title, index, value, i);
        value = i;
    }

    uint16_t get (void) {
        if (!initialized) {
            EEPROM_LOG_ERROR("ОШИБКА! Попытка get() до вызова init()!");
            return 0;
        }
        
        EEPROM_LOG(FG(82) "\"%s\"" RESET1 ": Чтение значения: " FG(226) "индекс=%u" RESET1 
                  ", значение=" FG(82) "%u" RESET1, title, index, value);
        return value;
    }

    FLASH_Status save() {
        if (!initialized) {
            EEPROM_LOG_ERROR("ОШИБКА! Попытка save() до вызова init()!");
            return FLASH_ERROR_PG;
        }
        
        uint32_t address = EEPROM_START_ADDRESS + (index * 64);
        
        EEPROM_LOG(BOLD FG(82) "\"%s\"" RESET1 BOLD ": Сохранение в Flash" RESET1, title);
        EEPROM_LOG("  " FG(226) "индекс=%u" RESET1 ", адрес=" FG(141) "0x%08X" RESET1 
                  ", значение=" FG(82) "%u" RESET1, index, address, value);

        uint16_t currentValue = *(volatile uint16_t *)address;
        if (currentValue == value) {
            EEPROM_LOG_OK(FG(82) "\"%s\"" RESET1 ": Значение уже записано (0x%04X), пропуск записи", 
                         title, currentValue);
            return FLASH_COMPLETE;
        }

        EEPROM_LOG_INFO(FG(82) "\"%s\"" RESET1 ": Разблокировка Flash...", title);
        FLASH_Unlock();
        
        EEPROM_LOG_INFO(FG(82) "\"%s\"" RESET1 ": Быстрое стирание страницы по адресу " 
                       FG(141) "0x%08X" RESET1 "...", title, address);
        FLASH_ErasePage_Fast (address);

        EEPROM_LOG_INFO(FG(82) "\"%s\"" RESET1 ": Запись первого слова по адресу " 
                       FG(141) "0x%08X" RESET1 "...", title, address);
        FLASH_Status res = FLASH_ProgramHalfWord (address, value);
        if (res != FLASH_COMPLETE) {
            EEPROM_LOG_ERROR(FG(82) "\"%s\"" RESET1 ": ОШИБКА записи первого слова! Статус=%d", 
                            title, res);
        }
        
        EEPROM_LOG_INFO(FG(82) "\"%s\"" RESET1 ": Запись второго слова по адресу " 
                       FG(141) "0x%08X" RESET1 "...", title, address + 2);
        res = FLASH_ProgramHalfWord (address + 2, value);
        if (res != FLASH_COMPLETE) {
            EEPROM_LOG_ERROR(FG(82) "\"%s\"" RESET1 ": ОШИБКА записи второго слова! Статус=%d", 
                            title, res);
        }
        
        EEPROM_LOG_INFO(FG(82) "\"%s\"" RESET1 ": Блокировка Flash...", title);
        FLASH_Lock();

        // Проверка записи
        uint16_t verify1 = *(volatile uint16_t *)address;
        uint16_t verify2 = *(volatile uint16_t *)(address + 2);
        if (verify1 == value && verify2 == value) {
            EEPROM_LOG_OK(FG(82) "\"%s\"" RESET1 ": Сохранение завершено успешно, "
                         "проверка: слово1=0x%04X, слово2=0x%04X", title, verify1, verify2);
        } else {
            EEPROM_LOG_ERROR(FG(82) "\"%s\"" RESET1 ": ОШИБКА верификации! "
                            "Ожидалось=0x%04X, прочитано: слово1=0x%04X, слово2=0x%04X", 
                            title, value, verify1, verify2);
        }

        return res;
    }

    int16_t readEEPROM (void) {
        if (!initialized) {
            EEPROM_LOG_ERROR("ОШИБКА! Попытка readEEPROM() до вызова init()!");
            return 0;
        }
        
        uint32_t address = EEPROM_START_ADDRESS + (index * 64);
        int16_t val = *(volatile uint16_t *)address;
        EEPROM_LOG(FG(82) "\"%s\"" RESET1 ": Прямое чтение EEPROM: " FG(226) "индекс=%u" RESET1 
                  ", адрес=" FG(141) "0x%08X" RESET1 ", значение=" FG(82) "%d" RESET1, 
                   title, index, address, val);
        return val;
    }

    // ГЛАВНЫЙ МЕТОД - принимает все параметры и инициализирует объект
    void init(uint16_t _index, uint16_t _min, uint16_t _max, uint16_t _define, const char* _title) {
        // Сохраняем параметры
        index = _index;
        min = _min;
        max = _max;
        define = _define;
        title = _title;
        
        uint32_t address = EEPROM_START_ADDRESS + (index * 64);
        
        EEPROM_LOG_INFO(BOLD "╔════════════════════════════════════════════════════════════╗" RESET1);
        EEPROM_LOG_INFO(BOLD "║ Инициализация EEPROM: " FG(82) "%-30s" RESET1 BOLD " ║" RESET1, title);
        EEPROM_LOG_INFO(BOLD "╠════════════════════════════════════════════════════════════╣" RESET1);
        EEPROM_LOG_INFO("║ " FG(226) "Индекс:  %3u" RESET1 "                                             ║", index);
        EEPROM_LOG_INFO("║ " FG(141) "Адрес:   0x%08X" RESET1 "                                    ║", address);
        EEPROM_LOG_INFO("║ Диапазон: [%3u .. %3u]                                    ║", min, max);
        EEPROM_LOG_INFO("║ " FG(82) "Default:  %3u" RESET1 "                                            ║", define);
        EEPROM_LOG_INFO(BOLD "╚════════════════════════════════════════════════════════════╝" RESET1);
        
        uint16_t a = (*(volatile uint16_t *)address);
        uint16_t b = (*(volatile uint16_t *)(address + 2));

        EEPROM_LOG("Прочитано из Flash: слово1=" FG(141) "0x%04X" RESET1 " (%u), "
                  "слово2=" FG(141) "0x%04X" RESET1 " (%u)", 
                   a, a, b, b);

        if ((a != b) || (a == 0xFFFF) || (b == 0xFFFF)) {
            EEPROM_LOG_WARN("Обнаружены неверные данные или пустая память!");
            
            if (a != b) {
                EEPROM_LOG_WARN("  Причина: " FG(196) "несовпадение значений" RESET1 
                               " (слово1=0x%04X != слово2=0x%04X)", a, b);
            }
            if (a == 0xFFFF) {
                EEPROM_LOG_WARN("  Причина: " FG(196) "слово1=0xFFFF" RESET1 " (не запрограммировано)");
            }
            if (b == 0xFFFF) {
                EEPROM_LOG_WARN("  Причина: " FG(196) "слово2=0xFFFF" RESET1 " (не запрограммировано)");
            }
            
            EEPROM_LOG_INFO("Инициализация значением по умолчанию: " FG(82) "%u" RESET1, define);
            
            EEPROM_LOG_INFO("Разблокировка Flash...");
            FLASH_Unlock();
            
            EEPROM_LOG_INFO("Быстрое стирание страницы...");
            FLASH_ErasePage_Fast (address);
            
            EEPROM_LOG_INFO("Запись значения по умолчанию...");
            FLASH_Status st1 = FLASH_ProgramHalfWord (address, define);
            FLASH_Status st2 = FLASH_ProgramHalfWord (address+2, define);
            
            if (st1 != FLASH_COMPLETE || st2 != FLASH_COMPLETE) {
                EEPROM_LOG_ERROR("ОШИБКА записи! st1=%d, st2=%d", st1, st2);
            }
            
            EEPROM_LOG_INFO("Блокировка Flash...");
            FLASH_Lock();
            
            // Верификация
            uint16_t verify1 = *(volatile uint16_t *)address;
            uint16_t verify2 = *(volatile uint16_t *)(address + 2);
            EEPROM_LOG_DEBUG("Верификация: слово1=0x%04X, слово2=0x%04X", verify1, verify2);
            
            value = define;
            EEPROM_LOG_OK("Инициализация завершена, установлено значение=" FG(82) "%u" RESET1, value);
        } else {
            value = a;
            EEPROM_LOG_OK("Инициализация завершена успешно, загружено значение=" FG(82) "%u" RESET1, value);
        }
        
        initialized = true;
    }

  private:
    int16_t value;
};

#endif /* __cplusplus */

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/