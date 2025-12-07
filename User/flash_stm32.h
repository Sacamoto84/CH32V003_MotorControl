#ifndef __FLASH_STM32_H
#define __FLASH_STM32_H

#include <stdint.h>


// HACK ALERT. This definition may not match your processor
// To Do. Work out correct value for EEPROM_PAGE_SIZE on the STM32F103CT6 etc 
#define FLASH_PAGE_SIZE          1024





#define EEPROM_PAGE0 FLASH_SECTOR_TOTAL - 2
#define EEPROM_PAGE1 FLASH_SECTOR_TOTAL - 1
#define EEPROM_PAGE_SIZE (uint32_t)0x20000u /*Page size 128k*/

#if FLASH_SECTOR_TOTAL == 8
/*
   * RM0368, table 5: STM32F401xE
   * RM0383, table 4: STM32F411xE
   * RM0390, table 4: STM32F446xx
   */
#define EEPROM_START_ADDRESS (uint32_t)0x08040000u  //EEPROM SECTOR 6

#elif  FLASH_SECTOR_TOTAL == 12
/*
  * RM0090, table 5: STM32F405xx, STM32F415xx, STM32F407xx, STM32F417xx
  * RM0402, table 5: STM32F412Zx, STM32F412Vx, STM32F412Rx, STM32F412Cx
  */
#define EEPROM_START_ADDRESS (uint32_t)0x080C0000u  //EEPROM SECTOR 10
#elif  FLASH_SECTOR_TOTAL == 16
/* RM0430, table 5.: STM32F413xx, STM32F423xx */
#define EEPROM_START_ADDRESS (uint32_t)0x08140000u  //EEPROM SECTOR 14
#elif FLASH_SECTOR_TOTAL == 24
/*
	 * RM0090, table 6: STM32F427xx, STM32F437xx, STM32F429xx, STM32F439xx
	 * RM0386, table 4: STM32F469xx, STM32F479xx
	 */
#define EEPROM_START_ADDRESS (uint32_t)0x081C0000u  //EEPROM SECTOR 22



#endif //STM32F4xx
#endif // !defined(EEPROM_PAGE0) && !defined(EEPROM_PAGE1) && !defined(EEPROM_START_ADDRESS)
#ifndef EEPROM_PAGE_SIZE
#error	"No EEPROM_PAGE_SIZE. Define EEPROM_PAGE_SIZE"
#endif

  /* Pages 0 and 1 base and end addresses */
#define EEPROM_PAGE0_BASE		((uint32_t)(EEPROM_START_ADDRESS + 0x000))
#define EEPROM_PAGE1_BASE		((uint32_t)(EEPROM_START_ADDRESS + EEPROM_PAGE_SIZE))

/* Page status definitions */
#define EEPROM_ERASED			((uint16_t)0xFFFF)	/* PAGE is empty */
#define EEPROM_RECEIVE_DATA		((uint16_t)0xEEEE)	/* PAGE is marked to receive data */
#define EEPROM_VALID_PAGE		((uint16_t)0x0000)	/* PAGE containing valid data */


#ifdef __cplusplus
 extern "C" {
#endif



typedef enum
	{
  FLASH_COMPLETE = 0,
  FASH_ERROR,  
	FLASH_BUSY,
  FLASH_TIMEOUT, 
	FLASH_ERROR_PG,
	FLASH_ERROR_OPT,
	FLASH_BAD_ADDRESS
	} FLASH_Status;

FLASH_Status FLASH_ErasePage(uint32_t Page_Address);
FLASH_Status FLASH_ProgramHalfWord(uint32_t Address, uint16_t Data);


#ifdef __cplusplus
}
#endif

#endif /* __FLASH_STM32_H */
