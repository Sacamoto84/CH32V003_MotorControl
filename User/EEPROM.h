/******************************************************************************
 * EEPROM.h - Flash Storage Library для CH32V003J4M6
 *
 * Created by  Shakir Abdo
 * Modified for block-based wear leveling architecture
 *
 * ОСОБЕННОСТИ:
 * - 1024 байта разделены на 16 блоков по 64 байта
 * - Fast Page Erase (FLASH_CTLR_PAGE_ER) для стирания 64-байтных блоков
 * - Wear leveling между блоками
 * - Хранение только байтов (без CRC для экономии ресурса)
 * - Структура: ID(1) + Value(1) = 2 байта на переменную
 ******************************************************************************/

//https://github.com/shakir-abdo/ch32v003-eeprom

#ifndef EEPROM_H
#define EEPROM_H

#include "debug.h"

#ifdef __cplusplus
 extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// Error codes
#define EEPROM_OK                   0   // Success
#define EEPROM_ERROR_FLASH_LOCK     1   // Flash unlock/lock failed
#define EEPROM_ERROR_FLASH_ERASE    2   // Flash erase failed
#define EEPROM_ERROR_FLASH_WRITE    3   // Flash write failed
#define EEPROM_ERROR_FLASH_VERIFY   4   // Flash write verification failed
#define EEPROM_ERROR_TIMEOUT        5   // Flash operation timeout
#define EEPROM_ERROR_NOT_INIT       6   // EEPROM not initialized
#define EEPROM_ERROR_VAR_NOT_FOUND  7   // Variable ID not found
#define EEPROM_ERROR_CRC_MISMATCH   8   // CRC verification failed (не используется)
#define EEPROM_ERROR_STORAGE_FULL   9   // Storage capacity exceeded
#define EEPROM_ERROR_INVALID_ID     10  // Invalid variable ID
#define EEPROM_ERROR_INVALID_PARAM  11  // Invalid parameter

// Legacy compatibility
#define EEPROM_ERROR 1  // Legacy error code

// Configuration
#define EEPROM_ADDRESS      0x08003C00  // Flash storage address (последние 1024 байта)
#define EEPROM_MAX_VARIABLES 254        // Максимум уникальных переменных (ID: 1-254)
#define EEPROM_PAGE_SIZE    1024        // Общий размер области (1024 байта)
#define EEPROM_MARKER       0x45455045  // "EEP" magic marker (32-bit) - не используется
#define EEPROM_VERSION      0x01        // EEPROM format version - не используется

// Block-based architecture
#define EEPROM_BLOCK_SIZE   64          // Размер блока Fast Page Erase
#define EEPROM_NUM_BLOCKS   16          // Количество блоков (1024/64)
#define EEPROM_BLOCK_HEADER 4           // Размер заголовка блока
#define EEPROM_VAR_SIZE     2           // Размер переменной: ID(1) + Value(1)

// Block header marker
#define EEPROM_BLOCK_MARKER     0xEE55  // Признак валидного блока
#define EEPROM_BLOCK_ACTIVE     0xAA    // Статус: активный блок
#define EEPROM_BLOCK_OBSOLETE   0x00    // Статус: устаревший блок
#define EEPROM_BLOCK_EMPTY      0xFF    // Статус: пустой блок

// Flash registers (для справки)
#define FLASH_CTLR_PG           ((uint16_t)0x0001)      // Programming
#define FLASH_CTLR_PER          ((uint16_t)0x0002)      // Page Erase 1KByte
#define FLASH_CTLR_STRT         ((uint16_t)0x0040)      // Start
#define FLASH_CTLR_LOCK         ((uint16_t)0x0080)      // Lock
#define FLASH_CTLR_PAGE_ER      ((uint32_t)0x00020000)  // Page Erase 64 Byte

// EEPROM device structure для поддержки нескольких экземпляров
typedef struct {
    uint32_t base_address;      // Flash storage base address
    uint16_t max_variables;     // Maximum number of variables
    bool initialized;           // Initialization status
    uint8_t wear_level_counter; // Wear leveling counter (количество компакций)
    uint32_t last_write_time;   // Last write operation time
    uint16_t write_count;       // Total write operations counter
} EEPROM_Dev;

// ============================================================================
// ОСНОВНОЙ API - работа с байтами
// ============================================================================

/**
 * @brief Инициализация EEPROM
 * @param dev Указатель на структуру устройства
 * @param base_address Базовый адрес во flash (должен быть выровнен по 64)
 * @return EEPROM_OK при успехе
 */
uint8_t eeprom_init(EEPROM_Dev* dev, uint32_t base_address);

/**
 * @brief Деинициализация EEPROM
 * @param dev Указатель на структуру устройства
 */
void eeprom_deinit(EEPROM_Dev* dev);

/**
 * @brief Форматирование всей области EEPROM (стирание 1024 байт)
 * @param dev Указатель на структуру устройства
 * @return EEPROM_OK при успехе
 */
uint8_t eeprom_format(EEPROM_Dev* dev);

/**
 * @brief Сохранить байт
 * @param dev Указатель на структуру устройства
 * @param id ID переменной (1-254, 0 и 255 зарезервированы)
 * @param value Значение (0-255)
 * @return EEPROM_OK при успехе
 * @note При повторной записи старое значение помечается как удалённое
 * @note Автоматическая компакция при заполнении блока
 */
uint8_t eeprom_saveByte(EEPROM_Dev* dev, uint8_t id, uint8_t value);

/**
 * @brief Прочитать байт
 * @param dev Указатель на структуру устройства
 * @param id ID переменной
 * @param value Указатель для сохранения значения
 * @return EEPROM_OK при успехе, EEPROM_ERROR_VAR_NOT_FOUND если не найдено
 */
uint8_t eeprom_readByte(EEPROM_Dev* dev, uint8_t id, uint8_t* value);

/**
 * @brief Удалить переменную
 * @param dev Указатель на структуру устройства
 * @param id ID переменной
 * @return EEPROM_OK при успехе
 * @note Помечает переменную как удалённую (ID = 0x00)
 */
uint8_t eeprom_deleteVar(EEPROM_Dev* dev, uint8_t id);

/**
 * @brief Проверить существование переменной
 * @param dev Указатель на структуру устройства
 * @param id ID переменной
 * @return true если переменная существует
 */
bool eeprom_varExists(EEPROM_Dev* dev, uint8_t id);

/**
 * @brief Проверить инициализацию EEPROM
 * @param dev Указатель на структуру устройства
 * @return true если инициализирована
 */
bool eeprom_isInitialized(EEPROM_Dev* dev);

/**
 * @brief Получить статус EEPROM
 * @param dev Указатель на структуру устройства
 * @param used_vars Указатель для количества используемых переменных (может быть NULL)
 * @param free_space Указатель для свободного места в активном блоке (может быть NULL)
 * @return EEPROM_OK при успехе
 */
uint8_t eeprom_getStatus(EEPROM_Dev* dev, uint16_t* used_vars, uint16_t* free_space);

/**
 * @brief Ручная компакция (defragmentation)
 * @param dev Указатель на структуру устройства
 * @return EEPROM_OK при успехе
 * @note Переносит все актуальные переменные в следующий блок
 * @note Обычно вызывается автоматически при заполнении блока
 */
uint8_t eeprom_compact(EEPROM_Dev* dev);

/**
 * @brief Получить свободное место (для внутреннего использования)
 * @param dev Указатель на структуру устройства
 * @return Количество свободных байт в активном блоке
 */
uint8_t get_free_space(EEPROM_Dev* dev);

// ============================================================================
// LEGACY API - для обратной совместимости
// ============================================================================

/**
 * @brief Инициализация EEPROM (legacy)
 * @note Использует адрес по умолчанию EEPROM_ADDRESS
 */
void EEPROM_init(void);

/**
 * @brief Форматирование EEPROM (legacy)
 * @return EEPROM_OK при успехе
 */
uint8_t EEPROM_format(void);

/**
 * @brief Сохранить байт (legacy)
 * @param id ID переменной (1-254)
 * @param value Значение (0-255)
 * @return EEPROM_OK при успехе
 */
uint8_t EEPROM_saveByte(uint8_t id, uint8_t value);

/**
 * @brief Прочитать байт (legacy)
 * @param id ID переменной
 * @return Значение переменной или -1 при ошибке
 * @note При ошибке выводит диагностическое сообщение в printf
 */
int16_t EEPROM_readByte(uint8_t id);

/**
 * @brief Проверить существование переменной (legacy)
 * @param id ID переменной
 * @return 1 если существует, 0 если нет
 */
uint8_t EEPROM_varExists(uint8_t id);

/**
 * @brief Удалить переменную (legacy)
 * @param id ID переменной
 * @return EEPROM_OK при успехе
 */
uint8_t EEPROM_deleteVar(uint8_t id);

/**
 * @brief Ручная компакция (legacy)
 * @return EEPROM_OK при успехе
 */
uint8_t EEPROM_compact(void);

// ============================================================================
// ПРИМЕРЫ ИСПОЛЬЗОВАНИЯ
// ============================================================================

/*
// Пример 1: Legacy API (простой)
void example_legacy(void) {
    EEPROM_init();
    
    EEPROM_saveByte(1, 100);
    EEPROM_saveByte(2, 200);
    
    int16_t val1 = EEPROM_readByte(1);  // Вернёт 100 или -1 при ошибке
    int16_t val2 = EEPROM_readByte(2);  // Вернёт 200 или -1 при ошибке
    
    if (EEPROM_varExists(3)) {
        printf("Variable 3 exists\n");
    }
}

// Пример 2: Новый API (с проверкой ошибок)
void example_new_api(void) {
    EEPROM_Dev eeprom;
    uint8_t result;
    
    // Инициализация
    result = eeprom_init(&eeprom, EEPROM_ADDRESS);
    if (result != EEPROM_OK) {
        printf("Init failed: %d\n", result);
        return;
    }
    
    // Сохранение
    result = eeprom_saveByte(&eeprom, 10, 150);
    if (result != EEPROM_OK) {
        printf("Save failed: %d\n", result);
        return;
    }
    
    // Чтение
    uint8_t value;
    result = eeprom_readByte(&eeprom, 10, &value);
    if (result == EEPROM_OK) {
        printf("Value: %d\n", value);
    } else {
        printf("Read failed: %d\n", result);
    }
    
    // Статус
    uint16_t used_vars, free_space;
    eeprom_getStatus(&eeprom, &used_vars, &free_space);
    printf("Variables: %d, Free space: %d bytes\n", used_vars, free_space);
    
    // Деинициализация
    eeprom_deinit(&eeprom);
}

// Пример 3: Множественные экземпляры
void example_multiple_instances(void) {
    EEPROM_Dev eeprom1, eeprom2;
    
    eeprom_init(&eeprom1, 0x08003C00);  // Первая область
    eeprom_init(&eeprom2, 0x08003800);  // Вторая область
    
    eeprom_saveByte(&eeprom1, 1, 100);
    eeprom_saveByte(&eeprom2, 1, 200);
    
    // Разные значения в разных областях
}
*/

#ifdef __cplusplus
}
#endif

#endif /* EEPROM_H */