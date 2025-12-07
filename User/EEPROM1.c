/*
 * EEPROM emulation for CH32V003
 * - Uses 1024 bytes (16 blocks x 64B) starting from base_address
 * - Block: 64 bytes, header 4 bytes: marker(2) + sequence(1) + status(1)
 * - Variable entry: 2 bytes (id(1) + value(1)) stored as halfword (value<<8 | id)
 * - Wear leveling: copies last versions of variables into next erased block
 *
 * API:
 *  uint8_t eeprom_init(EEPROM_Dev *dev, uint32_t base_address);
 *  uint8_t eeprom_format(EEPROM_Dev *dev);
 *  uint8_t eeprom_saveByte(EEPROM_Dev *dev, uint8_t id, uint8_t value);
 *  uint8_t eeprom_readByte(EEPROM_Dev *dev, uint8_t id, uint8_t *value);
 *  uint8_t eeprom_deleteVar(EEPROM_Dev *dev, uint8_t id);
 *  uint8_t eeprom_compact(EEPROM_Dev *dev);
 *  uint8_t eeprom_getStatus(EEPROM_Dev *dev, uint16_t *used_vars, uint16_t *free_space);
 *
 * Notes:
 * - This is a single-file implementation; adapt to your project layout.
 * - Make sure FLASH register defines (FLASH->CTLR, etc.) match CH32V003 headers.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "ch32v00x_flash.h"

#ifndef EEPROM_DEBUG
#define EEPROM_DEBUG 1
#endif

#if EEPROM_DEBUG
#include <stdio.h>
#define LOG(fmt, ...) printf ("[EEPROM] " fmt "\n", ##__VA_ARGS__)
#else
#define LOG(fmt, ...) ((void)0)
#endif

/* Configuration */
#define EEPROM_TOTAL_SIZE 1024u
#define BLOCK_SIZE 64u
#define NUM_BLOCKS (EEPROM_TOTAL_SIZE / BLOCK_SIZE)
#define BLOCK_HEADER_SIZE 4u
#define VARIABLE_SIZE 2u

/* Flash keys and timeouts - these must match CH32V003 HAL/headers */
#define FLASH_KEY1 ((uint32_t)0x45670123)
#define FLASH_KEY2 ((uint32_t)0xCDEF89AB)
#define FLASH_TIMEOUT_CYCLES 50000u

// Error codes
#define EEPROM_OK 0                    // Success
#define EEPROM_ERROR_FLASH_LOCK 1      // Flash unlock/lock failed
#define EEPROM_ERROR_FLASH_ERASE 2     // Flash erase failed
#define EEPROM_ERROR_FLASH_WRITE 3     // Flash write failed
#define EEPROM_ERROR_FLASH_VERIFY 4    // Flash write verification failed
#define EEPROM_ERROR_TIMEOUT 5         // Flash operation timeout
#define EEPROM_ERROR_NOT_INIT 6        // EEPROM not initialized
#define EEPROM_ERROR_VAR_NOT_FOUND 7   // Variable ID not found
#define EEPROM_ERROR_CRC_MISMATCH 8    // CRC verification failed (не используется)
#define EEPROM_ERROR_STORAGE_FULL 9    // Storage capacity exceeded
#define EEPROM_ERROR_INVALID_ID 10     // Invalid variable ID
#define EEPROM_ERROR_INVALID_PARAM 11  // Invalid parameter

// Legacy compatibility
#define EEPROM_ERROR 1  // Legacy error code

// Configuration
#define EEPROM_ADDRESS 0x08003C00  // Flash storage address (последние 1024 байта)
#define EEPROM_MAX_VARIABLES 254   // Максимум уникальных переменных (ID: 1-254)
#define EEPROM_PAGE_SIZE 1024       // Общий размер области (1024 байта)
#define EEPROM_MARKER 0x45455045   // "EEP" magic marker (32-bit) - не используется
#define EEPROM_VERSION 0x01        // EEPROM format version - не используется

// Block-based architecture
#define EEPROM_BLOCK_SIZE 64   // Размер блока Fast Page Erase
#define EEPROM_NUM_BLOCKS 16   // Количество блоков (1024/64)
#define EEPROM_BLOCK_HEADER 4  // Размер заголовка блока
#define EEPROM_VAR_SIZE 2      // Размер переменной: ID(1) + Value(1)

/* Block header marker and statuses */
#define BLOCK_MARKER 0xEE55u // Признак валидного блока
#define STATUS_EMPTY 0xFFu   // Статус: активный блок
#define STATUS_ACTIVE 0xAAu  // Статус: устаревший блок
#define STATUS_OBSOLETE 0x00u // Статус: пустой блок

/* Device struct (simple) */
typedef struct {
    uint32_t base_address;
    uint16_t max_variables;
    uint32_t wear_level_counter;
    uint32_t last_write_time;
    uint32_t write_count;
    bool initialized;
} EEPROM_Dev;

/*/* Forward declarations of flash-specific functions (implementations below) */
static uint8_t flash_erase_block_64 (uint32_t address);
static uint8_t flash_write_halfword (uint32_t address, uint16_t data);

/* Public forward declarations */
uint8_t eeprom_format (EEPROM_Dev *dev);

/* Internal helpers */
static uint8_t find_active_block (EEPROM_Dev *dev);
static bool find_variable_in_storage (EEPROM_Dev *dev, uint8_t id, uint32_t *address);
static uint16_t get_block_free_space (uint32_t block_addr);
static uint8_t compact_to_new_block (EEPROM_Dev *dev, uint8_t current_block);

/* Legacy single instance */
static EEPROM_Dev legacy_device;

/* ---------------------- Public API ---------------------- */

int8_t eeprom_init (EEPROM_Dev *dev, uint32_t base_address) {
    if (!dev)
        return EEPROM_ERROR_INVALID_PARAM;
    if (dev->initialized) {
        // re-init: reset
        dev->base_address = 0;
        dev->max_variables = 0;
        dev->wear_level_counter = 0;
        dev->last_write_time = 0;
        dev->write_count = 0;
        dev->initialized = false;
    }

    dev->base_address = base_address;
    dev->max_variables = 256;  // logical limit
    dev->wear_level_counter = 0;
    dev->last_write_time = 0;
    dev->write_count = 0;
    dev->initialized = true;

    LOG ("Инициализировано в 0x%08X", (unsigned)base_address);

    uint8_t active = find_active_block (dev);
    if (active == 0xFFu) {
        LOG ("Активный блок не найден, форматирование...");
        uint8_t r = eeprom_format (dev);
        if (r != EEPROM_OK) {
            dev->initialized = false;
            return r;
        }
    } else {
        LOG ("Активный блок %d", active);
    }
    return EEPROM_OK;
}

void eeprom_deinit (EEPROM_Dev *dev) {
    if (!dev)
        return;
    dev->initialized = false;
}

uint8_t eeprom_format (EEPROM_Dev *dev) {
    if (!dev || !dev->initialized)
        return EEPROM_ERROR_NOT_INIT;

    LOG ("Форматирование всей области EEPROM (размер %u)", EEPROM_TOTAL_SIZE);

    // Erase all blocks
    for (uint8_t b = 0; b < NUM_BLOCKS; b++) {
        uint32_t addr = dev->base_address + (b * BLOCK_SIZE);
        uint8_t res = flash_erase_block_64 (addr);
        if (res != EEPROM_OK) {
            LOG ("Сбой при удалении блока %d: %d", b, res);
            return res;
        }
    }

    // Initialize block 0 header
    uint32_t block0 = dev->base_address;
    if (flash_write_halfword (block0, (uint16_t)BLOCK_MARKER) != EEPROM_OK)
        return EEPROM_ERROR_FLASH_WRITE;

    // pack sequence (0) and status active (0xAA) into halfword: seq in high byte, status in low
    uint16_t seq_status = ((uint16_t)0 << 8) | (uint16_t)STATUS_ACTIVE;
    if (flash_write_halfword (block0 + 2u, seq_status) != EEPROM_OK)
        return EEPROM_ERROR_FLASH_WRITE;

    dev->write_count = 0;
    dev->wear_level_counter = 0;

    LOG ("Формат выполнен: блок 0 активен");
    return EEPROM_OK;
}

uint8_t eeprom_saveByte (EEPROM_Dev *dev, uint8_t id, uint8_t value) {
    if (!dev || !dev->initialized)
        return EEPROM_ERROR_NOT_INIT;

    if (id == 0 || id == 0xFF)
        return EEPROM_ERROR_INVALID_ID;

    LOG ("Save ID=%d Value:%d", id, value);

    uint8_t active = find_active_block (dev);

    if (active == 0xFFu)
        return EEPROM_ERROR_NOT_INIT;

    uint32_t active_addr = dev->base_address + (active * BLOCK_SIZE);

    // If variable exists, mark old as deleted (0x0000)
    uint32_t old_addr;
    if (find_variable_in_storage (dev, id, &old_addr)) {
        LOG ("Пометить старую запись 0x%08X удалено", (unsigned)old_addr);
        if (flash_write_halfword (old_addr, 0x0000u) != EEPROM_OK)
            return EEPROM_ERROR_FLASH_WRITE;
    }

    // Check free space in active block
    uint16_t free_space = get_block_free_space (active_addr);
    LOG ("Свободное место в блоке %d: %u байт", active, free_space);
    if (free_space < VARIABLE_SIZE) {
        // Need compaction
        LOG ("Недостаточно места, уплотнение из блока %d...", active);
        uint8_t r = compact_to_new_block (dev, active);
        if (r != EEPROM_OK) {
            LOG ("Уплотнение не удалось: %d", r);
            return r;
        }
        active = find_active_block (dev);
        if (active == 0xFFu)
            return EEPROM_ERROR_STORAGE_FULL;
        active_addr = dev->base_address + (active * BLOCK_SIZE);
        LOG ("Новый активный блок %d", active);
    }

    // find write address inside active block
    uint32_t write_addr = active_addr + BLOCK_HEADER_SIZE;
    uint32_t block_end = active_addr + BLOCK_SIZE;
    while (write_addr <= block_end - VARIABLE_SIZE) {
        uint8_t stored_id = *(volatile uint8_t *)write_addr;
        if (stored_id == 0xFFu)
            break;
        write_addr += VARIABLE_SIZE;
    }

    if (write_addr > block_end - VARIABLE_SIZE) {
        LOG ("Storage full after scan (shouldn't happen)");
        return EEPROM_ERROR_STORAGE_FULL;
    }

    uint16_t word = ((uint16_t)value << 8) | (uint16_t)id;
    if (flash_write_halfword (write_addr, word) != EEPROM_OK)
        return EEPROM_ERROR_FLASH_WRITE;

    dev->write_count++;
    // last_write_time left as-is (SysTick not referenced here)

    LOG ("Записан в 0x%08X: 0x%04X", (unsigned)write_addr, word);
    return EEPROM_OK;
}

uint8_t eeprom_readByte (EEPROM_Dev *dev, uint8_t id, uint8_t *value) {
    if (!dev || !dev->initialized || !value)
        return EEPROM_ERROR_INVALID_PARAM;

    uint32_t addr;
    if (!find_variable_in_storage (dev, id, &addr))
        return EEPROM_ERROR_VAR_NOT_FOUND;

    uint16_t word = *(volatile uint16_t *)addr;
    *value = (uint8_t)((word >> 8) & 0xFFu);
    LOG ("Read ID=%d => %d from 0x%08X", id, *value, (unsigned)addr);
    return EEPROM_OK;
}

uint8_t eeprom_deleteVar (EEPROM_Dev *dev, uint8_t id) {
    if (!dev || !dev->initialized)
        return EEPROM_ERROR_NOT_INIT;
    uint32_t addr;
    if (!find_variable_in_storage (dev, id, &addr))
        return EEPROM_ERROR_VAR_NOT_FOUND;
    if (flash_write_halfword (addr, 0x0000u) != EEPROM_OK)
        return EEPROM_ERROR_FLASH_WRITE;
    return EEPROM_OK;
}

uint8_t eeprom_getStatus (EEPROM_Dev *dev, uint16_t *used_vars, uint16_t *free_space) {
    if (!dev || !dev->initialized)
        return EEPROM_ERROR_NOT_INIT;
    if (used_vars) {
        uint8_t seen[256] = {0};
        uint16_t count = 0;
        for (uint8_t b = 0; b < NUM_BLOCKS; b++) {
            uint32_t block_addr = dev->base_address + (b * BLOCK_SIZE);
            uint16_t marker = *(volatile uint16_t *)block_addr;
            if (marker != BLOCK_MARKER)
                continue;
            uint32_t addr = block_addr + BLOCK_HEADER_SIZE;
            uint32_t end = block_addr + BLOCK_SIZE;
            while (addr <= end - VARIABLE_SIZE) {
                uint8_t sid = *(volatile uint8_t *)addr;
                if (sid == 0xFFu)
                    break;
                if (sid != 0u && !seen[sid]) {
                    seen[sid] = 1;
                    count++;
                }
                addr += VARIABLE_SIZE;
            }
        }
        *used_vars = count;
    }
    if (free_space) {
        uint8_t active = find_active_block (dev);
        if (active == 0xFFu)
            *free_space = 0;
        else {
            uint32_t block_addr = dev->base_address + (active * BLOCK_SIZE);
            *free_space = get_block_free_space (block_addr);
        }
    }
    return EEPROM_OK;
}

uint8_t eeprom_compact (EEPROM_Dev *dev) {
    if (!dev || !dev->initialized)
        return EEPROM_ERROR_NOT_INIT;
    uint8_t active = find_active_block (dev);
    if (active == 0xFFu)
        return EEPROM_ERROR_NOT_INIT;
    return compact_to_new_block (dev, active);
}

/* Legacy API */
void EEPROM_init (void) { eeprom_init (&legacy_device, 0x08003C00u); /* default example address */ }

uint8_t EEPROM_format (void) { return eeprom_format (&legacy_device); }

uint8_t EEPROM_saveByte (uint8_t id, uint8_t value) { return eeprom_saveByte (&legacy_device, id, value); }

int16_t EEPROM_readByte (uint8_t id) {
    uint8_t v;
    return (eeprom_readByte (&legacy_device, id, &v) == EEPROM_OK) ? v : -1;
}

uint8_t EEPROM_varExists (uint8_t id) {
    uint32_t a;
    return find_variable_in_storage (&legacy_device, id, &a) ? 1 : 0;
}

uint8_t EEPROM_deleteVar (uint8_t id) { return eeprom_deleteVar (&legacy_device, id); }

uint8_t EEPROM_compact (void) { return eeprom_compact (&legacy_device); }

/* ---------------------- Internal helpers ---------------------- */

static uint8_t find_active_block (EEPROM_Dev *dev) {
    uint8_t active = 0xFFu;
    int32_t best_seq = -32768;  // signed for wrap handling

    for (uint8_t i = 0; i < NUM_BLOCKS; i++) {
        uint32_t addr = dev->base_address + (i * BLOCK_SIZE);

        uint16_t marker = *(volatile uint16_t *)addr;

        if (marker != BLOCK_MARKER)
            continue;

        uint16_t seq_status = *(volatile uint16_t *)(addr + 2u);
        uint8_t seq = (uint8_t)((seq_status >> 8) & 0xFFu);
        uint8_t status = (uint8_t)(seq_status & 0xFFu);
        if (status != STATUS_ACTIVE)
            continue;

        // choose block with maximum sequence (with wrap handling)
        int32_t s = (int32_t)seq;
        if (active == 0xFFu) {
            active = i;
            best_seq = s;
        } else {
            int32_t diff = (s - best_seq);
            // normalize diff in signed 8-bit range
            if (diff > 127)
                diff -= 256;
            if (diff < -127)
                diff += 256;
            if (diff > 0) {
                active = i;
                best_seq = s;
            }
        }
    }
    return active;
}

static bool find_variable_in_storage (EEPROM_Dev *dev, uint8_t id, uint32_t *address) {
    if (!dev)
        return false;
    uint32_t last_found = 0;
    bool found = false;
    int32_t best_seq = -32768;

    // Iterate all blocks and keep last by block sequence and position
    for (uint8_t b = 0; b < NUM_BLOCKS; b++) {
        uint32_t block_addr = dev->base_address + (b * BLOCK_SIZE);
        uint16_t marker = *(volatile uint16_t *)block_addr;
        if (marker != BLOCK_MARKER)
            continue;
        uint16_t seq_status = *(volatile uint16_t *)(block_addr + 2u);
        uint8_t seq = (uint8_t)((seq_status >> 8) & 0xFFu);
        // scan inside block for last occurance
        uint32_t addr = block_addr + BLOCK_HEADER_SIZE;
        uint32_t block_end = block_addr + BLOCK_SIZE;
        uint32_t block_last = 0;
        bool block_has = false;
        while (addr <= block_end - VARIABLE_SIZE) {
            uint8_t sid = *(volatile uint8_t *)addr;
            if (sid == 0xFFu)
                break;
            if (sid == id && sid != 0u) {
                block_has = true;
                block_last = addr;
            }
            addr += VARIABLE_SIZE;
        }
        if (block_has) {
            if (!found) {
                found = true;
                best_seq = (int32_t)seq;
                last_found = block_last;
            } else {
                int32_t diff = (int32_t)seq - best_seq;
                if (diff > 127)
                    diff -= 256;
                if (diff < -127)
                    diff += 256;
                if (diff > 0) {
                    best_seq = (int32_t)seq;
                    last_found = block_last;
                }
            }
        }
    }
    if (found && address)
        *address = last_found;
    return found;
}

static uint16_t get_block_free_space (uint32_t block_addr) {
    uint32_t addr = block_addr + BLOCK_HEADER_SIZE;
    uint32_t end = block_addr + BLOCK_SIZE;
    uint16_t used = BLOCK_HEADER_SIZE;
    while (addr <= end - VARIABLE_SIZE) {
        uint8_t sid = *(volatile uint8_t *)addr;
        if (sid == 0xFFu)
            break;
        used += VARIABLE_SIZE;
        addr += VARIABLE_SIZE;
    }
    return (uint16_t)(BLOCK_SIZE - used);
}

/* Compaction strategy:
 * - Find a target block that is erased (marker != BLOCK_MARKER) or obsolete (status==0x00)
 * - If none available -> return STORAGE_FULL
 * - Write header (marker, new_sequence, STATUS_ACTIVE)
 * - Copy latest values of unique IDs from all blocks ordered by descending sequence
 * - Mark ALL processed blocks obsolete after copying is complete
 */
static uint8_t compact_to_new_block (EEPROM_Dev *dev, uint8_t current_block) {
    
    uint32_t curr_addr = dev->base_address + (current_block * BLOCK_SIZE);
    uint16_t curr_marker = *(volatile uint16_t *)curr_addr;
    if (curr_marker != BLOCK_MARKER)
        return EEPROM_ERROR_NOT_INIT;

    uint16_t curr_seq_status = *(volatile uint16_t *)(curr_addr + 2u);
    uint8_t curr_seq = (uint8_t)((curr_seq_status >> 8) & 0xFFu);

    LOG ("Compaction: curr block %d seq %d", current_block, curr_seq);

    // find erased or obsolete block
    int target = -1;
    for (uint8_t i = 1; i <= NUM_BLOCKS; i++) {
        uint8_t b = (current_block + i) % NUM_BLOCKS;
        uint32_t addr = dev->base_address + (b * BLOCK_SIZE);
        uint16_t marker = *(volatile uint16_t *)addr;
        uint16_t seq_status = *(volatile uint16_t *)(addr + 2u);
        uint8_t status = (uint8_t)(seq_status & 0xFFu);
        if (marker != BLOCK_MARKER || status == STATUS_OBSOLETE) {
            target = b;
            break;
        }
    }
    if (target < 0) {
        LOG ("No target block available for compaction -> STORAGE FULL");
        return EEPROM_ERROR_STORAGE_FULL;
    }

    uint32_t target_addr = dev->base_address + (target * BLOCK_SIZE);
    LOG ("Уплотнение: целевой блок %d", target);

    // Ensure target erased; if not erased, erase it
    uint16_t tmarker = *(volatile uint16_t *)target_addr;
    uint16_t tseq_status = *(volatile uint16_t *)(target_addr + 2u);
    uint8_t tseq = (uint8_t)((tseq_status >> 8) & 0xFFu);
    uint8_t tstatus = (uint8_t)(tseq_status & 0xFFu);
    LOG ("Уплотнение: tmarker: %04X, seq: %d, status: %02X", tmarker, tseq, tstatus);

    if (tmarker == BLOCK_MARKER) {
        // if it's marked (contains data) but status == OBSOLETE we can reuse without erase
        uint16_t seq_status = *(volatile uint16_t *)(target_addr + 2u);
        uint8_t status = (uint8_t)(seq_status & 0xFFu);
        if (status != STATUS_OBSOLETE) {
            // Shouldn't happen because we filtered earlier, but safe-guard
            LOG ("Target block not obsolete nor erased - cannot use");
            return EEPROM_ERROR_STORAGE_FULL;
        }
        // Erase to be safe
        if (flash_erase_block_64 (target_addr) != EEPROM_OK) {
            LOG ("Erase target block %d failed", target);
            return EEPROM_ERROR_FLASH_ERASE;
        }
    } else {
        // erased already - nothing to do
    }

    // write header for new block: marker + (sequence+1, STATUS_ACTIVE)
    uint8_t new_seq = (uint8_t)(curr_seq + 1u);
    if (flash_write_halfword (target_addr, (uint16_t)BLOCK_MARKER) != EEPROM_OK)
        return EEPROM_ERROR_FLASH_WRITE;

    uint16_t seq_status = ((uint16_t)new_seq << 8) | (uint16_t)STATUS_ACTIVE;
    if (flash_write_halfword (target_addr + 2u, seq_status) != EEPROM_OK)
        return EEPROM_ERROR_FLASH_WRITE;

    // Copy latest unique IDs: walk blocks by descending sequence
    // Keep track of which blocks we processed for later marking as obsolete
    bool copied[256];
    uint8_t processed_blocks[NUM_BLOCKS];
    uint8_t processed_count = 0;
    memset (copied, 0, sizeof (copied));
    uint32_t write_pos = target_addr + BLOCK_HEADER_SIZE;
    uint8_t copied_count = 0;

    for (int pass = 0; pass < NUM_BLOCKS; pass++) {
        // find block with highest sequence among unprocessed blocks
        int8_t sel = -1;
        uint8_t max_seq = 0;
        bool first = true;
        
        for (uint8_t b = 0; b < NUM_BLOCKS; b++) {
            if ((int)b == target)
                continue;  // skip target
            uint32_t ba = dev->base_address + (b * BLOCK_SIZE);
            uint16_t m = *(volatile uint16_t *)ba;
            if (m != BLOCK_MARKER)
                continue;
            uint16_t ss = *(volatile uint16_t *)(ba + 2u);
            uint8_t s = (uint8_t)((ss >> 8) & 0xFFu);
            uint8_t st = (uint8_t)(ss & 0xFFu);
            
            LOG ("  Проверка блока %d: seq=%d, status=%02X", b, s, st);
            
            // check not obsolete
            if (st == STATUS_OBSOLETE) {
                LOG ("  -> Пропущен (obsolete)");
                continue;
            }
            
            // check if already processed in previous passes
            bool already_processed = false;
            for (uint8_t p = 0; p < processed_count; p++) {
                if (processed_blocks[p] == b) {
                    already_processed = true;
                    break;
                }
            }
            if (already_processed) {
                LOG ("  -> Пропущен (уже обработан)");
                continue;
            }
            
            if (first) {
                sel = b;
                max_seq = s;
                first = false;
                LOG ("  -> Выбран как первый кандидат");
            } else {
                // Handle wrap-around: treat as signed 8-bit comparison
                int16_t diff = (int16_t)((int8_t)s - (int8_t)max_seq);
                LOG ("  -> Сравнение: diff=%d", diff);
                if (diff > 0) {
                    sel = b;
                    max_seq = s;
                    LOG ("  -> Выбран как лучший кандидат");
                }
            }
        }
        if (sel < 0)
            break;

        LOG ("Уплотнение: блок процесса %d (последовательность %d)", sel, max_seq);
        
        // Mark this block as processed
        processed_blocks[processed_count++] = (uint8_t)sel;
        
        uint32_t ba = dev->base_address + (sel * BLOCK_SIZE);
        uint32_t addr = ba + BLOCK_HEADER_SIZE;
        uint32_t end = ba + BLOCK_SIZE;
        while (addr <= end - VARIABLE_SIZE) {
            uint8_t sid = *(volatile uint8_t *)addr;
            if (sid == 0xFFu)
                break;
            if (sid != 0u && !copied[sid]) {
                uint16_t word = *(volatile uint16_t *)addr;
                if (write_pos + VARIABLE_SIZE <= target_addr + BLOCK_SIZE) {
                    if (flash_write_halfword (write_pos, word) != EEPROM_OK)
                        return EEPROM_ERROR_FLASH_WRITE;
                    write_pos += VARIABLE_SIZE;
                    copied[sid] = true;
                    copied_count++;
                } else {
                    // target full -> stop copying
                    LOG ("Целевой блок заполнен во время копирования");
                    break;
                }
            }
            addr += VARIABLE_SIZE;
        }
    }

    // NOW mark all processed blocks as obsolete (after all copying is done)
    for (uint8_t p = 0; p < processed_count; p++) {
        uint8_t b = processed_blocks[p];
        uint32_t ba = dev->base_address + (b * BLOCK_SIZE);
        uint16_t ss = *(volatile uint16_t *)(ba + 2u);
        uint8_t seq = (uint8_t)((ss >> 8) & 0xFFu);
        uint16_t new_ss = ((uint16_t)seq << 8) | (uint16_t)STATUS_OBSOLETE;

        if (flash_write_halfword (ba + 2u, new_ss) != EEPROM_OK)
            return EEPROM_ERROR_FLASH_WRITE;
        
        LOG ("Блок %d помечен как obsolete", b);
    }

    dev->wear_level_counter++;
    LOG ("Уплотнение выполнено: скопировано %d переменных, новый блок %d seq %d", copied_count, target, new_seq);
    return EEPROM_OK;
}

/* ---------------------- Flash operations using WCH HAL ---------------------- */
static uint8_t flash_erase_block_64 (uint32_t address) {
    /* Ensure address is aligned to 64-byte page boundary. Some HAL implementations expect
     * Page_Address to be page-aligned; passing an unaligned address may erase a larger
     * region (e.g. a 1KB sector) depending on device configuration. Aligning prevents
     * accidental erasure of adjacent blocks.
     */
    uint32_t page_addr = address & ~(uint32_t)(BLOCK_SIZE - 1u);

    FLASH_Unlock();

    FLASH_Status st = FLASH_ErasePage (page_addr);
    if (st != FLASH_COMPLETE) {
        FLASH_Lock();
        if (st == FLASH_TIMEOUT)
            return EEPROM_ERROR_TIMEOUT;
        return EEPROM_ERROR_FLASH_ERASE;
    }

    /* verify erase: check first word of the page */
    if (*(volatile uint32_t *)page_addr != 0xFFFFFFFFu) {
        FLASH_Lock();
        return EEPROM_ERROR_FLASH_ERASE;
    }

    FLASH_Lock();
    return EEPROM_OK;
}

static uint8_t flash_write_halfword (uint32_t address, uint16_t data) {

    FLASH_Unlock();

    FLASH_Status st = FLASH_ProgramHalfWord (address, data);
    if (st != FLASH_COMPLETE) {
        FLASH_Lock();
        if (st == FLASH_TIMEOUT)
            return EEPROM_ERROR_TIMEOUT;
        return EEPROM_ERROR_FLASH_WRITE;
    }

    /* verify */
    if (*(volatile uint16_t *)address != data) {
        FLASH_Lock();
        return EEPROM_ERROR_FLASH_VERIFY;
    }

    FLASH_Lock();
    return EEPROM_OK;
}

/* End of file */
