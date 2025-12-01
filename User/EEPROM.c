/******************************************************************************
 * EEPROM.c - Enhanced Flash Storage Library for CH32V003J4M6
 *
 * Created by  Shakir Abdo
 * Enhanced with GPIO integration and comprehensive error handling
 ******************************************************************************/

#include "EEPROM.h"
#include <stdbool.h>
#include <string.h>

// Legacy compatibility functions
static EEPROM_Dev legacy_device;  // Global device for legacy API

#define MCU_CORE_CLOCK FUNCONF_SYSTEM_CORE_CLOCK

// Flash keys for unlocking
#define FLASH_KEY1 ((uint32_t)0x45670123)
#define FLASH_KEY2 ((uint32_t)0xCDEF89AB)

// Internal constants
#define VARIABLE_HEADER_SIZE 6      // ID(1) + Length(1) + Value(2) + CRC(2)
#define MAX_STRING_LENGTH 32        // Maximum string variable length
#define FLASH_TIMEOUT_CYCLES 50000  // Flash operation timeout
#define WEAR_LEVEL_THRESHOLD 100    // Wear leveling threshold

// Variable entry structure in flash
typedef struct {
    uint8_t id;          // Variable ID
    uint8_t length;      // Data length (1=uint16_t, 2=uint32_t, 3+=string)
    uint16_t data_low;   // Lower 16 bits of data
    uint16_t data_high;  // Upper 16 bits of data (for 32-bit) or CRC
} __attribute__ ((packed)) VariableEntry;

// Forward declarations
static uint8_t flash_unlock (void);
static void flash_lock (void);
static uint8_t flash_wait_for_operation (void);
static uint8_t flash_erase_page (uint32_t address);
static uint8_t flash_write_halfword (uint32_t address, uint16_t data);
static uint16_t calculate_crc16 (const uint8_t *data, uint8_t length);
static uint8_t find_variable (EEPROM_Dev *dev, uint8_t id, uint32_t *address);
static uint8_t count_variables (EEPROM_Dev *dev);

// uint8_t get_free_space();

// Initialize EEPROM storage system
uint8_t eeprom_init (EEPROM_Dev *dev, uint32_t base_address) {
    if (!dev) {
        return EEPROM_ERROR_INVALID_PARAM;
    }

    // Clean up previous initialization
    if (dev->initialized) {
        eeprom_deinit (dev);
    }

    // Validate base address (must be page-aligned)
    if (base_address & (EEPROM_PAGE_SIZE - 1)) {
        return EEPROM_ERROR_INVALID_PARAM;
    }

    // Initialize device structure
    dev->base_address = base_address;
    dev->max_variables = EEPROM_MAX_VARIABLES;
    dev->wear_level_counter = 0;
    dev->last_write_time = 0;
    dev->write_count = 0;
    dev->initialized = true;

    // Check if EEPROM is already formatted
    uint32_t *marker_ptr = (uint32_t *)base_address;
    if (*marker_ptr != EEPROM_MARKER) {
        // Not formatted, format it now
        uint8_t result = eeprom_format (dev);
        if (result != EEPROM_OK) {
            dev->initialized = false;
            return result;
        }
    }

    return EEPROM_OK;
}

// Deinitialize EEPROM and release resources
void eeprom_deinit (EEPROM_Dev *dev) {
    if (!dev || !dev->initialized) {
        return;
    }

    // Reset device structure
    dev->base_address = 0;
    dev->max_variables = 0;
    dev->wear_level_counter = 0;
    dev->last_write_time = 0;
    dev->write_count = 0;
    dev->initialized = false;
}

// Check if EEPROM is initialized
bool eeprom_isInitialized (EEPROM_Dev *dev) {
    return (dev && dev->initialized);
}

// Format EEPROM storage (erase and initialize)
uint8_t eeprom_format (EEPROM_Dev *dev) {
    if (!dev || !dev->initialized) {
        return EEPROM_ERROR_NOT_INIT;
    }

    // Erase the flash page
    uint8_t result = flash_erase_page (dev->base_address);
    if (result != EEPROM_OK) {
        return result;
    }

    // Write EEPROM marker
    result = flash_write_halfword (dev->base_address, EEPROM_MARKER & 0xFFFF);
    if (result != EEPROM_OK) {
        return result;
    }

    result = flash_write_halfword (dev->base_address + 2, (EEPROM_MARKER >> 16) & 0xFFFF);
    if (result != EEPROM_OK) {
        return result;
    }

    // Write version and reserved space
    result = flash_write_halfword (dev->base_address + 4, EEPROM_VERSION);
    if (result != EEPROM_OK) {
        return result;
    }

    result = flash_write_halfword (dev->base_address + 6, 0x0000);  // Reserved
    if (result != EEPROM_OK) {
        return result;
    }

    // Reset counters
    dev->write_count = 0;
    dev->wear_level_counter = 0;

    return EEPROM_OK;
}

// Save 16-bit variable
uint8_t eeprom_saveVar (EEPROM_Dev *dev, uint8_t id, uint16_t value) {
    if (!dev || !dev->initialized) {
        return EEPROM_ERROR_NOT_INIT;
    }

    if (id == 0) {
        return EEPROM_ERROR_INVALID_ID;
    }

    // Check if we have space (simple check)
    uint8_t var_count = count_variables (dev);
    if (var_count >= dev->max_variables) {
        // Try compaction
        uint8_t result = eeprom_compact (dev);
        if (result != EEPROM_OK) {
            return EEPROM_ERROR_STORAGE_FULL;
        }
    }

    // Find next free slot
    uint32_t write_address = dev->base_address + 8;  // After header

    // Skip existing variables
    while (write_address < (dev->base_address + EEPROM_PAGE_SIZE - VARIABLE_HEADER_SIZE)) {
        uint8_t stored_id = *(volatile uint8_t *)write_address;
        if (stored_id == 0xFF) {
            break;  // Found free space
        }

        uint8_t length = *(volatile uint8_t *)(write_address + 1);
        if (length == 0)
            length = 1;  // Minimum size
        write_address += VARIABLE_HEADER_SIZE;
    }

    if (write_address >= (dev->base_address + EEPROM_PAGE_SIZE - VARIABLE_HEADER_SIZE)) {
        return EEPROM_ERROR_STORAGE_FULL;
    }

    // Calculate CRC
    uint8_t data_bytes[3] = {id, 1, (uint8_t)(value & 0xFF)};
    data_bytes[2] = (value >> 8) & 0xFF;
    uint16_t crc = calculate_crc16 (data_bytes, 3);

    // Write variable entry
    uint8_t result;
    result = flash_write_halfword (write_address, (uint16_t)((1 << 8) | id));  // Length + ID
    if (result != EEPROM_OK)
        return result;

    result = flash_write_halfword (write_address + 2, value);  // Value
    if (result != EEPROM_OK)
        return result;

    result = flash_write_halfword (write_address + 4, crc);  // CRC
    if (result != EEPROM_OK)
        return result;

    // Update statistics
    dev->write_count++;
    dev->last_write_time = SysTick->CNT;

    return EEPROM_OK;
}

// Read 16-bit variable
uint8_t eeprom_readVar (EEPROM_Dev *dev, uint8_t id, uint16_t *value) {
    if (!dev || !dev->initialized || !value) {
        return EEPROM_ERROR_INVALID_PARAM;
    }

    uint32_t address;
    if (!find_variable (dev, id, &address)) {
        return EEPROM_ERROR_VAR_NOT_FOUND;
    }

    // Read the value
    *value = *(volatile uint16_t *)(address + 2);

    // Verify CRC
    uint8_t data_bytes[3] = {id, 1, (uint8_t)(*value & 0xFF)};
    data_bytes[2] = (*value >> 8) & 0xFF;
    uint16_t expected_crc = calculate_crc16 (data_bytes, 3);
    uint16_t stored_crc = *(volatile uint16_t *)(address + 4);

    if (expected_crc != stored_crc) {
        return EEPROM_ERROR_CRC_MISMATCH;
    }

    return EEPROM_OK;
}

// Save 32-bit variable
uint8_t eeprom_saveVar32 (EEPROM_Dev *dev, uint8_t id, uint32_t value) {
    if (!dev || !dev->initialized) {
        return EEPROM_ERROR_NOT_INIT;
    }

    if (id == 0) {
        return EEPROM_ERROR_INVALID_ID;
    }

    // Find next free slot
    uint32_t write_address = dev->base_address + 8;  // After header

    // Skip existing variables
    while (write_address < (dev->base_address + EEPROM_PAGE_SIZE - 8)) {  // Need 8 bytes for 32-bit
        uint8_t stored_id = *(volatile uint8_t *)write_address;
        if (stored_id == 0xFF) {
            break;  // Found free space
        }

        write_address += VARIABLE_HEADER_SIZE;
    }

    if (write_address >= (dev->base_address + EEPROM_PAGE_SIZE - 8)) {
        return EEPROM_ERROR_STORAGE_FULL;
    }

    // Calculate CRC
    uint8_t data_bytes[5] = {id, 2,
                             (uint8_t)(value & 0xFF),
                             (uint8_t)((value >> 8) & 0xFF),
                             (uint8_t)((value >> 16) & 0xFF)};
    uint16_t crc = calculate_crc16 (data_bytes, 5);

    // Write variable entry
    uint8_t result;
    result = flash_write_halfword (write_address, (uint16_t)((2 << 8) | id));  // Length + ID
    if (result != EEPROM_OK)
        return result;

    result = flash_write_halfword (write_address + 2, value & 0xFFFF);  // Low word
    if (result != EEPROM_OK)
        return result;

    result = flash_write_halfword (write_address + 4, (value >> 16) & 0xFFFF);  // High word
    if (result != EEPROM_OK)
        return result;

    result = flash_write_halfword (write_address + 6, crc);  // CRC
    if (result != EEPROM_OK)
        return result;

    // Update statistics
    dev->write_count++;
    dev->last_write_time = SysTick->CNT;

    return EEPROM_OK;
}

// Read 32-bit variable
uint8_t eeprom_readVar32 (EEPROM_Dev *dev, uint8_t id, uint32_t *value) {
    if (!dev || !dev->initialized || !value) {
        return EEPROM_ERROR_INVALID_PARAM;
    }

    uint32_t address;
    if (!find_variable (dev, id, &address)) {
        return EEPROM_ERROR_VAR_NOT_FOUND;
    }

    // Check if it's actually a 32-bit variable
    uint8_t length = *(volatile uint8_t *)(address + 1);
    if (length != 2) {
        return EEPROM_ERROR_INVALID_PARAM;
    }

    // Read the value
    uint16_t low_word = *(volatile uint16_t *)(address + 2);
    uint16_t high_word = *(volatile uint16_t *)(address + 4);
    *value = ((uint32_t)high_word << 16) | low_word;

    // Verify CRC
    uint8_t data_bytes[5] = {id, 2,
                             (uint8_t)(*value & 0xFF),
                             (uint8_t)((*value >> 8) & 0xFF),
                             (uint8_t)((*value >> 16) & 0xFF)};
    uint16_t expected_crc = calculate_crc16 (data_bytes, 5);
    uint16_t stored_crc = *(volatile uint16_t *)(address + 6);

    if (expected_crc != stored_crc) {
        return EEPROM_ERROR_CRC_MISMATCH;
    }

    return EEPROM_OK;
}

// Save string variable
uint8_t eeprom_saveString (EEPROM_Dev *dev, uint8_t id, const char *str, uint8_t max_len) {
    if (!dev || !dev->initialized || !str) {
        return EEPROM_ERROR_INVALID_PARAM;
    }

    if (id == 0 || max_len > MAX_STRING_LENGTH) {
        return EEPROM_ERROR_INVALID_ID;
    }

    uint8_t str_len = strlen (str);
    if (str_len > max_len)
        str_len = max_len;

    // Find next free slot (need variable space for string)
    uint32_t needed_space = 6 + ((str_len + 1) & ~1);  // Round up to even bytes
    uint32_t write_address = dev->base_address + 8;

    while (write_address < (dev->base_address + EEPROM_PAGE_SIZE - needed_space)) {
        uint8_t stored_id = *(volatile uint8_t *)write_address;
        if (stored_id == 0xFF) {
            break;  // Found free space
        }
        write_address += VARIABLE_HEADER_SIZE;
    }

    if (write_address >= (dev->base_address + EEPROM_PAGE_SIZE - needed_space)) {
        return EEPROM_ERROR_STORAGE_FULL;
    }

    // Write header
    uint8_t result;
    result = flash_write_halfword (write_address, (uint16_t)((str_len << 8) | id));
    if (result != EEPROM_OK)
        return result;

    // Write string data (in 16-bit chunks)
    for (uint8_t i = 0; i < str_len; i += 2) {
        uint16_t word_data = str[i];
        if (i + 1 < str_len) {
            word_data |= ((uint16_t)str[i + 1] << 8);
        }

        result = flash_write_halfword (write_address + 2 + i, word_data);
        if (result != EEPROM_OK)
            return result;
    }

    // Calculate and write CRC
    uint16_t crc = calculate_crc16 ((const uint8_t *)str, str_len);
    result = flash_write_halfword (write_address + 2 + ((str_len + 1) & ~1), crc);
    if (result != EEPROM_OK)
        return result;

    dev->write_count++;
    dev->last_write_time = SysTick->CNT;

    return EEPROM_OK;
}

// Read string variable
uint8_t eeprom_readString (EEPROM_Dev *dev, uint8_t id, char *str, uint8_t max_len) {
    if (!dev || !dev->initialized || !str) {
        return EEPROM_ERROR_INVALID_PARAM;
    }

    uint32_t address;
    if (!find_variable (dev, id, &address)) {
        return EEPROM_ERROR_VAR_NOT_FOUND;
    }

    uint8_t stored_len = *(volatile uint8_t *)(address + 1);
    if (stored_len > max_len - 1) {
        stored_len = max_len - 1;  // Truncate if necessary
    }

    // Read string data
    for (uint8_t i = 0; i < stored_len; i += 2) {
        uint16_t word_data = *(volatile uint16_t *)(address + 2 + i);
        str[i] = word_data & 0xFF;
        if (i + 1 < stored_len) {
            str[i + 1] = (word_data >> 8) & 0xFF;
        }
    }
    str[stored_len] = '\0';  // Null terminate

    return EEPROM_OK;
}

// Check if variable exists
bool eeprom_varExists (EEPROM_Dev *dev, uint8_t id) {
    uint32_t dummy_address;
    return find_variable (dev, id, &dummy_address);
}

// Delete variable
uint8_t eeprom_deleteVar (EEPROM_Dev *dev, uint8_t id) {
    if (!dev || !dev->initialized) {
        return EEPROM_ERROR_NOT_INIT;
    }

    uint32_t address;
    if (!find_variable (dev, id, &address)) {
        return EEPROM_ERROR_VAR_NOT_FOUND;
    }

    // Mark variable as deleted by setting ID to 0
    uint8_t result = flash_write_halfword (address, 0x0000);
    return result;
}

// Get storage status
uint8_t eeprom_getStatus (EEPROM_Dev *dev, uint16_t *used_vars, uint16_t *free_space) {
    if (!dev || !dev->initialized) {
        return EEPROM_ERROR_NOT_INIT;
    }

    if (used_vars) {
        *used_vars = count_variables (dev);
    }

    if (free_space) {
        *free_space = get_free_space ();
    }

    return EEPROM_OK;
}

// Compact EEPROM (defragmentation)
uint8_t eeprom_compact (EEPROM_Dev *dev) {
    if (!dev || !dev->initialized) {
        return EEPROM_ERROR_NOT_INIT;
    }

    // Simple compaction: re-write all valid variables
    // This would require a backup page in real implementation
    // For now, just return OK (placeholder)

    dev->wear_level_counter++;
    return EEPROM_OK;
}

// Backup EEPROM to another location
uint8_t eeprom_backup (EEPROM_Dev *dev, uint32_t backup_address) {
    if (!dev || !dev->initialized) {
        return EEPROM_ERROR_NOT_INIT;
    }

    // Copy entire page to backup location
    // Implementation would copy data word by word
    // Placeholder for now
    (void)backup_address;  // Suppress unused parameter warning

    return EEPROM_OK;
}

// Restore EEPROM from backup
uint8_t eeprom_restore (EEPROM_Dev *dev, uint32_t backup_address) {
    if (!dev || !dev->initialized) {
        return EEPROM_ERROR_NOT_INIT;
    }

    // Restore from backup location
    // Placeholder for now
    (void)backup_address;  // Suppress unused parameter warning

    return EEPROM_OK;
}

// Flash operation functions
static uint8_t flash_unlock (void) {
    if (FLASH->CTLR & FLASH_CTLR_LOCK) {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;

        // Verify unlock
        if (FLASH->CTLR & FLASH_CTLR_LOCK) {
            return EEPROM_ERROR_FLASH_LOCK;
        }
    }
    return EEPROM_OK;
}

static void flash_lock (void) {
    FLASH->CTLR |= FLASH_CTLR_LOCK;
}

static uint8_t flash_wait_for_operation (void) {
    uint32_t timeout = FLASH_TIMEOUT_CYCLES;

    while ((FLASH->STATR & FLASH_STATR_BSY) && timeout) {
        timeout--;
    }

    return (timeout > 0) ? EEPROM_OK : EEPROM_ERROR_TIMEOUT;
}

static uint8_t flash_erase_page (uint32_t address) {
    uint8_t result = flash_unlock();
    if (result != EEPROM_OK) {
        return result;
    }

    // Wait for any ongoing operations
    result = flash_wait_for_operation();
    if (result != EEPROM_OK) {
        flash_lock();
        return result;
    }

    // Set page erase bit
    FLASH->CTLR |= FLASH_CTLR_PER;
    // Set the address to erase
    FLASH->ADDR = address;
    // Start the erase operation
    FLASH->CTLR |= FLASH_CTLR_STRT;

    // Wait for completion
    result = flash_wait_for_operation();

    // Clear page erase bit
    FLASH->CTLR &= ~FLASH_CTLR_PER;

    // Verify erase
    if (*(volatile uint32_t *)address != 0xFFFFFFFF) {
        flash_lock();
        return EEPROM_ERROR_FLASH_ERASE;
    }

    flash_lock();
    return result;
}

static uint8_t flash_write_halfword (uint32_t address, uint16_t data) {
    uint8_t result = flash_unlock();
    if (result != EEPROM_OK) {
        return result;
    }

    // Wait for any ongoing operations
    result = flash_wait_for_operation();
    if (result != EEPROM_OK) {
        flash_lock();
        return result;
    }

    // Enable programming
    FLASH->CTLR |= FLASH_CTLR_PG;

    // Write the data
    *(volatile uint16_t *)address = data;

    // Wait for completion
    result = flash_wait_for_operation();

    // Disable programming
    FLASH->CTLR &= ~FLASH_CTLR_PG;

    // Verify the write
    if (*(volatile uint16_t *)address != data) {
        flash_lock();
        return EEPROM_ERROR_FLASH_VERIFY;
    }

    flash_lock();
    return result;
}

// Helper functions
static uint16_t calculate_crc16 (const uint8_t *data, uint8_t length) {
    uint16_t crc = 0xFFFF;

    for (uint8_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = crc >> 1;
            }
        }
    }

    return crc;
}

static uint8_t find_variable (EEPROM_Dev *dev, uint8_t id, uint32_t *address) {
    uint32_t current_addr = dev->base_address + 8;  // Skip header

    while (current_addr < (dev->base_address + EEPROM_PAGE_SIZE - VARIABLE_HEADER_SIZE)) {
        uint8_t stored_id = *(volatile uint8_t *)current_addr;

        if (stored_id == 0xFF) {
            break;  // End of data
        }

        if (stored_id == id && stored_id != 0) {  // Found and not deleted
            if (address)
                *address = current_addr;
            return 1;
        }

        // Skip this variable
        current_addr += VARIABLE_HEADER_SIZE;
    }

    return 0;  // Not found
}

static uint8_t count_variables (EEPROM_Dev *dev) {
    uint8_t count = 0;
    uint32_t current_addr = dev->base_address + 8;  // Skip header

    while (current_addr < (dev->base_address + EEPROM_PAGE_SIZE - VARIABLE_HEADER_SIZE)) {
        uint8_t stored_id = *(volatile uint8_t *)current_addr;

        if (stored_id == 0xFF) {
            break;  // End of data
        }

        if (stored_id != 0) {  // Not deleted
            count++;
        }

        current_addr += VARIABLE_HEADER_SIZE;
    }

    return count;
}

uint8_t get_free_space (void) {
    uint32_t current_addr = legacy_device.base_address + 8;  // Skip header
    uint16_t used_space = 0;

    while (current_addr < (legacy_device.base_address + EEPROM_PAGE_SIZE - VARIABLE_HEADER_SIZE)) {
        uint8_t stored_id = *(volatile uint8_t *)current_addr;

        if (stored_id == 0xFF) {
            break;  // End of data
        }

        used_space += VARIABLE_HEADER_SIZE;
        current_addr += VARIABLE_HEADER_SIZE;
    }

    return (EEPROM_PAGE_SIZE - 8 - used_space);  // Subtract header
}

void EEPROM_init (void) {
    eeprom_init (&legacy_device, EEPROM_ADDRESS);
}

uint8_t EEPROM_format (void) {
    return eeprom_format (&legacy_device);
}

uint8_t EEPROM_saveVar (uint8_t id, uint16_t value) {
    return eeprom_saveVar (&legacy_device, id, value);
}

int32_t EEPROM_readVar (uint8_t id) {
    uint16_t value;

    uint8_t res = eeprom_readVar (&legacy_device, id, &value);
    if (res == EEPROM_OK) {
        return value;
    }
    printf ("Error EEPROM_readVar code:%d ", res);
    switch (res) {
    case 0: printf ("Success"); break;
    case 1: printf ("Flash unlock/lock failed"); break;
    case 2: printf ("Flash erase failed"); break;
    case 3: printf ("Flash write failed"); break;
    case 4: printf ("Flash write verification failed"); break;
    case 5: printf ("Flash operation timeout"); break;
    case 6: printf ("EEPROM not initialized"); break;
    case 7: printf ("Variable ID not found"); break;
    case 8: printf ("CRC verification failed"); break;
    case 9: printf ("Storage capacity exceeded"); break;
    case 10: printf ("Invalid variable ID"); break;
    case 11: printf ("Invalid parameter"); break;
    }
    printf ("\n");
    return -1;  // Error value
}

uint8_t EEPROM_varExists (uint8_t id) {
    return eeprom_varExists (&legacy_device, id) ? 1 : 0;
}