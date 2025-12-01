/******************************************************************************
 * EEPROM.h - Flash Storage Library for CH32V003J4M6
 *
 * Created by  Shakir Abdo
 *
 * Simple and reliable EEPROM emulation for the CH32V003 microcontroller.
 ******************************************************************************/

//https://github.com/shakir-abdo/ch32v003-eeprom

#ifndef EEPROM_H
#define EEPROM_H

#ifdef __cplusplus
 extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "debug.h"

//#include "gpios.h"  // Include GPIO library for integration

// Error codes
#define EEPROM_OK                0  // Success
#define EEPROM_ERROR_FLASH_LOCK  1  // Flash unlock/lock failed
#define EEPROM_ERROR_FLASH_ERASE 2  // Flash erase failed
#define EEPROM_ERROR_FLASH_WRITE 3  // Flash write failed
#define EEPROM_ERROR_FLASH_VERIFY 4  // Flash write verification failed
#define EEPROM_ERROR_TIMEOUT     5  // Flash operation timeout
#define EEPROM_ERROR_NOT_INIT    6  // EEPROM not initialized
#define EEPROM_ERROR_VAR_NOT_FOUND 7  // Variable ID not found
#define EEPROM_ERROR_CRC_MISMATCH 8  // CRC verification failed
#define EEPROM_ERROR_STORAGE_FULL 9  // Storage capacity exceeded
#define EEPROM_ERROR_INVALID_ID  10  // Invalid variable ID
#define EEPROM_ERROR_INVALID_PARAM 11  // Invalid parameter

// Legacy compatibility
#define EEPROM_ERROR 1  // Legacy error code

// Configuration
#define EEPROM_ADDRESS 0x08003C00   // Flash storage address (safe page)
#define EEPROM_MAX_VARIABLES 32     // Maximum number of variables
#define EEPROM_PAGE_SIZE 1024       // Flash page size in bytes
#define EEPROM_MARKER 0x45455045    // "EEP" magic marker (32-bit)
#define EEPROM_VERSION 0x01         // EEPROM format version

// EEPROM device structure for multi-instance support
typedef struct {
    uint32_t base_address;      // Flash storage base address
    uint16_t max_variables;     // Maximum number of variables
    bool initialized;           // Initialization status
    uint8_t wear_level_counter; // Simple wear leveling counter
    uint32_t last_write_time;   // Last write operation time
    uint16_t write_count;       // Total write operations counter
} EEPROM_Dev;

// Function declarations - New GPIO-integrated API
uint8_t eeprom_init(EEPROM_Dev* dev, uint32_t base_address);
void eeprom_deinit(EEPROM_Dev* dev);
uint8_t eeprom_format(EEPROM_Dev* dev);
uint8_t eeprom_saveVar(EEPROM_Dev* dev, uint8_t id, uint16_t value);
uint8_t eeprom_readVar(EEPROM_Dev* dev, uint8_t id, uint16_t* value);
uint8_t eeprom_saveVar32(EEPROM_Dev* dev, uint8_t id, uint32_t value);
uint8_t eeprom_readVar32(EEPROM_Dev* dev, uint8_t id, uint32_t* value);
uint8_t eeprom_saveString(EEPROM_Dev* dev, uint8_t id, const char* str, uint8_t max_len);
uint8_t eeprom_readString(EEPROM_Dev* dev, uint8_t id, char* str, uint8_t max_len);
uint8_t eeprom_deleteVar(EEPROM_Dev* dev, uint8_t id);
bool eeprom_varExists(EEPROM_Dev* dev, uint8_t id);
bool eeprom_isInitialized(EEPROM_Dev* dev);
uint8_t eeprom_getStatus(EEPROM_Dev* dev, uint16_t* used_vars, uint16_t* free_space);
uint8_t eeprom_compact(EEPROM_Dev* dev);  // Defragmentation
uint8_t eeprom_backup(EEPROM_Dev* dev, uint32_t backup_address);
uint8_t eeprom_restore(EEPROM_Dev* dev, uint32_t backup_address);

extern uint8_t get_free_space();


// Legacy compatibility functions
void EEPROM_init(void);                     // Legacy - uses default address
uint8_t EEPROM_format(void);                // Legacy wrapper
uint8_t EEPROM_saveVar(uint8_t id, uint16_t value);  // Legacy wrapper
int32_t EEPROM_readVar(uint8_t id);        // Legacy wrapper
uint8_t EEPROM_varExists(uint8_t id);       // Legacy wrapper

#ifdef __cplusplus
}
#endif

#endif /* EEPROM_H */
