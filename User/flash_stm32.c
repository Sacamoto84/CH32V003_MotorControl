#include "flash_stm32.h"


/**
  * @brief  Erases a specified FLASH page.
  * @param  Page_Address: The page address to be erased.
  * @retval FLASH Status: The returned value can be: FLASH_BUSY, FLASH_ERROR_PG,
  *   FLASH_ERROR_WRP, FLASH_COMPLETE or FLASH_TIMEOUT.
  */
 
  FLASH_Status FLASH_ErasePage(uint32_t Page_Address) {
  FLASH_Status status = FLASH_COMPLETE;
  FLASH_EraseInitTypeDef EraseInitStruct;

  /* Check the parameters */
  switch (Page_Address)
  {
    case EEPROM_PAGE0_BASE ... (EEPROM_PAGE1_BASE - 1) :
      EraseInitStruct.Sector = EEPROM_PAGE0;
      break;
      case EEPROM_PAGE1_BASE ... (EEPROM_PAGE1_BASE + EEPROM_PAGE_SIZE - 1) :
        EraseInitStruct.Sector = EEPROM_PAGE1;
        break;
      default:
        return FLASH_BAD_ADDRESS;
        break;
  }

  uint32_t PAGEError = 0;


  /* Fill EraseInit structure*/
  EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
  EraseInitStruct.NbSectors = 1U;
  EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  EraseInitStruct.Banks = 1U;

  //try to unlock flash
 __disable_irq();  // Disable interrupts  
  status = HAL_FLASH_Unlock();
  if (status != FLASH_COMPLETE)
  {
    __enable_irq(); // Re-enable interrupts before exiting
    return status;
  }

  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
  status = HAL_FLASHEx_Erase(&EraseInitStruct, &PAGEError);

  HAL_FLASH_Lock();
  __enable_irq();   // Re-enable interrupts
  return status;
}

/**
  * @brief  Programs a half word at a specified address.
  * @param  Address: specifies the address to be programmed.
  * @param  Data: specifies the data to be programmed.
  * @retval FLASH Status: The returned value can be: FLASH_ERROR_PG,
  *   FLASH_ERROR_WRP, FLASH_COMPLETE or FLASH_TIMEOUT.
  */
FLASH_Status FLASH_ProgramHalfWord(uint32_t Address, uint16_t Data) {
  FLASH_Status status = FLASH_BAD_ADDRESS;

  if (IS_FLASH_ADDRESS(Address))
  {
    __disable_irq();
    if ((status = HAL_FLASH_Unlock()) != FLASH_COMPLETE)
    {
     __enable_irq(); // Re-enable interrupts before exiting
      return status;
    }
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, Address, Data);

    HAL_FLASH_Lock();
   __enable_irq(); 
  }
  return status;
}

