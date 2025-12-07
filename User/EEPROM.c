/******************************************************************************
 * EEPROM.c - Flash Storage Library для CH32V003J4M6
 *
 * ОСОБЕННОСТИ:
 * 1. Использует 1024 байта (последняя страница flash)
 * 2. Разделена на 16 блоков по 64 байта (Fast Page Erase)
 * 3. Wear leveling между блоками
 * 4. Структура: ID(1) + Value(1) = 2 байта (БЕЗ CRC для экономии записей)
 ******************************************************************************/

#include "EEPROM.h"
#include <stdbool.h>
#include <string.h>

// Legacy compatibility
static EEPROM_Dev legacy_device;

#define MCU_CORE_CLOCK FUNCONF_SYSTEM_CORE_CLOCK

// Flash keys
#define FLASH_KEY1 ((uint32_t)0x45670123)
#define FLASH_KEY2 ((uint32_t)0xCDEF89AB)

// Storage constants
#define EEPROM_TOTAL_SIZE 1024          // Общий размер (последняя страница)
#define BLOCK_SIZE 64                    // Размер блока Fast Page Erase
#define NUM_BLOCKS 16                    // Количество блоков (1024/64)
#define BLOCK_HEADER_SIZE 4              // Заголовок блока
#define VARIABLE_SIZE 2                  // ID(1) + Value(1) - БЕЗ CRC!
#define FLASH_TIMEOUT_CYCLES 50000

// Заголовок блока (4 байта)
typedef struct {
    uint16_t marker;        // 0xEE55 - признак валидного блока
    uint8_t sequence;       // Номер последовательности для wear leveling
    uint8_t status;         // 0xFF=пустой, 0xAA=активный, 0x00=устаревший
} __attribute__ ((packed)) BlockHeader;

// Variable entry (2 bytes) - упрощено без CRC
typedef struct {
    uint8_t id;
    uint8_t value;
} __attribute__ ((packed)) VariableEntry;

// Forward declarations
static uint8_t flash_unlock(void);
static void flash_lock(void);
static uint8_t flash_wait_for_operation(void);
static uint8_t flash_erase_page(uint32_t address);
static uint8_t flash_erase_block_64(uint32_t address);
static uint8_t flash_write_halfword(uint32_t address, uint16_t data);
static uint8_t find_active_block(EEPROM_Dev *dev);
static uint8_t find_variable_in_storage(EEPROM_Dev *dev, uint8_t id, uint32_t *address);
static uint8_t get_block_free_space(uint32_t block_addr);
static uint8_t compact_to_new_block(EEPROM_Dev *dev, uint8_t current_block);

// Initialize EEPROM
uint8_t eeprom_init(EEPROM_Dev *dev, uint32_t base_address) {
    if (!dev) {
        return EEPROM_ERROR_INVALID_PARAM;
    }

    if (dev->initialized) {
        eeprom_deinit(dev);
    }

    dev->base_address = base_address;
    dev->max_variables = EEPROM_MAX_VARIABLES;
    dev->wear_level_counter = 0;
    dev->last_write_time = 0;
    dev->write_count = 0;
    dev->initialized = true;

    // Ищем активный блок или форматируем
    uint8_t active_block = find_active_block(dev);
    
    if (active_block == 0xFF) {
        // Не найден активный блок - форматируем
        uint8_t result = eeprom_format(dev);
        if (result != EEPROM_OK) {
            dev->initialized = false;
            return result;
        }
    }

    return EEPROM_OK;
}

void eeprom_deinit(EEPROM_Dev *dev) {
    if (!dev || !dev->initialized) {
        return;
    }

    dev->base_address = 0;
    dev->max_variables = 0;
    dev->wear_level_counter = 0;
    dev->last_write_time = 0;
    dev->write_count = 0;
    dev->initialized = false;
}

bool eeprom_isInitialized(EEPROM_Dev *dev) {
    return (dev && dev->initialized);
}

// Форматирует всю область и инициализирует первый блок
uint8_t eeprom_format(EEPROM_Dev *dev) {
    if (!dev || !dev->initialized) {
        return EEPROM_ERROR_NOT_INIT;
    }

    // Стираем всю страницу (1024 байта)
    uint8_t result = flash_erase_page(dev->base_address);
    if (result != EEPROM_OK) {
        return result;
    }

    // Инициализируем первый блок (блок 0)
    uint32_t block0_addr = dev->base_address;
    
    // Записываем заголовок блока
    // marker = 0xEE55
    result = flash_write_halfword(block0_addr, 0xEE55);
    if (result != EEPROM_OK) {
        return result;
    }
    
    // sequence = 0, status = 0xAA (активный)
    result = flash_write_halfword(block0_addr + 2, 0xAA00);
    if (result != EEPROM_OK) {
        return result;
    }

    dev->write_count = 0;
    dev->wear_level_counter = 0;

    return EEPROM_OK;
}

// Сохранить байт
uint8_t eeprom_saveByte(EEPROM_Dev *dev, uint8_t id, uint8_t value) {
    if (!dev || !dev->initialized) {
        return EEPROM_ERROR_NOT_INIT;
    }

    if (id == 0 || id == 0xFF) {
        return EEPROM_ERROR_INVALID_ID;
    }

    // Находим активный блок
    uint8_t active_block_num = find_active_block(dev);
    if (active_block_num == 0xFF) {
        return EEPROM_ERROR_NOT_INIT;
    }

    uint32_t active_block_addr = dev->base_address + (active_block_num * BLOCK_SIZE);

    // Помечаем старую запись как удалённую (если существует)
    uint32_t old_address;
    if (find_variable_in_storage(dev, id, &old_address)) {
        uint8_t result = flash_write_halfword(old_address, 0x0000);
        if (result != EEPROM_OK) {
            return result;
        }
    }

    // Проверяем свободное место в текущем блоке
    uint8_t free_space = get_block_free_space(active_block_addr);
    
    if (free_space < VARIABLE_SIZE) {
        // Нужна компакция в новый блок
        uint8_t result = compact_to_new_block(dev, active_block_num);
        if (result != EEPROM_OK) {
            return result;
        }
        
        // Обновляем адрес активного блока
        active_block_num = find_active_block(dev);
        if (active_block_num == 0xFF) {
            return EEPROM_ERROR_STORAGE_FULL;
        }
        active_block_addr = dev->base_address + (active_block_num * BLOCK_SIZE);
    }

    // Находим свободное место в активном блоке
    uint32_t write_address = active_block_addr + BLOCK_HEADER_SIZE;

    while (write_address < (active_block_addr + BLOCK_SIZE - VARIABLE_SIZE)) {
        uint8_t stored_id = *(volatile uint8_t *)write_address;
        if (stored_id == 0xFF) {
            break;
        }
        write_address += VARIABLE_SIZE;
    }

    if (write_address >= (active_block_addr + BLOCK_SIZE - VARIABLE_SIZE)) {
        return EEPROM_ERROR_STORAGE_FULL;
    }

    // Записываем переменную (2 байта = 1 halfword) - БЕЗ CRC!
    uint8_t result;
    uint16_t word = (uint16_t)(value << 8) | id;
    result = flash_write_halfword(write_address, word);
    if (result != EEPROM_OK)
        return result;

    dev->write_count++;
    dev->last_write_time = SysTick->CNT;

    return EEPROM_OK;
}

// Прочитать байт
uint8_t eeprom_readByte(EEPROM_Dev *dev, uint8_t id, uint8_t *value) {
    if (!dev || !dev->initialized || !value) {
        return EEPROM_ERROR_INVALID_PARAM;
    }

    uint32_t address;
    if (!find_variable_in_storage(dev, id, &address)) {
        return EEPROM_ERROR_VAR_NOT_FOUND;
    }

    // Читаем значение (2 байта = 1 halfword)
    uint16_t word = *(volatile uint16_t *)address;
    *value = (word >> 8) & 0xFF;

    return EEPROM_OK;
}

bool eeprom_varExists(EEPROM_Dev *dev, uint8_t id) {
    uint32_t dummy_address;
    return find_variable_in_storage(dev, id, &dummy_address);
}

uint8_t eeprom_deleteVar(EEPROM_Dev *dev, uint8_t id) {
    if (!dev || !dev->initialized) {
        return EEPROM_ERROR_NOT_INIT;
    }

    uint32_t address;
    if (!find_variable_in_storage(dev, id, &address)) {
        return EEPROM_ERROR_VAR_NOT_FOUND;
    }

    uint8_t result = flash_write_halfword(address, 0x0000);
    return result;
}

uint8_t eeprom_getStatus(EEPROM_Dev *dev, uint16_t *used_vars, uint16_t *free_space) {
    if (!dev || !dev->initialized) {
        return EEPROM_ERROR_NOT_INIT;
    }

    if (used_vars) {
        // Подсчёт уникальных переменных
        uint8_t unique_ids[256] = {0};
        uint8_t count = 0;
        
        for (uint8_t block = 0; block < NUM_BLOCKS; block++) {
            uint32_t block_addr = dev->base_address + (block * BLOCK_SIZE);
            BlockHeader *header = (BlockHeader *)block_addr;
            
            if (header->marker != 0xEE55) continue;
            
            uint32_t addr = block_addr + BLOCK_HEADER_SIZE;
            while (addr < (block_addr + BLOCK_SIZE - VARIABLE_SIZE)) {
                uint8_t stored_id = *(volatile uint8_t *)addr;
                if (stored_id == 0xFF) break;
                if (stored_id != 0) {
                    unique_ids[stored_id] = 1;
                }
                addr += VARIABLE_SIZE;
            }
        }
        
        for (int i = 0; i < 256; i++) {
            if (unique_ids[i]) count++;
        }
        *used_vars = count;
    }

    if (free_space) {
        uint8_t active_block = find_active_block(dev);
        if (active_block != 0xFF) {
            uint32_t block_addr = dev->base_address + (active_block * BLOCK_SIZE);
            *free_space = get_block_free_space(block_addr);
        } else {
            *free_space = 0;
        }
    }

    return EEPROM_OK;
}

// Ручная компакция
uint8_t eeprom_compact(EEPROM_Dev *dev) {
    if (!dev || !dev->initialized) {
        return EEPROM_ERROR_NOT_INIT;
    }

    uint8_t active_block = find_active_block(dev);
    if (active_block == 0xFF) {
        return EEPROM_ERROR_NOT_INIT;
    }

    return compact_to_new_block(dev, active_block);
}

// Flash operations
static uint8_t flash_unlock(void) {
    if (FLASH->CTLR & FLASH_CTLR_LOCK) {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;

        if (FLASH->CTLR & FLASH_CTLR_LOCK) {
            return EEPROM_ERROR_FLASH_LOCK;
        }
    }
    return EEPROM_OK;
}

static void flash_lock(void) {
    FLASH->CTLR |= FLASH_CTLR_LOCK;
}

static uint8_t flash_wait_for_operation(void) {
    uint32_t timeout = FLASH_TIMEOUT_CYCLES;

    while ((FLASH->STATR & FLASH_STATR_BSY) && timeout) {
        timeout--;
    }

    return (timeout > 0) ? EEPROM_OK : EEPROM_ERROR_TIMEOUT;
}

// Стирание всей страницы (1024 байта)
static uint8_t flash_erase_page(uint32_t address) {
    uint8_t result = flash_unlock();
    if (result != EEPROM_OK) {
        return result;
    }

    result = flash_wait_for_operation();
    if (result != EEPROM_OK) {
        flash_lock();
        return result;
    }

    FLASH->CTLR |= FLASH_CTLR_PER;
    FLASH->ADDR = address;
    FLASH->CTLR |= FLASH_CTLR_STRT;

    result = flash_wait_for_operation();

    FLASH->CTLR &= ~FLASH_CTLR_PER;

    if (*(volatile uint32_t *)address != 0xFFFFFFFF) {
        flash_lock();
        return EEPROM_ERROR_FLASH_ERASE;
    }

    flash_lock();
    return result;
}

// Fast Page Erase - стирание 64 байт
static uint8_t flash_erase_block_64(uint32_t address) {
    uint8_t result = flash_unlock();
    if (result != EEPROM_OK) {
        return result;
    }

    result = flash_wait_for_operation();
    if (result != EEPROM_OK) {
        flash_lock();
        return result;
    }

    // Используем Page Erase 64 Byte (FLASH_CTLR_PAGE_ER)
    FLASH->CTLR |= FLASH_CTLR_PAGE_ER;  // 0x00020000
    FLASH->ADDR = address;
    FLASH->CTLR |= FLASH_CTLR_STRT;

    result = flash_wait_for_operation();

    FLASH->CTLR &= ~FLASH_CTLR_PAGE_ER;

    // Проверяем стирание
    if (*(volatile uint32_t *)address != 0xFFFFFFFF) {
        flash_lock();
        return EEPROM_ERROR_FLASH_ERASE;
    }

    flash_lock();
    return result;
}

static uint8_t flash_write_halfword(uint32_t address, uint16_t data) {
    uint8_t result = flash_unlock();
    if (result != EEPROM_OK) {
        return result;
    }

    result = flash_wait_for_operation();
    if (result != EEPROM_OK) {
        flash_lock();
        return result;
    }

    FLASH->CTLR |= FLASH_CTLR_PG;

    *(volatile uint16_t *)address = data;

    result = flash_wait_for_operation();

    FLASH->CTLR &= ~FLASH_CTLR_PG;

    if (*(volatile uint16_t *)address != data) {
        flash_lock();
        return EEPROM_ERROR_FLASH_VERIFY;
    }

    flash_lock();
    return result;
}

// Найти активный блок
static uint8_t find_active_block(EEPROM_Dev *dev) {
    uint8_t max_sequence = 0;
    uint8_t active_block = 0xFF;
    uint8_t found_first = 0;

    for (uint8_t i = 0; i < NUM_BLOCKS; i++) {
        uint32_t block_addr = dev->base_address + (i * BLOCK_SIZE);
        BlockHeader *header = (BlockHeader *)block_addr;

        if (header->marker == 0xEE55 && header->status == 0xAA) {
            if (!found_first) {
                max_sequence = header->sequence;
                active_block = i;
                found_first = 1;
            } else {
                // Учитываем переполнение счётчика (255 -> 0)
                int8_t diff = (int8_t)(header->sequence - max_sequence);
                if (diff > 0) {
                    max_sequence = header->sequence;
                    active_block = i;
                }
            }
        }
    }

    return active_block;
}

// Найти переменную во всех блоках (возвращает последнее вхождение)
static uint8_t find_variable_in_storage(EEPROM_Dev *dev, uint8_t id, uint32_t *address) {
    uint32_t last_found = 0;
    uint8_t found = 0;
    uint8_t max_sequence = 0;
    uint8_t found_first_seq = 0;

    // Ищем во всех блоках, начиная с самого нового
    for (uint8_t block = 0; block < NUM_BLOCKS; block++) {
        uint32_t block_addr = dev->base_address + (block * BLOCK_SIZE);
        BlockHeader *header = (BlockHeader *)block_addr;

        if (header->marker != 0xEE55) continue;

        uint32_t current_addr = block_addr + BLOCK_HEADER_SIZE;
        uint32_t block_last_found = 0;
        uint8_t block_found = 0;

        // Ищем последнее вхождение в этом блоке
        while (current_addr < (block_addr + BLOCK_SIZE - VARIABLE_SIZE)) {
            uint8_t stored_id = *(volatile uint8_t *)current_addr;

            if (stored_id == 0xFF) {
                break;
            }

            if (stored_id == id && stored_id != 0) {
                block_last_found = current_addr;
                block_found = 1;
            }

            current_addr += VARIABLE_SIZE;
        }

        // Если нашли в этом блоке, проверяем sequence
        if (block_found) {
            if (!found_first_seq) {
                max_sequence = header->sequence;
                last_found = block_last_found;
                found = 1;
                found_first_seq = 1;
            } else {
                // Учитываем переполнение счётчика
                int8_t diff = (int8_t)(header->sequence - max_sequence);
                if (diff > 0) {
                    max_sequence = header->sequence;
                    last_found = block_last_found;
                    found = 1;
                }
            }
        }
    }

    if (found && address) {
        *address = last_found;
    }

    return found;
}

// Получить свободное место в блоке
static uint8_t get_block_free_space(uint32_t block_addr) {
    uint32_t current_addr = block_addr + BLOCK_HEADER_SIZE;
    uint8_t used = BLOCK_HEADER_SIZE;

    while (current_addr < (block_addr + BLOCK_SIZE)) {
        uint8_t stored_id = *(volatile uint8_t *)current_addr;
        if (stored_id == 0xFF) {
            break;
        }
        used += VARIABLE_SIZE;
        current_addr += VARIABLE_SIZE;
    }

    return (BLOCK_SIZE - used);
}

// Компакция в новый блок с wear leveling
static uint8_t compact_to_new_block(EEPROM_Dev *dev, uint8_t current_block) {
    uint32_t current_block_addr = dev->base_address + (current_block * BLOCK_SIZE);
    BlockHeader *current_header = (BlockHeader *)current_block_addr;
    
    // Находим следующий свободный блок (циклически)
    uint8_t next_block = (current_block + 1) % NUM_BLOCKS;
    uint32_t next_block_addr = dev->base_address + (next_block * BLOCK_SIZE);

    // Стираем следующий блок (Fast Erase 64 байта)
    uint8_t result = flash_erase_block_64(next_block_addr);
    if (result != EEPROM_OK) {
        return result;
    }

    // Записываем заголовок нового блока
    uint8_t new_sequence = current_header->sequence + 1;
    result = flash_write_halfword(next_block_addr, 0xEE55);
    if (result != EEPROM_OK) return result;
    
    result = flash_write_halfword(next_block_addr + 2, 0xAA00 | new_sequence);
    if (result != EEPROM_OK) return result;

    // Копируем только последние версии переменных из ВСЕХ блоков
    uint8_t copied_ids[256] = {0};
    uint32_t write_pos = next_block_addr + BLOCK_HEADER_SIZE;

    // Сначала собираем все уникальные ID и их последние значения
    // Проходим блоки от нового к старому
    for (int pass = 0; pass < NUM_BLOCKS; pass++) {
        // Ищем блок с максимальным sequence среди непройденных
        uint8_t max_seq = 0;
        int8_t max_block = -1;
        uint8_t found_block = 0;
        
        for (uint8_t block = 0; block < NUM_BLOCKS; block++) {
            uint32_t block_addr = dev->base_address + (block * BLOCK_SIZE);
            BlockHeader *header = (BlockHeader *)block_addr;
            
            if (header->marker != 0xEE55) continue;
            
            // Проверяем, не обработан ли уже этот блок
            uint8_t already_processed = 0;
            if (block == next_block) already_processed = 1;  // Пропускаем новый блок
            
            if (!already_processed) {
                if (!found_block || ((int8_t)(header->sequence - max_seq) > 0)) {
                    max_seq = header->sequence;
                    max_block = block;
                    found_block = 1;
                }
            }
        }
        
        if (max_block < 0) break;
        
        // Обрабатываем найденный блок
        uint32_t block_addr = dev->base_address + (max_block * BLOCK_SIZE);
        uint32_t addr = block_addr + BLOCK_HEADER_SIZE;
        
        while (addr < (block_addr + BLOCK_SIZE - VARIABLE_SIZE)) {
            uint8_t stored_id = *(volatile uint8_t *)addr;
            if (stored_id == 0xFF) break;
            
            if (stored_id != 0 && !copied_ids[stored_id]) {
                // Копируем переменную (2 байта = 1 halfword)
                if (write_pos + VARIABLE_SIZE <= (next_block_addr + BLOCK_SIZE)) {
                    uint16_t word = *(volatile uint16_t *)addr;
                    
                    result = flash_write_halfword(write_pos, word);
                    if (result != EEPROM_OK) return result;
                    
                    write_pos += VARIABLE_SIZE;
                    copied_ids[stored_id] = 1;
                }
            }
            addr += VARIABLE_SIZE;
        }
        
        // Помечаем блок как обработанный, записывая status = 0x00
        result = flash_write_halfword(block_addr + 2, (max_seq << 8) | 0x00);
        if (result != EEPROM_OK) return result;
    }

    dev->wear_level_counter++;
    return EEPROM_OK;
}

// Legacy API



void EEPROM_init(void) {
    eeprom_init(&legacy_device, EEPROM_ADDRESS);
}

uint8_t EEPROM_format(void) {
    return eeprom_format(&legacy_device);
}

uint8_t EEPROM_saveByte(uint8_t id, uint8_t value) {
    return eeprom_saveByte(&legacy_device, id, value);
}

int16_t EEPROM_readByte(uint8_t id) {
    uint8_t value;
    uint8_t res = eeprom_readByte(&legacy_device, id, &value);
    
    if (res == EEPROM_OK) {
        return value;
    }
    
    printf("Error EEPROM_readByte code:%d ", res);
    switch (res) {
        case 0: printf("Success"); break;
        case 1: printf("Flash unlock/lock failed"); break;
        case 2: printf("Flash erase failed"); break;
        case 3: printf("Flash write failed"); break;
        case 4: printf("Flash write verification failed"); break;
        case 5: printf("Flash operation timeout"); break;
        case 6: printf("EEPROM not initialized"); break;
        case 7: printf("Variable ID not found"); break;
        case 8: printf("CRC verification failed"); break;
        case 9: printf("Storage capacity exceeded"); break;
        case 10: printf("Invalid variable ID"); break;
        case 11: printf("Invalid parameter"); break;
    }
    printf("\n");
    return -1;
}

uint8_t EEPROM_varExists(uint8_t id) {
    return eeprom_varExists(&legacy_device, id) ? 1 : 0;
}

uint8_t EEPROM_deleteVar(uint8_t id) {
    return eeprom_deleteVar(&legacy_device, id);
}

uint8_t EEPROM_compact(void) {
    return eeprom_compact(&legacy_device);
}