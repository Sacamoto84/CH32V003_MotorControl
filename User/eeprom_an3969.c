#include "eeprom.h"
#include "ch32v00x_flash.h"

/* Глобальная переменная для хранения значения переменной при чтении */
uint16_t DataVar = 0;

/* Таблица виртуальных адресов, определяемая пользователем: значение 0xFFFF запрещено */
uint16_t VirtAddVarTab[NB_OF_VAR];

/* Прототипы приватных функций -----------------------------------------------*/
static FLASH_Status EE_Format (void);
static uint16_t EE_FindValidPage (uint8_t Operation);
static uint16_t EE_VerifyPageFullWriteVariable (uint16_t VirtAddress, uint16_t Data);
static uint16_t EE_PageTransfer (uint16_t VirtAddress, uint16_t Data);

/**
 * @brief  Стирание сектора Flash-памяти
 * @param  page_id: Идентификатор страницы (PAGE0_ID или PAGE1_ID)
 * @retval Статус операции стирания Flash
 */
uint16_t FLASH_EraseSector (int page_id) {
    EEPROM_LOG_ERASE_MSG ("========================================");
    EEPROM_LOG_ERASE_MSG ("Начало стирания сектора, ID страницы: %d", page_id);

    FLASH_Unlock();

    uint32_t base;
    if (page_id == PAGE0_ID) {
        base = PAGE0_BASE_ADDRESS;
        EEPROM_LOG_ERASE_MSG ("Выбрана PAGE0, адрес: 0x%08X", base);
    } else {
        base = PAGE1_BASE_ADDRESS;
        EEPROM_LOG_ERASE_MSG ("Выбрана PAGE1, адрес: 0x%08X", base);
    }

    EEPROM_LOG_ERASE_MSG ("⏳ Выполняется стирание...");
    FLASH_Status status = FLASH_ErasePage (base);

    if (status != FLASH_COMPLETE) {
        EEPROM_LOG_ERASE_MSG ("❌ ОШИБКА стирания! Код статуса: %d", status);
    } else {
        EEPROM_LOG_ERASE_MSG ("✓ Стирание завершено успешно");
    }

    FLASH_Lock();
    EEPROM_LOG_ERASE_MSG ("========================================");

    return status;
}

/**
 * @brief  Восстановление страниц в известное корректное состояние в случае
 *         повреждения статуса страниц после потери питания.
 * @param  Нет параметров
 * @retval - Код ошибки Flash: при ошибке записи во Flash
 *         - FLASH_COMPLETE: при успехе
 */
uint16_t EE_Init (void) {
    EEPROM_LOG_INIT_MSG ("╔════════════════════════════════════════╗");
    EEPROM_LOG_INIT_MSG ("║   ИНИЦИАЛИЗАЦИЯ ЭМУЛЯЦИИ EEPROM        ║");
    EEPROM_LOG_INIT_MSG ("╚════════════════════════════════════════╝");

    uint16_t PageStatus0 = 6, PageStatus1 = 6;
    uint16_t VarIdx = 0;
    uint16_t EepromStatus = 0, ReadStatus = 0;
    int16_t x = -1;
    uint16_t FlashStatus;

    /* Получение статуса Page0 */
    PageStatus0 = (*(__IO uint16_t *)PAGE0_BASE_ADDRESS);
    EEPROM_LOG_INIT_MSG ("Статус PAGE0 (0x%08X): 0x%04X", PAGE0_BASE_ADDRESS, PageStatus0);

    /* Получение статуса Page1 */
    PageStatus1 = (*(__IO uint16_t *)PAGE1_BASE_ADDRESS);
    EEPROM_LOG_INIT_MSG ("Статус PAGE1 (0x%08X): 0x%04X", PAGE1_BASE_ADDRESS, PageStatus1);

    /* Проверка недопустимых состояний заголовков и восстановление при необходимости */
    EEPROM_LOG_INIT_MSG ("----------------------------------------");
    EEPROM_LOG_INIT_MSG ("Анализ состояний страниц...");

    switch (PageStatus0) {
    case ERASED:
        EEPROM_LOG_INIT_MSG ("PAGE0: ERASED (стерта)");

        if (PageStatus1 == VALID_PAGE) {
            EEPROM_LOG_INIT_MSG ("PAGE1: VALID_PAGE (валидна)");
            EEPROM_LOG_INIT_MSG ("Сценарий: Page0 стерта, Page1 валидна");
            EEPROM_LOG_INIT_MSG ("Действие: Стереть Page0 для очистки");

            /* Стирание Page0 */
            FlashStatus = FLASH_EraseSector (PAGE0_ID);
            if (FlashStatus != FLASH_COMPLETE) {
                EEPROM_LOG_INIT_MSG ("❌ Ошибка при стирании Page0!");
                return FlashStatus;
            }
        } else if (PageStatus1 == RECEIVE_DATA) {
            EEPROM_LOG_INIT_MSG ("PAGE1: RECEIVE_DATA (принимает данные)");
            EEPROM_LOG_INIT_MSG ("Сценарий: Page0 стерта, Page1 принимает данные");
            EEPROM_LOG_INIT_MSG ("Действие: Стереть Page0 и пометить Page1 как валидную");

            /* Стирание Page0 */
            FlashStatus = FLASH_EraseSector (PAGE0_ID);
            if (FlashStatus != FLASH_COMPLETE) {
                EEPROM_LOG_INIT_MSG ("❌ Ошибка при стирании Page0!");
                return FlashStatus;
            }
            /* Пометить Page1 как валидную */
            EEPROM_LOG_INIT_MSG ("Запись статуса VALID_PAGE в Page1...");
            FLASH_Unlock();
            FlashStatus = FLASH_ProgramHalfWord (PAGE1_BASE_ADDRESS, VALID_PAGE);
            FLASH_Lock();
            if (FlashStatus != FLASH_COMPLETE) {
                EEPROM_LOG_INIT_MSG ("❌ Ошибка при записи статуса Page1!");
                return FlashStatus;
            }
            EEPROM_LOG_INIT_MSG ("✓ Page1 помечена как валидная");
        } else {
            EEPROM_LOG_INIT_MSG ("PAGE1: статус 0x%04X (некорректный)", PageStatus1);
            EEPROM_LOG_INIT_MSG ("Сценарий: Первый доступ к EEPROM или недопустимое состояние");
            EEPROM_LOG_INIT_MSG ("Действие: Форматирование EEPROM");

            /* Стирание обеих страниц и установка Page0 как валидной */
            FlashStatus = EE_Format();
            if (FlashStatus != FLASH_COMPLETE) {
                EEPROM_LOG_INIT_MSG ("❌ Ошибка при форматировании!");
                return FlashStatus;
            }
        }
        break;

    case RECEIVE_DATA:
        EEPROM_LOG_INIT_MSG ("PAGE0: RECEIVE_DATA (принимает данные)");

        if (PageStatus1 == VALID_PAGE) {
            EEPROM_LOG_INIT_MSG ("PAGE1: VALID_PAGE (валидна)");
            EEPROM_LOG_INIT_MSG ("Сценарий: Page0 принимает данные, Page1 валидна");
            EEPROM_LOG_INIT_MSG ("Действие: Перенос данных из Page1 в Page0");

            /* Перенос данных из Page1 в Page0 */
            EEPROM_LOG_INIT_MSG ("Начало переноса переменных (всего: %d)...", NB_OF_VAR);
            for (VarIdx = 0; VarIdx < NB_OF_VAR; VarIdx++) {
                if ((*(__IO uint16_t *)(PAGE0_BASE_ADDRESS + 6)) == VirtAddVarTab[VarIdx]) {
                    x = VarIdx;
                    EEPROM_LOG_INIT_MSG ("Переменная %d уже записана в Page0, пропуск", VarIdx);
                }
                if (VarIdx != x) {
                    EEPROM_LOG_INIT_MSG ("Перенос переменной %d (VirtAddr: 0x%04X)...", VarIdx, VirtAddVarTab[VarIdx]);
                    /* Чтение последних обновлений переменных */
                    ReadStatus = EE_ReadVariable (VirtAddVarTab[VarIdx], &DataVar);
                    /* Если переменная, соответствующая виртуальному адресу, была найдена */
                    if (ReadStatus != 0x1) {
                        EEPROM_LOG_INIT_MSG ("  Найдена, значение: 0x%04X", DataVar);
                        /* Перенос переменной в Page0 */
                        EepromStatus = EE_VerifyPageFullWriteVariable (VirtAddVarTab[VarIdx], DataVar);
                        if (EepromStatus != FLASH_COMPLETE) {
                            EEPROM_LOG_INIT_MSG ("❌ Ошибка при переносе переменной %d!", VarIdx);
                            return EepromStatus;
                        }
                        EEPROM_LOG_INIT_MSG ("  ✓ Переменная перенесена");
                    } else {
                        EEPROM_LOG_INIT_MSG ("  Переменная не найдена, пропуск");
                    }
                }
            }
            /* Пометить Page0 как валидную */
            EEPROM_LOG_INIT_MSG ("Запись статуса VALID_PAGE в Page0...");
            FLASH_Unlock();
            FlashStatus = FLASH_ProgramHalfWord (PAGE0_BASE_ADDRESS, VALID_PAGE);
            FLASH_Lock();
            if (FlashStatus != FLASH_COMPLETE) {
                EEPROM_LOG_INIT_MSG ("❌ Ошибка при записи статуса Page0!");
                return FlashStatus;
            }
            EEPROM_LOG_INIT_MSG ("✓ Page0 помечена как валидная");

            /* Стирание Page1 */
            EEPROM_LOG_INIT_MSG ("Стирание Page1...");
            FlashStatus = FLASH_EraseSector (PAGE1_ID);
            if (FlashStatus != FLASH_COMPLETE) {
                EEPROM_LOG_INIT_MSG ("❌ Ошибка при стирании Page1!");
                return FlashStatus;
            }
        } else if (PageStatus1 == ERASED) {
            EEPROM_LOG_INIT_MSG ("PAGE1: ERASED (стерта)");
            EEPROM_LOG_INIT_MSG ("Сценарий: Page0 принимает данные, Page1 стерта");
            EEPROM_LOG_INIT_MSG ("Действие: Очистить Page1 и пометить Page0 как валидную");

            /* Стирание Page1 */
            FlashStatus = FLASH_EraseSector (PAGE1_ID);
            if (FlashStatus != FLASH_COMPLETE) {
                EEPROM_LOG_INIT_MSG ("❌ Ошибка при стирании Page1!");
                return FlashStatus;
            }
            /* Пометить Page0 как валидную */
            EEPROM_LOG_INIT_MSG ("Запись статуса VALID_PAGE в Page0...");
            FLASH_Unlock();
            FlashStatus = FLASH_ProgramHalfWord (PAGE0_BASE_ADDRESS, VALID_PAGE);
            FLASH_Lock();
            if (FlashStatus != FLASH_COMPLETE) {
                EEPROM_LOG_INIT_MSG ("❌ Ошибка при записи статуса Page0!");
                return FlashStatus;
            }
            EEPROM_LOG_INIT_MSG ("✓ Page0 помечена как валидная");
        } else {
            EEPROM_LOG_INIT_MSG ("PAGE1: статус 0x%04X (некорректный)", PageStatus1);
            EEPROM_LOG_INIT_MSG ("Сценарий: Недопустимое состояние");
            EEPROM_LOG_INIT_MSG ("Действие: Форматирование EEPROM");

            /* Стирание обеих страниц и установка Page0 как валидной */
            FlashStatus = EE_Format();
            if (FlashStatus != FLASH_COMPLETE) {
                EEPROM_LOG_INIT_MSG ("❌ Ошибка при форматировании!");
                return FlashStatus;
            }
        }
        break;

    case VALID_PAGE:
        EEPROM_LOG_INIT_MSG ("PAGE0: VALID_PAGE (валидна)");

        if (PageStatus1 == VALID_PAGE) {
            EEPROM_LOG_INIT_MSG ("PAGE1: VALID_PAGE (валидна)");
            EEPROM_LOG_INIT_MSG ("Сценарий: Обе страницы валидны (недопустимое состояние!)");
            EEPROM_LOG_INIT_MSG ("Действие: Форматирование EEPROM");

            /* Стирание обеих страниц и установка Page0 как валидной */
            FlashStatus = EE_Format();
            if (FlashStatus != FLASH_COMPLETE) {
                EEPROM_LOG_INIT_MSG ("❌ Ошибка при форматировании!");
                return FlashStatus;
            }
        } else if (PageStatus1 == ERASED) {
            EEPROM_LOG_INIT_MSG ("PAGE1: ERASED (стерта)");
            EEPROM_LOG_INIT_MSG ("Сценарий: Page0 валидна, Page1 стерта");
            EEPROM_LOG_INIT_MSG ("Действие: Очистить Page1");

            /* Стирание Page1 */
            FlashStatus = FLASH_EraseSector (PAGE1_ID);
            if (FlashStatus != FLASH_COMPLETE) {
                EEPROM_LOG_INIT_MSG ("❌ Ошибка при стирании Page1!");
                return FlashStatus;
            }
        } else {
            EEPROM_LOG_INIT_MSG ("PAGE1: RECEIVE_DATA (принимает данные)");
            EEPROM_LOG_INIT_MSG ("Сценарий: Page0 валидна, Page1 принимает данные");
            EEPROM_LOG_INIT_MSG ("Действие: Перенос данных из Page0 в Page1");

            /* Перенос данных из Page0 в Page1 */
            EEPROM_LOG_INIT_MSG ("Начало переноса переменных (всего: %d)...", NB_OF_VAR);
            for (VarIdx = 0; VarIdx < NB_OF_VAR; VarIdx++) {
                if ((*(__IO uint16_t *)(PAGE1_BASE_ADDRESS + 6)) == VirtAddVarTab[VarIdx]) {
                    x = VarIdx;
                    EEPROM_LOG_INIT_MSG ("Переменная %d уже записана в Page1, пропуск", VarIdx);
                }
                if (VarIdx != x) {
                    EEPROM_LOG_INIT_MSG ("Перенос переменной %d (VirtAddr: 0x%04X)...", VarIdx, VirtAddVarTab[VarIdx]);
                    /* Чтение последних обновлений переменных */
                    ReadStatus = EE_ReadVariable (VirtAddVarTab[VarIdx], &DataVar);
                    /* Если переменная, соответствующая виртуальному адресу, была найдена */
                    if (ReadStatus != 0x1) {
                        EEPROM_LOG_INIT_MSG ("  Найдена, значение: 0x%04X", DataVar);
                        /* Перенос переменной в Page1 */
                        EepromStatus = EE_VerifyPageFullWriteVariable (VirtAddVarTab[VarIdx], DataVar);
                        if (EepromStatus != FLASH_COMPLETE) {
                            EEPROM_LOG_INIT_MSG ("❌ Ошибка при переносе переменной %d!", VarIdx);
                            return EepromStatus;
                        }
                        EEPROM_LOG_INIT_MSG ("  ✓ Переменная перенесена");
                    } else {
                        EEPROM_LOG_INIT_MSG ("  Переменная не найдена, пропуск");
                    }
                }
            }
            /* Пометить Page1 как валидную */
            EEPROM_LOG_INIT_MSG ("Запись статуса VALID_PAGE в Page1...");
            FLASH_Unlock();
            FlashStatus = FLASH_ProgramHalfWord (PAGE1_BASE_ADDRESS, VALID_PAGE);
            FLASH_Lock();
            if (FlashStatus != FLASH_COMPLETE) {
                EEPROM_LOG_INIT_MSG ("❌ Ошибка при записи статуса Page1!");
                return FlashStatus;
            }
            EEPROM_LOG_INIT_MSG ("✓ Page1 помечена как валидная");

            /* Стирание Page0 */
            EEPROM_LOG_INIT_MSG ("Стирание Page0...");
            FlashStatus = FLASH_EraseSector (PAGE0_ID);
            if (FlashStatus != FLASH_COMPLETE) {
                EEPROM_LOG_INIT_MSG ("❌ Ошибка при стирании Page0!");
                return FlashStatus;
            }
        }
        break;

    default:
        EEPROM_LOG_INIT_MSG ("PAGE0: статус 0x%04X (неизвестный)", PageStatus0);
        EEPROM_LOG_INIT_MSG ("Сценарий: Любое другое состояние");
        EEPROM_LOG_INIT_MSG ("Действие: Форматирование EEPROM");

        /* Стирание обеих страниц и установка Page0 как валидной */
        FlashStatus = EE_Format();
        if (FlashStatus != FLASH_COMPLETE) {
            EEPROM_LOG_INIT_MSG ("❌ Ошибка при форматировании!");
            return FlashStatus;
        }
        break;
    }

    EEPROM_LOG_INIT_MSG ("----------------------------------------");
    EEPROM_LOG_INIT_MSG ("✓ Инициализация завершена успешно");
    EEPROM_LOG_INIT_MSG ("╚════════════════════════════════════════╝");

    return FLASH_COMPLETE;
}

/**
 * @brief  Возвращает последние сохраненные данные переменной, если найдены,
 *         которые соответствуют переданному виртуальному адресу
 * @param  VirtAddress: Виртуальный адрес переменной
 * @param  Data: Глобальная переменная, содержащая прочитанное значение переменной
 * @retval Статус успеха или ошибки:
 *           - 0: если переменная была найдена
 *           - 1: если переменная не была найдена
 *           - NO_VALID_PAGE: если валидная страница не была найдена
 */
uint16_t EE_ReadVariable (uint16_t VirtAddress, uint16_t *Data) {
    EEPROM_LOG_READ_MSG ("→ Чтение переменной, VirtAddr: 0x%04X", VirtAddress);

    uint16_t ValidPage = PAGE0;
    uint16_t AddressValue = 0x5555, ReadStatus = 1;
    uint32_t Address = EEPROM_START_ADDRESS, PageStartAddress = EEPROM_START_ADDRESS;

    /* Получение активной страницы для операции чтения */
    ValidPage = EE_FindValidPage (READ_FROM_VALID_PAGE);
    EEPROM_LOG_READ_MSG ("  Активная страница: %s", ValidPage == PAGE0 ? "PAGE0" : (ValidPage == PAGE1 ? "PAGE1" : "НЕТ ВАЛИДНОЙ"));

    /* Проверка на отсутствие валидной страницы */
    if (ValidPage == NO_VALID_PAGE) {
        EEPROM_LOG_READ_MSG ("  ❌ Валидная страница не найдена!");
        return NO_VALID_PAGE;
    }

    /* Получение начального адреса валидной страницы */
    PageStartAddress = (uint32_t)(EEPROM_START_ADDRESS + (uint32_t)(ValidPage * PAGE_SIZE));
    EEPROM_LOG_READ_MSG ("  Начальный адрес страницы: 0x%08X", PageStartAddress);

    /* Получение конечного адреса валидной страницы */
    Address = (uint32_t)((EEPROM_START_ADDRESS - 2) + (uint32_t)((1 + ValidPage) * PAGE_SIZE));
    EEPROM_LOG_READ_MSG ("  Конечный адрес страницы: 0x%08X", Address);

    /* Проверка каждого адреса активной страницы, начиная с конца */
    EEPROM_LOG_READ_MSG ("  Поиск переменной (от конца к началу)...");
    int search_count = 0;
    while (Address > (PageStartAddress + 2)) {
        /* Получение содержимого текущего адреса для сравнения с виртуальным адресом */
        AddressValue = (*(__IO uint16_t *)Address);

        /* Сравнение прочитанного адреса с виртуальным адресом */
        if (AddressValue == VirtAddress) {
            /* Получение содержимого Address-2, которое является значением переменной */
            *Data = (*(__IO uint16_t *)(Address - 2));

            EEPROM_LOG_READ_MSG ("  ✓ Переменная найдена на адресе 0x%08X", Address);
            EEPROM_LOG_READ_MSG ("  Значение: 0x%04X (проверено %d адресов)", *Data, search_count + 1);

            /* Если значение переменной прочитано, сброс флага ReadStatus */
            ReadStatus = 0;
            break;
        } else {
            /* Переход к следующему адресу */
            Address = Address - 4;
            search_count++;
        }
    }

    if (ReadStatus == 1) {
        EEPROM_LOG_READ_MSG ("  ❌ Переменная НЕ найдена (проверено %d адресов)", search_count);
    }

    /* Возврат значения ReadStatus: (0: переменная существует, 1: переменная не существует) */
    return ReadStatus;
}

/**
 * @brief  Записывает/обновляет данные переменной в EEPROM
 * @param  VirtAddress: Виртуальный адрес переменной
 * @param  Data: 16-битные данные для записи
 * @retval Статус успеха или ошибки:
 *           - FLASH_COMPLETE: при успехе
 *           - PAGE_FULL: если валидная страница заполнена
 *           - NO_VALID_PAGE: если валидная страница не была найдена
 *           - Код ошибки Flash: при ошибке записи во Flash
 */
uint16_t EE_WriteVariable (uint16_t VirtAddress, uint16_t Data) {
    EEPROM_LOG_WRITE_MSG ("═");
    EEPROM_LOG_WRITE_MSG ("→ Write Addr:0x%04X D:0x%04X", VirtAddress, Data);

    uint16_t Status = 0;

    /* Запись виртуального адреса и значения переменной в EEPROM */
    Status = EE_VerifyPageFullWriteVariable (VirtAddress, Data);

    /* Если активная страница EEPROM заполнена */
    if (Status == PAGE_FULL) {
        EEPROM_LOG_WRITE_MSG ("  ⚠ Page Full!");
        EEPROM_LOG_WRITE_MSG ("  Page transfer...");
        /* Выполнить перенос страницы */
        Status = EE_PageTransfer (VirtAddress, Data);
    }

    if (Status == FLASH_COMPLETE) {
        EEPROM_LOG_WRITE_MSG ("  ✓ Write..Ok");
    } else {
        EEPROM_LOG_WRITE_MSG ("  ❌ Write Error: 0x%04X", Status);
    }
    EEPROM_LOG_WRITE_MSG ("═");

    /* Возврат статуса последней операции */
    return Status;
}

/**
 * @brief  Стирает PAGE0 и PAGE1 и записывает заголовок VALID_PAGE в PAGE0
 * @param  Нет параметров
 * @retval Статус последней операции (запись или стирание Flash), выполненной
 *         во время форматирования EEPROM
 */
static FLASH_Status EE_Format (void) {
    EEPROM_LOG_FORMAT_MSG ("╔════════════════════════════════════════╗");
    EEPROM_LOG_FORMAT_MSG ("║     ФОРМАТИРОВАНИЕ EEPROM              ║");
    EEPROM_LOG_FORMAT_MSG ("╚════════════════════════════════════════╝");

    FLASH_Status FlashStatus = FLASH_COMPLETE;

    /* Стирание Page0 */
    EEPROM_LOG_FORMAT_MSG ("Шаг 1/3: Стирание Page0...");
    FlashStatus = FLASH_EraseSector (PAGE0_ID);

    /* Если операция стирания не удалась, возвращается код ошибки Flash */
    if (FlashStatus != FLASH_COMPLETE) {
        EEPROM_LOG_FORMAT_MSG ("❌ Ошибка при стирании Page0!");
        return FlashStatus;
    }
    EEPROM_LOG_FORMAT_MSG ("✓ Page0 стерта");

    /* Установка Page0 как валидной страницы: запись VALID_PAGE по базовому адресу Page0 */
    EEPROM_LOG_FORMAT_MSG ("Шаг 2/3: Установка Page0 как валидной...");
    FLASH_Unlock();
    FlashStatus = FLASH_ProgramHalfWord (PAGE0_BASE_ADDRESS, VALID_PAGE);
    FLASH_Lock();
    /* Если операция программирования не удалась, возвращается код ошибки Flash */
    if (FlashStatus != FLASH_COMPLETE) {
        EEPROM_LOG_FORMAT_MSG ("❌ Ошибка при записи статуса Page0!");
        return FlashStatus;
    }
    EEPROM_LOG_FORMAT_MSG ("✓ Page0 помечена как VALID_PAGE");

    /* Стирание Page1 */
    EEPROM_LOG_FORMAT_MSG ("Шаг 3/3: Стирание Page1...");
    FlashStatus = FLASH_EraseSector (PAGE1_ID);

    if (FlashStatus == FLASH_COMPLETE) {
        EEPROM_LOG_FORMAT_MSG ("✓ Page1 стерта");
        EEPROM_LOG_FORMAT_MSG ("✓ Форматирование завершено успешно");
    } else {
        EEPROM_LOG_FORMAT_MSG ("❌ Ошибка при стирании Page1!");
    }
    EEPROM_LOG_FORMAT_MSG ("╚════════════════════════════════════════╝");

    /* Возврат статуса операции стирания Page1 */
    return FlashStatus;
}

/**
 * @brief  Поиск валидной страницы для операции записи или чтения
 * @param  Operation: операция, которую необходимо выполнить на валидной странице.
 *   Этот параметр может принимать одно из следующих значений:
 *     @arg READ_FROM_VALID_PAGE: операция чтения из валидной страницы
 *     @arg WRITE_IN_VALID_PAGE: операция записи в валидную страницу
 * @retval Номер валидной страницы (PAGE0 или PAGE1) или NO_VALID_PAGE
 *         в случае, если валидная страница не была найдена
 */
static uint16_t EE_FindValidPage (uint8_t Operation) {
    uint16_t PageStatus0 = 6, PageStatus1 = 6;

    /* Получение актуального статуса Page0 */
    PageStatus0 = (*(__IO uint16_t *)PAGE0_BASE_ADDRESS);

    /* Получение актуального статуса Page1 */
    PageStatus1 = (*(__IO uint16_t *)PAGE1_BASE_ADDRESS);

    /* Операция записи или чтения */
    switch (Operation) {
    case WRITE_IN_VALID_PAGE: /* ---- Операция записи ---- */
        if (PageStatus1 == VALID_PAGE) {
            /* Page0 принимает данные */
            if (PageStatus0 == RECEIVE_DATA) {
                return PAGE0; /* Page0 валидна */
            } else {
                return PAGE1; /* Page1 валидна */
            }
        } else if (PageStatus0 == VALID_PAGE) {
            /* Page1 принимает данные */
            if (PageStatus1 == RECEIVE_DATA) {
                return PAGE1; /* Page1 валидна */
            } else {
                return PAGE0; /* Page0 валидна */
            }
        } else {
            return NO_VALID_PAGE; /* Нет валидной страницы */
        }

    case READ_FROM_VALID_PAGE:    /* ---- Операция чтения ---- */
        if (PageStatus0 == VALID_PAGE) {
            return PAGE0;         /* Page0 валидна */
        } else if (PageStatus1 == VALID_PAGE) {
            return PAGE1;         /* Page1 валидна */
        } else {
            return NO_VALID_PAGE; /* Нет валидной страницы */
        }

    default:
        return PAGE0; /* Page0 валидна (по умолчанию) */
    }
}

/**
 * @brief  Проверяет, заполнена ли активная страница, и записывает переменную в EEPROM
 * @param  VirtAddress: 16-битный виртуальный адрес переменной
 * @param  Data: 16-битные данные для записи как значение переменной
 * @retval Статус успеха или ошибки:
 *           - FLASH_COMPLETE: при успехе
 *           - PAGE_FULL: если валидная страница заполнена
 *           - NO_VALID_PAGE: если валидная страница не была найдена
 *           - Код ошибки Flash: при ошибке записи во Flash
 */
static uint16_t EE_VerifyPageFullWriteVariable (uint16_t VirtAddress, uint16_t Data) {
    EEPROM_LOG_WRITE_MSG ("  Проверка заполненности страницы и запись...");

    FLASH_Status FlashStatus = FLASH_COMPLETE;
    uint16_t ValidPage = PAGE0;
    uint32_t Address = EEPROM_START_ADDRESS, PageEndAddress = EEPROM_START_ADDRESS + PAGE_SIZE;

    /* Получение валидной страницы для операции записи */
    ValidPage = EE_FindValidPage (WRITE_IN_VALID_PAGE);
    EEPROM_LOG_WRITE_MSG ("    Валидная страница для записи: %s", ValidPage == PAGE0 ? "PAGE0" : (ValidPage == PAGE1 ? "PAGE1" : "НЕТ"));

    /* Проверка на отсутствие валидной страницы */
    if (ValidPage == NO_VALID_PAGE) {
        EEPROM_LOG_WRITE_MSG ("    ❌ Валидная страница не найдена!");
        return NO_VALID_PAGE;
    }

    /* Получение начального адреса валидной страницы */
    Address = (uint32_t)(EEPROM_START_ADDRESS + (uint32_t)(ValidPage * PAGE_SIZE));

    /* Получение конечного адреса валидной страницы */
    PageEndAddress = (uint32_t)((EEPROM_START_ADDRESS - 2) + (uint32_t)((1 + ValidPage) * PAGE_SIZE));
    EEPROM_LOG_WRITE_MSG ("    адрес: 0x%08X..0x%08X", Address, PageEndAddress);

    /* Проверка каждого адреса активной страницы, начиная с начала */
    EEPROM_LOG_WRITE_MSG ("    Search available space...");
    int checked_addresses = 0;
    while (Address < PageEndAddress) {
        /* Проверка, содержат ли Address и Address+2 значение 0xFFFFFFFF (пусто) */
        if ((*(__IO uint32_t *)Address) == 0xFFFFFFFF) {
            EEPROM_LOG_WRITE_MSG ("    ✓ Свободное место найдено на 0x%08X (проверено %d адресов)", Address, checked_addresses + 1);

            /* Установка данных переменной */
            EEPROM_LOG_WRITE_MSG ("    Запись данных (0x%04X) на адрес 0x%08X...", Data, Address);
            FLASH_Unlock();
            FlashStatus = FLASH_ProgramHalfWord (Address, Data);
            FLASH_Lock();
            /* Если операция программирования не удалась, возвращается код ошибки Flash */
            if (FlashStatus != FLASH_COMPLETE) {
                EEPROM_LOG_WRITE_MSG ("    ❌ Ошибка записи данных!");
                return FlashStatus;
            }

            /* Установка виртуального адреса переменной */
            EEPROM_LOG_WRITE_MSG ("    Запись VirtAddr (0x%04X) на адрес 0x%08X...", VirtAddress, Address + 2);
            FLASH_Unlock();
            FlashStatus = FLASH_ProgramHalfWord (Address + 2, VirtAddress);
            FLASH_Lock();
            if (FlashStatus == FLASH_COMPLETE) {
                EEPROM_LOG_WRITE_MSG ("    ✓ Переменная успешно записана");
            } else {
                EEPROM_LOG_WRITE_MSG ("    ❌ Ошибка записи виртуального адреса!");
            }

            /* Возврат статуса операции программирования */
            return FlashStatus;
        } else {
            /* Переход к следующему адресу */
            Address = Address + 4;
            checked_addresses++;
        }
    }

    /* Возврат PAGE_FULL в случае, если валидная страница заполнена */
    EEPROM_LOG_WRITE_MSG ("    ⚠ Страница ЗАПОЛНЕНА (проверено %d адресов)", checked_addresses);
    return PAGE_FULL;
}

/**
 * @brief  Переносит данные последних обновленных переменных из заполненной
 *         страницы в пустую
 * @param  VirtAddress: 16-битный виртуальный адрес переменной
 * @param  Data: 16-битные данные для записи как значение переменной
 * @retval Статус успеха или ошибки:
 *           - FLASH_COMPLETE: при успехе
 *           - PAGE_FULL: если валидная страница заполнена
 *           - NO_VALID_PAGE: если валидная страница не была найдена
 *           - Код ошибки Flash: при ошибке записи во Flash
 */
static uint16_t EE_PageTransfer (uint16_t VirtAddress, uint16_t Data) {
    EEPROM_LOG_TRANSFER_MSG ("╔════════════════════════════════════════╗");
    EEPROM_LOG_TRANSFER_MSG ("║       ПЕРЕНОС СТРАНИЦЫ                 ║");
    EEPROM_LOG_TRANSFER_MSG ("╚════════════════════════════════════════╝");
    EEPROM_LOG_TRANSFER_MSG ("Причина: активная страница заполнена");
    EEPROM_LOG_TRANSFER_MSG ("Новая переменная: VirtAddr=0x%04X, Data=0x%04X", VirtAddress, Data);

    FLASH_Status FlashStatus = FLASH_COMPLETE;
    uint32_t NewPageAddress = EEPROM_START_ADDRESS;
    uint16_t OldPageId = 0;
    uint16_t ValidPage = PAGE0, VarIdx = 0;
    uint16_t EepromStatus = 0, ReadStatus = 0;

    /* Получение активной страницы для операции чтения */
    ValidPage = EE_FindValidPage (READ_FROM_VALID_PAGE);

    if (ValidPage == PAGE1) {
        EEPROM_LOG_TRANSFER_MSG ("Старая страница: PAGE1 (0x%08X)", PAGE1_BASE_ADDRESS);
        EEPROM_LOG_TRANSFER_MSG ("Новая страница: PAGE0 (0x%08X)", PAGE0_BASE_ADDRESS);

        /* Адрес новой страницы, куда будет перенесена переменная */
        NewPageAddress = PAGE0_BASE_ADDRESS;

        /* ID старой страницы, откуда будет взята переменная */
        OldPageId = PAGE1_ID;
    } else if (ValidPage == PAGE0) {
        EEPROM_LOG_TRANSFER_MSG ("Старая страница: PAGE0 (0x%08X)", PAGE0_BASE_ADDRESS);
        EEPROM_LOG_TRANSFER_MSG ("Новая страница: PAGE1 (0x%08X)", PAGE1_BASE_ADDRESS);

        /* Адрес новой страницы, куда будет перенесена переменная */
        NewPageAddress = PAGE1_BASE_ADDRESS;

        /* ID старой страницы, откуда будет взята переменная */
        OldPageId = PAGE0_ID;
    } else {
        EEPROM_LOG_TRANSFER_MSG ("❌ Валидная страница не найдена!");
        return NO_VALID_PAGE; /* Нет валидной страницы */
    }

    /* Установка статуса новой страницы в RECEIVE_DATA */
    EEPROM_LOG_TRANSFER_MSG ("----------------------------------------");
    EEPROM_LOG_TRANSFER_MSG ("Шаг 1: Установка статуса RECEIVE_DATA для новой страницы...");
    FLASH_Unlock();
    FlashStatus = FLASH_ProgramHalfWord (NewPageAddress, RECEIVE_DATA);
    FLASH_Lock();
    /* Если операция программирования не удалась, возвращается код ошибки Flash */
    if (FlashStatus != FLASH_COMPLETE) {
        EEPROM_LOG_TRANSFER_MSG ("❌ Ошибка установки статуса!");
        return FlashStatus;
    }
    EEPROM_LOG_TRANSFER_MSG ("✓ Статус установлен");

    /* Запись переменной, переданной как параметр, в новую активную страницу */
    EEPROM_LOG_TRANSFER_MSG ("Шаг 2: Запись новой переменной в новую страницу...");
    EepromStatus = EE_VerifyPageFullWriteVariable (VirtAddress, Data);
    /* Если операция программирования не удалась, возвращается код ошибки Flash */
    if (EepromStatus != FLASH_COMPLETE) {
        EEPROM_LOG_TRANSFER_MSG ("❌ Ошибка записи новой переменной!");
        return EepromStatus;
    }
    EEPROM_LOG_TRANSFER_MSG ("✓ Новая переменная записана");

    /* Процесс переноса: перенос переменных со старой на новую активную страницу */
    EEPROM_LOG_TRANSFER_MSG ("Шаг 3: Перенос существующих переменных (всего: %d)...", NB_OF_VAR);
    int transferred = 0, skipped = 0;
    for (VarIdx = 0; VarIdx < NB_OF_VAR; VarIdx++) {
        if (VirtAddVarTab[VarIdx] != VirtAddress) {
            EEPROM_LOG_TRANSFER_MSG ("  Переменная %d (VirtAddr: 0x%04X)...", VarIdx, VirtAddVarTab[VarIdx]);

            /* Чтение последних обновлений других переменных */
            ReadStatus = EE_ReadVariable (VirtAddVarTab[VarIdx], &DataVar);
            /* Если переменная, соответствующая виртуальному адресу, была найдена */
            if (ReadStatus != 0x1) {
                EEPROM_LOG_TRANSFER_MSG ("    Найдена, значение: 0x%04X", DataVar);
                /* Перенос переменной на новую активную страницу */
                EepromStatus = EE_VerifyPageFullWriteVariable (VirtAddVarTab[VarIdx], DataVar);
                /* Если операция программирования не удалась, возвращается код ошибки Flash */
                if (EepromStatus != FLASH_COMPLETE) {
                    EEPROM_LOG_TRANSFER_MSG ("    ❌ Ошибка переноса!");
                    return EepromStatus;
                }
                EEPROM_LOG_TRANSFER_MSG ("    ✓ Перенесена");
                transferred++;
            } else {
                EEPROM_LOG_TRANSFER_MSG ("    Не найдена, пропуск");
                skipped++;
            }
        } else {
            EEPROM_LOG_TRANSFER_MSG ("  Переменная %d - уже записана, пропуск", VarIdx);
            skipped++;
        }
    }
    EEPROM_LOG_TRANSFER_MSG ("  Итог: перенесено=%d, пропущено=%d", transferred, skipped);

    /* Стирание старой страницы: установка статуса старой страницы в ERASED */
    EEPROM_LOG_TRANSFER_MSG ("Шаг 4: Стирание старой страницы...");
    FlashStatus = FLASH_EraseSector (OldPageId);
    /* Если операция стирания не удалась, возвращается код ошибки Flash */
    if (FlashStatus != FLASH_COMPLETE) {
        EEPROM_LOG_TRANSFER_MSG ("❌ Ошибка стирания старой страницы!");
        return FlashStatus;
    }
    EEPROM_LOG_TRANSFER_MSG ("✓ Старая страница стерта");

    /* Установка статуса новой страницы в VALID_PAGE */
    EEPROM_LOG_TRANSFER_MSG ("Шаг 5: Установка статуса VALID_PAGE для новой страницы...");
    FLASH_Unlock();
    FlashStatus = FLASH_ProgramHalfWord (NewPageAddress, VALID_PAGE);
    FLASH_Lock();
    /* Если операция программирования не удалась, возвращается код ошибки Flash */
    if (FlashStatus != FLASH_COMPLETE) {
        EEPROM_LOG_TRANSFER_MSG ("❌ Ошибка установки финального статуса!");
        return FlashStatus;
    }
    EEPROM_LOG_TRANSFER_MSG ("✓ Новая страница активирована");

    EEPROM_LOG_TRANSFER_MSG ("----------------------------------------");
    EEPROM_LOG_TRANSFER_MSG ("✓ Перенос страницы завершен успешно");
    EEPROM_LOG_TRANSFER_MSG ("╚════════════════════════════════════════╝");

    /* Возврат статуса последней операции с flash */
    return FlashStatus;
}