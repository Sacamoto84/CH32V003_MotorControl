/**
 * EEPROM Emulation for CH32V003
 * Ported from STM32 AN3969 implementation
 * 
 * Uses 2 flash pages (2KB total) for wear leveling
 * Key-Value storage: 16-bit address + 16-bit data
 * 
 * Configuration:
 * - Page 0: 0x08003C00 (last 1KB page)
 * - Page 1: 0x08004000 (requires 2KB reserved, or use different addresses)
 * 
 * Note: CH32V003 has 1KB pages. Adjust EEPROM_PAGE_SIZE if needed.
 */

#ifndef __EEPROM_CH32V_H
#define __EEPROM_CH32V_H

#include <stdint.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif

#include "ch32v00x.h"

/* Configuration - CH32V003 specific */
#define EEPROM_PAGE_SIZE        1024u           // CH32V003: 1KB pages
#define EEPROM_START_ADDRESS    0x08003800u     // Last 2KB of 16KB flash
#define EEPROM_PAGE0_BASE       ((uint32_t)(EEPROM_START_ADDRESS))
#define EEPROM_PAGE1_BASE       ((uint32_t)(EEPROM_START_ADDRESS + EEPROM_PAGE_SIZE))

/* Page status definitions */
#define EEPROM_ERASED           ((uint16_t)0xFFFF)  /* PAGE is empty */
#define EEPROM_RECEIVE_DATA     ((uint16_t)0xEEEE)  /* PAGE is marked to receive data */
#define EEPROM_VALID_PAGE       ((uint16_t)0x0000)  /* PAGE containing valid data */

/* Default data value */
#define EEPROM_DEFAULT_DATA     0xFFFF

/* Return codes */
typedef enum {
    EEPROM_OK = 0,
    EEPROM_OUT_SIZE = 0x81,
    EEPROM_BAD_ADDRESS = 0x82,
    EEPROM_BAD_FLASH = 0x83,
    EEPROM_NOT_INIT = 0x84,
    EEPROM_SAME_VALUE = 0x85,
    EEPROM_NO_VALID_PAGE = 0xAB,
    EEPROM_FLASH_ERROR = 0xFF
} EEPROM_Status;

/* EEPROM Handle structure */
typedef struct {
    uint32_t PageBase0;
    uint32_t PageBase1;
    uint32_t PageSize;
    uint16_t Status;
} EEPROM_HandleTypeDef;

// /* Forward declaration for C++ compatibility */
// #ifdef __cplusplus
// typedef struct EEPROM_HandleTypeDef EEPROM_HandleTypeDef;
// #endif

/* Public API */
uint16_t EEPROM_Init(EEPROM_HandleTypeDef *heeprom);
uint16_t EEPROM_Format(EEPROM_HandleTypeDef *heeprom);
uint16_t EEPROM_Read(EEPROM_HandleTypeDef *heeprom, uint16_t Address, uint16_t *Data);
uint16_t EEPROM_Write(EEPROM_HandleTypeDef *heeprom, uint16_t Address, uint16_t Data);
uint16_t EEPROM_Update(EEPROM_HandleTypeDef *heeprom, uint16_t Address, uint16_t Data);
uint16_t EEPROM_Count(EEPROM_HandleTypeDef *heeprom, uint16_t *Count);
uint16_t EEPROM_MaxCount(EEPROM_HandleTypeDef *heeprom);
uint16_t EEPROM_Erases(EEPROM_HandleTypeDef *heeprom, uint16_t *Erases);

/* Helper macro for default handle initialization */
#define EEPROM_HANDLE_DEFAULT() { \
    .PageBase0 = EEPROM_PAGE0_BASE, \
    .PageBase1 = EEPROM_PAGE1_BASE, \
    .PageSize = EEPROM_PAGE_SIZE, \
    .Status = EEPROM_NOT_INIT \
}

#ifdef __cplusplus
}
#endif

#endif /* __EEPROM_CH32V_H */