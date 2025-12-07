/**
 * EEPROM Emulation Implementation for CH32V003
 * Based on ST AN3969 algorithm
 */

#include "eeprom_ch32v.h"
#include "ch32v00x_flash.h"

/* Internal helper functions */
static uint16_t EE_CheckPage(uint32_t pageBase, uint16_t status, uint32_t pageSize);
static uint16_t EE_ErasePage(uint32_t pageBase, uint32_t pageSize);
static uint16_t EE_CheckErasePage(uint32_t pageBase, uint16_t status, uint32_t pageSize);
static uint32_t EE_FindValidPage(EEPROM_HandleTypeDef *heeprom);
static uint16_t EE_GetVariablesCount(uint32_t pageBase, uint16_t skipAddress, uint32_t pageSize);
static uint16_t EE_PageTransfer(EEPROM_HandleTypeDef *heeprom, uint32_t newPage, uint32_t oldPage, uint16_t skipAddress);
static uint16_t EE_VerifyPageFullWriteVariable(EEPROM_HandleTypeDef *heeprom, uint16_t Address, uint16_t Data);

/**
  * @brief  Check page for blank
  * @param  pageBase: page base address
  * @param  status: expected status
  * @param  pageSize: page size
  * @retval EEPROM_OK if page is blank, EEPROM_BAD_FLASH otherwise
  */
static uint16_t EE_CheckPage(uint32_t pageBase, uint16_t status, uint32_t pageSize) {
    uint32_t pageEnd = pageBase + pageSize;
    
    // Check page status
    uint16_t pageStatus = (*(__IO uint16_t*)pageBase);
    if (pageStatus != EEPROM_ERASED && pageStatus != status)
        return EEPROM_BAD_FLASH;
    
    // Check if page is empty (skip first 4 bytes: status + erase counter)
    for (uint32_t addr = pageBase + 4; addr < pageEnd; addr += 4) {
        if ((*(__IO uint32_t*)addr) != 0xFFFFFFFF)
            return EEPROM_BAD_FLASH;
    }
    
    return EEPROM_OK;
}

/**
  * @brief  Erase page and increment erase counter
  * @param  pageBase: page base address
  * @param  pageSize: page size
  * @retval EEPROM_OK or error code
  */
static uint16_t EE_ErasePage(uint32_t pageBase, uint32_t pageSize) {
    FLASH_Status status;
    uint16_t eraseCount;
    
    // Read current erase counter
    uint16_t pageStatus = (*(__IO uint16_t*)pageBase);
    if ((pageStatus == EEPROM_ERASED) || 
        (pageStatus == EEPROM_VALID_PAGE) || 
        (pageStatus == EEPROM_RECEIVE_DATA)) {
        eraseCount = (*(__IO uint16_t*)(pageBase + 2)) + 1;
    } else {
        eraseCount = 0;
    }
    
    // Unlock flash
    FLASH_Unlock();
    
    // Erase page (CH32V003: 1KB fast page erase or standard page erase)
    status = FLASH_ErasePage(pageBase);
    if (status != FLASH_COMPLETE) {
        FLASH_Lock();
        return EEPROM_FLASH_ERROR;
    }
    
    // Write erase counter
    status = FLASH_ProgramHalfWord(pageBase + 2, eraseCount);
    FLASH_Lock();
    
    if (status != FLASH_COMPLETE)
        return EEPROM_FLASH_ERROR;
    
    return EEPROM_OK;
}

/**
  * @brief  Check page and erase if needed
  * @param  pageBase: page base address
  * @param  status: expected status
  * @param  pageSize: page size
  * @retval EEPROM_OK or error code
  */
static uint16_t EE_CheckErasePage(uint32_t pageBase, uint16_t status, uint32_t pageSize) {
    uint16_t result;
    
    result = EE_CheckPage(pageBase, status, pageSize);
    if (result != EEPROM_OK) {
        result = EE_ErasePage(pageBase, pageSize);
        if (result != EEPROM_OK)
            return result;
        return EE_CheckPage(pageBase, status, pageSize);
    }
    
    return EEPROM_OK;
}

/**
  * @brief  Find valid page for operation
  * @param  heeprom: EEPROM handle
  * @retval Valid page address or 0 if not found
  */
static uint32_t EE_FindValidPage(EEPROM_HandleTypeDef *heeprom) {
    uint16_t status0 = (*(__IO uint16_t*)heeprom->PageBase0);
    uint16_t status1 = (*(__IO uint16_t*)heeprom->PageBase1);
    
    if (status0 == EEPROM_VALID_PAGE && status1 == EEPROM_ERASED)
        return heeprom->PageBase0;
    if (status1 == EEPROM_VALID_PAGE && status0 == EEPROM_ERASED)
        return heeprom->PageBase1;
    
    return 0;
}

/**
  * @brief  Count unique variables in page
  * @param  pageBase: page base address
  * @param  skipAddress: address to skip (or 0xFFFF)
  * @param  pageSize: page size
  * @retval Number of unique variables
  */
static uint16_t EE_GetVariablesCount(uint32_t pageBase, uint16_t skipAddress, uint32_t pageSize) {
    uint16_t varAddress, nextAddress;
    uint32_t pageEnd = pageBase + pageSize;
    uint16_t count = 0;
    
    // Start from first data slot (skip status + counter)
    for (uint32_t addr = pageBase + 6; addr < pageEnd; addr += 4) {
        varAddress = (*(__IO uint16_t*)addr);
        if (varAddress == 0xFFFF || varAddress == skipAddress)
            continue;
        
        count++;
        
        // Check if this variable is updated later
        for (uint32_t idx = addr + 4; idx < pageEnd; idx += 4) {
            nextAddress = (*(__IO uint16_t*)idx);
            if (nextAddress == varAddress) {
                count--;
                break;
            }
        }
    }
    
    return count;
}

/**
  * @brief  Transfer data from old page to new page
  * @param  heeprom: EEPROM handle
  * @param  newPage: new page base address
  * @param  oldPage: old page base address
  * @param  skipAddress: address to skip (or 0xFFFF)
  * @retval EEPROM_OK or error code
  */
static uint16_t EE_PageTransfer(EEPROM_HandleTypeDef *heeprom, uint32_t newPage, uint32_t oldPage, uint16_t skipAddress) {
    FLASH_Status flashStatus;
    uint32_t newIdx, oldIdx;
    uint16_t address, data;
    bool found;
    
    uint32_t newEnd = newPage + heeprom->PageSize;
    uint32_t oldEnd = oldPage + 4;
    
    // Find first free slot in new page
    for (newIdx = newPage + 4; newIdx < newEnd; newIdx += 4) {
        if ((*(__IO uint32_t*)newIdx) == 0xFFFFFFFF)
            break;
    }
    
    if (newIdx >= newEnd)
        return EEPROM_OUT_SIZE;
    
    FLASH_Unlock();
    
    // Transfer variables from old to new page (scan from end to beginning)
    for (oldIdx = oldPage + (heeprom->PageSize - 2); oldIdx > oldEnd; oldIdx -= 4) {
        address = (*(__IO uint16_t*)oldIdx);
        
        if (address == 0xFFFF || address == skipAddress)
            continue;
        
        // Check if already copied
        found = false;
        for (uint32_t idx = newPage + 6; idx < newIdx; idx += 4) {
            if ((*(__IO uint16_t*)idx) == address) {
                found = true;
                break;
            }
        }
        
        if (found)
            continue;
        
        // Copy variable
        if (newIdx < newEnd) {
            data = (*(__IO uint16_t*)(oldIdx - 2));
            
            flashStatus = FLASH_ProgramHalfWord(newIdx, data);
            if (flashStatus != FLASH_COMPLETE) {
                FLASH_Lock();
                return EEPROM_FLASH_ERROR;
            }
            
            flashStatus = FLASH_ProgramHalfWord(newIdx + 2, address);
            if (flashStatus != FLASH_COMPLETE) {
                FLASH_Lock();
                return EEPROM_FLASH_ERROR;
            }
            
            newIdx += 4;
        } else {
            FLASH_Lock();
            return EEPROM_OUT_SIZE;
        }
    }
    
    FLASH_Lock();
    
    // Erase old page
    uint16_t result = EE_CheckErasePage(oldPage, EEPROM_ERASED, heeprom->PageSize);
    if (result != EEPROM_OK)
        return result;
    
    // Mark new page as valid
    FLASH_Unlock();
    flashStatus = FLASH_ProgramHalfWord(newPage, EEPROM_VALID_PAGE);
    FLASH_Lock();
    
    if (flashStatus != FLASH_COMPLETE)
        return EEPROM_FLASH_ERROR;
    
    return EEPROM_OK;
}

/**
  * @brief  Write variable with page full check
  * @param  heeprom: EEPROM handle
  * @param  Address: variable address
  * @param  Data: variable data
  * @retval EEPROM_OK or error code
  */
static uint16_t EE_VerifyPageFullWriteVariable(EEPROM_HandleTypeDef *heeprom, uint16_t Address, uint16_t Data) {
    FLASH_Status flashStatus;
    uint32_t pageBase, pageEnd, newPage;
    uint16_t count;
    
    // Get valid page
    pageBase = EE_FindValidPage(heeprom);
    if (pageBase == 0)
        return EEPROM_NO_VALID_PAGE;
    
    pageEnd = pageBase + heeprom->PageSize;
    
    // Check if variable exists and can be updated in place
    for (uint32_t idx = pageEnd - 2; idx > pageBase; idx -= 4) {
        if ((*(__IO uint16_t*)idx) == Address) {
            count = (*(__IO uint16_t*)(idx - 2));
            if (count == Data)
                return EEPROM_OK;
            if (count == 0xFFFF) {
                FLASH_Unlock();
                flashStatus = FLASH_ProgramHalfWord(idx - 2, Data);
                FLASH_Lock();
                return (flashStatus == FLASH_COMPLETE) ? EEPROM_OK : EEPROM_FLASH_ERROR;
            }
            break;
        }
    }
    
    // Find free slot
    for (uint32_t idx = pageBase + 4; idx < pageEnd; idx += 4) {
        if ((*(__IO uint32_t*)idx) == 0xFFFFFFFF) {
            FLASH_Unlock();
            flashStatus = FLASH_ProgramHalfWord(idx, Data);
            if (flashStatus != FLASH_COMPLETE) {
                FLASH_Lock();
                return EEPROM_FLASH_ERROR;
            }
            flashStatus = FLASH_ProgramHalfWord(idx + 2, Address);
            FLASH_Lock();
            return (flashStatus == FLASH_COMPLETE) ? EEPROM_OK : EEPROM_FLASH_ERROR;
        }
    }
    
    // Page full - need transfer
    count = EE_GetVariablesCount(pageBase, Address, heeprom->PageSize) + 1;
    if (count >= (heeprom->PageSize / 4 - 1))
        return EEPROM_OUT_SIZE;
    
    newPage = (pageBase == heeprom->PageBase1) ? heeprom->PageBase0 : heeprom->PageBase1;
    
    // Mark new page as receive
    FLASH_Unlock();
    flashStatus = FLASH_ProgramHalfWord(newPage, EEPROM_RECEIVE_DATA);
    if (flashStatus != FLASH_COMPLETE) {
        FLASH_Lock();
        return EEPROM_FLASH_ERROR;
    }
    
    // Write new variable
    flashStatus = FLASH_ProgramHalfWord(newPage + 4, Data);
    if (flashStatus != FLASH_COMPLETE) {
        FLASH_Lock();
        return EEPROM_FLASH_ERROR;
    }
    
    flashStatus = FLASH_ProgramHalfWord(newPage + 6, Address);
    FLASH_Lock();
    
    if (flashStatus != FLASH_COMPLETE)
        return EEPROM_FLASH_ERROR;
    
    return EE_PageTransfer(heeprom, newPage, pageBase, Address);
}

/* ==================== Public API ==================== */

/**
  * @brief  Initialize EEPROM emulation
  * @param  heeprom: EEPROM handle
  * @retval EEPROM_OK or error code
  */
uint16_t EEPROM_Init(EEPROM_HandleTypeDef *heeprom) {
    uint16_t status0, status1;
    FLASH_Status flashStatus;
    
    if (!heeprom)
        return EEPROM_BAD_ADDRESS;
    
    heeprom->Status = EEPROM_NO_VALID_PAGE;
    
    status0 = (*(__IO uint16_t*)heeprom->PageBase0);
    status1 = (*(__IO uint16_t*)heeprom->PageBase1);
    
    switch (status0) {
        case EEPROM_ERASED:
            if (status1 == EEPROM_VALID_PAGE) {
                heeprom->Status = EE_CheckErasePage(heeprom->PageBase0, EEPROM_ERASED, heeprom->PageSize);
            } else if (status1 == EEPROM_RECEIVE_DATA) {
                FLASH_Unlock();
                flashStatus = FLASH_ProgramHalfWord(heeprom->PageBase1, EEPROM_VALID_PAGE);
                FLASH_Lock();
                heeprom->Status = (flashStatus == FLASH_COMPLETE) ? 
                    EE_CheckErasePage(heeprom->PageBase0, EEPROM_ERASED, heeprom->PageSize) : EEPROM_FLASH_ERROR;
            } else if (status1 == EEPROM_ERASED) {
                heeprom->Status = EEPROM_Format(heeprom);
            }
            break;
            
        case EEPROM_RECEIVE_DATA:
            if (status1 == EEPROM_VALID_PAGE) {
                heeprom->Status = EE_PageTransfer(heeprom, heeprom->PageBase0, heeprom->PageBase1, 0xFFFF);
            } else if (status1 == EEPROM_ERASED) {
                heeprom->Status = EE_CheckErasePage(heeprom->PageBase1, EEPROM_ERASED, heeprom->PageSize);
                if (heeprom->Status == EEPROM_OK) {
                    FLASH_Unlock();
                    flashStatus = FLASH_ProgramHalfWord(heeprom->PageBase0, EEPROM_VALID_PAGE);
                    FLASH_Lock();
                    heeprom->Status = (flashStatus == FLASH_COMPLETE) ? EEPROM_OK : EEPROM_FLASH_ERROR;
                }
            }
            break;
            
        case EEPROM_VALID_PAGE:
            if (status1 == EEPROM_VALID_PAGE) {
                heeprom->Status = EEPROM_NO_VALID_PAGE;
            } else if (status1 == EEPROM_RECEIVE_DATA) {
                heeprom->Status = EE_PageTransfer(heeprom, heeprom->PageBase1, heeprom->PageBase0, 0xFFFF);
            } else {
                heeprom->Status = EE_CheckErasePage(heeprom->PageBase1, EEPROM_ERASED, heeprom->PageSize);
            }
            break;
            
        default:
            if (status1 == EEPROM_VALID_PAGE) {
                heeprom->Status = EE_CheckErasePage(heeprom->PageBase0, EEPROM_ERASED, heeprom->PageSize);
            } else if (status1 == EEPROM_RECEIVE_DATA) {
                FLASH_Unlock();
                flashStatus = FLASH_ProgramHalfWord(heeprom->PageBase1, EEPROM_VALID_PAGE);
                FLASH_Lock();
                heeprom->Status = (flashStatus == FLASH_COMPLETE) ? 
                    EE_CheckErasePage(heeprom->PageBase0, EEPROM_ERASED, heeprom->PageSize) : EEPROM_FLASH_ERROR;
            }
            break;
    }
    
    return heeprom->Status;
}

/**
  * @brief  Format EEPROM (erase both pages)
  * @param  heeprom: EEPROM handle
  * @retval EEPROM_OK or error code
  */
uint16_t EEPROM_Format(EEPROM_HandleTypeDef *heeprom) {
    FLASH_Status flashStatus;
    uint16_t status;
    
    if (!heeprom)
        return EEPROM_BAD_ADDRESS;
    
    // Erase Page0
    status = EE_CheckErasePage(heeprom->PageBase0, EEPROM_VALID_PAGE, heeprom->PageSize);
    if (status != EEPROM_OK)
        return status;
    
    // Mark Page0 as valid
    if ((*(__IO uint16_t*)heeprom->PageBase0) == EEPROM_ERASED) {
        FLASH_Unlock();
        flashStatus = FLASH_ProgramHalfWord(heeprom->PageBase0, EEPROM_VALID_PAGE);
        FLASH_Lock();
        if (flashStatus != FLASH_COMPLETE)
            return EEPROM_FLASH_ERROR;
    }
    
    // Erase Page1
    return EE_CheckErasePage(heeprom->PageBase1, EEPROM_ERASED, heeprom->PageSize);
}

/**
  * @brief  Read variable from EEPROM
  * @param  heeprom: EEPROM handle
  * @param  Address: variable address
  * @param  Data: pointer to data
  * @retval EEPROM_OK or error code
  */
uint16_t EEPROM_Read(EEPROM_HandleTypeDef *heeprom, uint16_t Address, uint16_t *Data) {
    uint32_t pageBase, pageEnd;
    
    if (!heeprom || !Data)
        return EEPROM_BAD_ADDRESS;
    
    *Data = EEPROM_DEFAULT_DATA;
    
    if (heeprom->Status == EEPROM_NOT_INIT) {
        if (EEPROM_Init(heeprom) != EEPROM_OK)
            return heeprom->Status;
    }
    
    pageBase = EE_FindValidPage(heeprom);
    if (pageBase == 0)
        return EEPROM_NO_VALID_PAGE;
    
    pageEnd = pageBase + heeprom->PageSize - 2;
    
    // Search from end to beginning (newest first)
    for (uint32_t addr = pageEnd; addr >= pageBase + 6; addr -= 4) {
        if ((*(__IO uint16_t*)addr) == Address) {
            *Data = (*(__IO uint16_t*)(addr - 2));
            return EEPROM_OK;
        }
    }
    
    return EEPROM_BAD_ADDRESS;
}

/**
  * @brief  Write variable to EEPROM
  * @param  heeprom: EEPROM handle
  * @param  Address: variable address
  * @param  Data: variable data
  * @retval EEPROM_OK or error code
  */
uint16_t EEPROM_Write(EEPROM_HandleTypeDef *heeprom, uint16_t Address, uint16_t Data) {
    if (!heeprom)
        return EEPROM_BAD_ADDRESS;
    
    if (heeprom->Status == EEPROM_NOT_INIT) {
        if (EEPROM_Init(heeprom) != EEPROM_OK)
            return heeprom->Status;
    }
    
    if (Address == 0xFFFF)
        return EEPROM_BAD_ADDRESS;
    
    return EE_VerifyPageFullWriteVariable(heeprom, Address, Data);
}

/**
  * @brief  Update variable (write only if different)
  * @param  heeprom: EEPROM handle
  * @param  Address: variable address
  * @param  Data: variable data
  * @retval EEPROM_OK, EEPROM_SAME_VALUE, or error code
  */
uint16_t EEPROM_Update(EEPROM_HandleTypeDef *heeprom, uint16_t Address, uint16_t Data) {
    uint16_t currentData;
    uint16_t status;
    
    status = EEPROM_Read(heeprom, Address, &currentData);
    if (status == EEPROM_OK && currentData == Data)
        return EEPROM_SAME_VALUE;
    
    return EEPROM_Write(heeprom, Address, Data);
}

/**
  * @brief  Count variables in EEPROM
  * @param  heeprom: EEPROM handle
  * @param  Count: pointer to count
  * @retval EEPROM_OK or error code
  */
uint16_t EEPROM_Count(EEPROM_HandleTypeDef *heeprom, uint16_t *Count) {
    uint32_t pageBase;
    
    if (!heeprom || !Count)
        return EEPROM_BAD_ADDRESS;
    
    if (heeprom->Status == EEPROM_NOT_INIT) {
        if (EEPROM_Init(heeprom) != EEPROM_OK)
            return heeprom->Status;
    }
    
    pageBase = EE_FindValidPage(heeprom);
    if (pageBase == 0)
        return EEPROM_NO_VALID_PAGE;
    
    *Count = EE_GetVariablesCount(pageBase, 0xFFFF, heeprom->PageSize);
    return EEPROM_OK;
}

/**
  * @brief  Get maximum variable count
  * @param  heeprom: EEPROM handle
  * @retval Maximum count
  */
uint16_t EEPROM_MaxCount(EEPROM_HandleTypeDef *heeprom) {
    if (!heeprom)
        return 0;
    return (heeprom->PageSize / 4) - 1;
}

/**
  * @brief  Get erase counter
  * @param  heeprom: EEPROM handle
  * @param  Erases: pointer to erase count
  * @retval EEPROM_OK or error code
  */
uint16_t EEPROM_Erases(EEPROM_HandleTypeDef *heeprom, uint16_t *Erases) {
    uint32_t pageBase;
    
    if (!heeprom || !Erases)
        return EEPROM_BAD_ADDRESS;
    
    if (heeprom->Status == EEPROM_NOT_INIT) {
        if (EEPROM_Init(heeprom) != EEPROM_OK)
            return heeprom->Status;
    }
    
    pageBase = EE_FindValidPage(heeprom);
    if (pageBase == 0)
        return EEPROM_NO_VALID_PAGE;
    
    *Erases = (*(__IO uint16_t*)(pageBase + 2));
    return EEPROM_OK;
}