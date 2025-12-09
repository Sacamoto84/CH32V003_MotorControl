
//
//  TX  [PD6   PD4] SWIO
//  GNG [VSS   PC4] KEY
//  PWM [PA2   PC2] LED
//  VDD [VDD   PC1] BUZZER
//
// ┌────────┬──────────┬──────────┬─────────────────┐
// │ Сигнал │ Вывод 1  │ Вывод 2  │ Описание        │
// ├────────┼──────────┼──────────┼─────────────────┤
// │ TX     │ PD6      │ PD4      │ SWIO            │
// │ GND    │ VSS      │ PC4      │ KEY (кнопка)    │
// │ PWM    │ PA2      │ PC2      │ LED             │
// │ VDD    │ VDD      │ PC1      │ Buzzer (зуммер) │
// └────────┴──────────┴──────────┴─────────────────┘
//

#include <debug.h>
#include "uButton.h"
#include "pwm.hpp"

#include "eeprom.hpp"

// Создать handle
// EEPROM_HandleTypeDef heeprom = EEPROM_HANDLE_DEFAULT();

uButton b;
Pwm pwm;

uEeprom eeprom_power;              // Мощность мотора
uEeprom eeprom_boostEnable;        // Настройка того что будет использоваться буст
uEeprom eeprom_boostPower;         //
uEeprom eeprom_boostTime;          // Время буста в ms

uint16_t configCurrentPower = 10;  // Текущая мощность 0..100
uint16_t comandMotorOn = 0;        // Признак того что мотор должен работать

extern "C" void tone1 (uint16_t frequency, uint16_t duration_ms);

void TIM1_PWMOut_CH2N_Init (u16 arr, u16 psc, u16 ccp);

void ScreenBoostPower (void);
void ScreenNormal (void);

Screen screen = {Screen::NORMAL};

/* Global define */
// Удобные макросы (можно положить в отдельный .h)
#define CLRscr "\033[2J\033[H"
#define FG(color) "\033[38;5;" #color "m"
#define BG(color) "\033[48;5;" #color "m"
#define RESET1 "\033[0m"
#define BOLD "\033[1m"
#define UNDERLINE "\033[4m"

void EXTI_INT_INIT (void) {
    // EXTI_InitTypeDef EXTI_InitStructure = {0};
    //  RCC_APB2PeriphClockCmd (RCC_APB2Periph_AFIO, ENABLE);
    //  EXTI_InitStructure.EXTI_Line = EXTI_Line9;
    //  EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Event;
    //  EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    //  EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    //  EXTI_Init (&EXTI_InitStructure);


    EXTI_InitTypeDef EXTI_InitStructure = {0};
    // GPIO_InitTypeDef GPIO_InitStructure = {0};

    // Включить AFIO для настройки EXTI
    RCC_APB2PeriphClockCmd (RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC, ENABLE);

    // Настроить PC4 как вход с подтяжкой вверх
    // GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    // GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // Подтяжка к VCC
    // GPIO_Init(GPIOC, &GPIO_InitStructure);

    // Привязать EXTI4 к порту C (PC4)
    GPIO_EXTILineConfig (GPIO_PortSourceGPIOC, GPIO_PinSource4);

    // Настроить EXTI4
    EXTI_InitStructure.EXTI_Line = EXTI_Line4;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Event;          // EVENT, не Interrupt!
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;  // По нажатию (0)
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init (&EXTI_InitStructure);
}

void userEEPROM() {

    printf (".READ CONFIG Power\n");
    eeprom_power.init (0, 0, 100, 50, (char *)"Power");

    printf (".READ CONFIG Boost Enable\n");
    eeprom_boostEnable.init (1, 0, 1, 0, (char *)"B Enable");

    printf (".READ CONFIG Boost Power\n");
    eeprom_boostPower.init (2, 0, 100, 10, (char *)"B Power");


    printf (".READ CONFIG Boost Time\n");
    eeprom_boostTime.init (3, 0, 100, 100, (char *)"B Time");
}

int main (void) {

    // Разблокируем нормальный двухпроводный отладочный интерфейс навсегда
    // (записывается в опции-байты при первой прошивке)
    // SystemCoreClockUpdate();

    init();

    EXTI_INT_INIT();

    // USART_Printf_Init (115200);
    USART_Printf_Init (460800);

    printf ("\r\n---------------------------------------\r\n");
    printf ("Привет SystemClk:%d\r\n", SystemCoreClock);

    userEEPROM();

    //----
    printf ("-------------------------\r\n");
    printf ("CONFIG Power        : %d\r\n", eeprom_power.get());
    printf ("CONFIG Boost Enable : %d\r\n", eeprom_boostEnable.get());
    printf ("CONFIG Boost Power   : %d ms\r\n", eeprom_boostPower.get());
    printf ("CONFIG Boost Time   : %d ms\r\n", eeprom_boostTime.get());

    printf ("-------------------------\r\n");
    //----

    // RCC_ClocksTypeDef RCC_ClocksStatus={0};

    // RCC_LSICmd (ENABLE);
    // while (RCC_GetFlagStatus (RCC_FLAG_LSIRDY) == RESET);
    // PWR_AWU_SetPrescaler (PWR_AWU_Prescaler_128);
    // PWR_AWU_SetWindowValue (10);
    // PWR_AutoWakeUpCmd (ENABLE);
    // PWR_EnterSTANDBYMode (PWR_STANDBYEntry_WFE);

    NVIC_EnableIRQ (SysTick_IRQn);

    SysTick->SR &= ~(1 << 0);
    SysTick->CMP = 1000 - 1;
    SysTick->CNT = 0;
    SysTick->CTLR = 0xb;

    pwm.init (100, 9, 0);
    pwm.setDuty (0);
    pwm.disable();


    // Восходящая трель - "данные сохранены"
    // Двойной тон с акцентом на втором
    tone1_vol (1000, 40, 70);
    tone1_vol (1500, 80, 100);

    // tone1_vol (800, 30, 60);
    // tone1_vol (1000, 30, 70);
    // delay (1000);

    //  tone1_vol(1500, 80, 90);
    // delay(40);
    // tone1_vol(1500, 80, 90);
    // delay(40);
    // tone1_vol(1200, 100, 70);  // Отскок назад

    // // Звук "меньше нельзя"
    // tone1_vol(800, 80, 90);
    // delay(40);
    // tone1_vol(800, 80, 90);
    // delay(40);
    // tone1_vol(1000, 100, 70);  // Отскок назад


    tone1_vol (1200, 40, 80);
    tone1_vol (1000, 40, 70);
    delay (50);
    tone1_vol (1500, 80, 100);  // Сохранено!


    // tone1_vol (1000, 20, 50);
    // tone1_vol (1200, 25, 60);
    // delay (1000);
    // tone1_vol (600, 25, 60);
    // tone1_vol (900, 25, 75);
    // tone1_vol (1200, 40, 90);

    // --- СЛУЖЕБНЫЕ ЗВУКИ ---
    // buzzer_ok();
    // delay (1000);

    // buzzer_error();
    // delay (1000);

    // buzzer_error_strong();
    // delay (1000);

    // buzzer_warning();
    // delay (1000);

    // buzzer_warning_double();
    // delay (1000);

    // buzzer_click();
    // delay (1000);

    // buzzer_success_long();
    // delay (1000);

    // buzzer_critical();
    // delay (1000);

    // buzzer_beepboop();
    // delay (1000);

    // buzzer_access_denied();
    // delay (1000);

    // buzzer_notify();
    // delay (1000);

    gotoDeepSleep();

    printf ("Go...\r\n");

    while (1) {

        // PWR_EnterSTANDBYMode (PWR_STANDBYEntry_WFE);

        b.tick();

        if (screen == Screen::NORMAL) {
            ScreenNormal();
        }

        if (screen == Screen::SET_POWER) {  // 1
            ScreenPower();
        }

        if (screen == Screen::SET_BOOST_ENABLE) {  // 2
            ScreenBoostEnable();
            comandMotorOn = 0;
        }

        if (screen == Screen::SET_BOOST_POWER) {  // 3

            comandMotorOn = 0;
            ScreenBoostPower();
        }

        if (screen == Screen::SET_BOOST_TIME) {  // 4
            comandMotorOn = 0;
            ScreenBoostTime();
        }


        if (comandMotorOn) {
            pwm.setDuty (configCurrentPower);
            pwm.enable();
            LED_ON;
        } else {
            pwm.disable();
            LED_OFF;
        }
    }
}

void gotoDeepSleep (void) {

    GPIO_InitTypeDef GPIO_InitStructure = {0};


    // === КРИТИЧЕСКИ ВАЖНО: Отключить отладку ===
    // Это ДОЛЖНО быть первым!
    RCC_APB2PeriphClockCmd (RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig (GPIO_Remap_SDI_Disable, ENABLE);  // Отключить SWD

    // Включить GPIO и PWR
    // RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD, ENABLE);
    // RCC_APB1PeriphClockCmd (RCC_APB1Periph_PWR, ENABLE);

    // === ВСЕ GPIO В АНАЛОГОВЫЙ РЕЖИМ (минимум тока) ===
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;  // ANALOG INPUT - минимум!

                                                   //  GPIO_Init (GPIOA, &GPIO_InitStructure);


    GPIO_Init (GPIOD, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All & ~GPIO_Pin_4;
    GPIO_Init (GPIOC, &GPIO_InitStructure);
    // // === ОТКЛЮЧИТЬ ВСЕ ПЕРИФЕРИЙНЫЕ БЛОКИ ===
    // // APB2
    // RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOA |
    //                             RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO |
    //                             RCC_APB2Periph_ADC1 | RCC_APB2Periph_TIM1 |
    //                             RCC_APB2Periph_SPI1 | RCC_APB2Periph_USART1,
    //                         DISABLE);

    // // APB1
    RCC_APB1PeriphClockCmd (RCC_APB1Periph_TIM2 | RCC_APB1Periph_WWDG | RCC_APB1Periph_I2C1, DISABLE);

    // === ОТКЛЮЧИТЬ HSI (внутренний генератор 24 МГц) ===
    // Это даст основную экономию!
    //RCC_HSICmd (DISABLE);

    printf ("Заснули\r\n");
    // // === ВОЙТИ В STANDBY БЕЗ ВОЗВРАТА ===
    PWR_EnterSTANDBYMode (PWR_STANDBYEntry_WFI);
   
    printf ("Проснулись\r\n");
    //Возврат
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init (GPIOC, &GPIO_InitStructure);

    //Возврат
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init (GPIOD, &GPIO_InitStructure);



}
