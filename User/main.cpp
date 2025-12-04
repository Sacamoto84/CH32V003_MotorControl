
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
#include "EEPROM.h"
#include "ws2812b_driver.h"

uButton b;
Pwm pwm;

extern "C" void tone1 (uint16_t frequency, uint16_t duration_ms);

void TIM1_PWMOut_CH2N_Init (u16 arr, u16 psc, u16 ccp);

void ScreenBoostPower (void);
void ScreenNormal (void);

Screen screen = {Screen::NORMAL};

uint16_t configBoostEnable = 0;    // Настройка того что будет использоваться буст
uint16_t configBoostTime = 100;    // Время буста в ms
uint16_t configCurrentPower = 10;  // Текущая мощность 0..100

uint16_t comandMotorOn = 0;        // Признак того что мотор должен работать

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

void userInitVarEEPROM (uint8_t id, uint16_t *value, uint16_t def) {

    // Читаем boostEnable
    // Проверка существования переменной
    uint32_t temp = EEPROM_readVar (id);
    // printf ("temp: %d\n", temp);
    if (temp == -1) {
        // Переменной нет
        printf ("EEPROM id:%d ! Not Present !\n", id);
        uint16_t res = EEPROM_saveVar (id, def);  // Сохранить по умолчанию 0
        printf ("EEPROM Save code:%d\n", res);
        uint16_t test = EEPROM_readVar (id);
        printf ("EEPROM Verification id:%d Value: %d\n", id, test);
        *value = def;
    } else {
        printf ("EEPROM id:%d Value:%d\n", id, temp);
        *value = temp;
    }
}

void userEEPROM() {
    EEPROM_init();
    printf ("EEPROM Demo Free: %d\n", get_free_space());

    printf (".READ CONFIG Boost Enable\n");
    userInitVarEEPROM (1, &configBoostEnable, 0);
    printf (".READ CONFIG Boost Time\n");
    userInitVarEEPROM (2, &configBoostTime, 200);
    printf (".READ CONFIG Power\n");
    userInitVarEEPROM (3, &configCurrentPower, 40);
}

int main (void) {

    // Разблокируем нормальный двухпроводный отладочный интерфейс навсегда
    // (записывается в опции-байты при первой прошивке)
    SystemCoreClockUpdate();

    init();

    EXTI_INT_INIT();

    USART_Printf_Init (115200);
   
    printf ("\r\n---------------------------------------\n");
    printf ("SystemClk:%d\r\n", SystemCoreClock);

    userEEPROM();

    //----
    printf ("-------------------------\n");
    printf ("CONFIG Boost Enable : %d\n", configBoostEnable);
    printf ("CONFIG Boost Time   : %d ms\n", configBoostTime);
    printf ("CONFIG Power        : %d\n", configCurrentPower);
    printf ("-------------------------\n");
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

    pwm.init (100, 9, 50);

    tone1_vol (CONS_K, 60, 80);     // г (как к)
    tone1_vol (VOWEL_O, 150, 90);   // о
    tone1_vol (CONS_T, 50, 80);     // т
    tone1_vol (VOWEL_O, 180, 100);  // о (ударный)
    tone1_vol (CONS_V, 80, 75);     // в

    // tone1 (1000, 100);
    // tone1 (1500, 150);

    // // Восходящая гамма с crescendo
    // tone1_vol (523, 80, 40);     // C5 - тихо
    // tone1_vol (659, 80, 60);     // E5 - средне
    // tone1_vol (784, 100, 80);    // G5 - громче
    // tone1_vol (1047, 150, 100);  // C6 - ГРОМКО!

    // delay (1000);

    // // Восходящий паттерн с crescendo
    // tone1_vol (800, 60, 50);     // Тихий старт
    // delay (30);
    // tone1_vol (1000, 60, 70);    // Нарастание
    // delay (30);
    // tone1_vol (1200, 150, 100);  // Яркий финал

    //  --- СЛУЖЕБНЫЕ ЗВУКИ ---
    //  buzzer_ok();
    //   delay (1000);


    // buzzer_error();
    // delay (1000);

    // buzzer_error_strong();
    // delay (1000);

    //  buzzer_warning();
    //  delay (1000);

    //  buzzer_warning_double();
    // // delay (1000);

    //  buzzer_click(); //Короткий
    //  delay (1000);

    //  buzzer_success_long();
    //  delay (1000);

    //  buzzer_critical();
    //  delay (1000);

    //  buzzer_beepboop();
    //  delay (1000);

    //  buzzer_access_denied();
    //  delay (1000);

    //  buzzer_notify();
    //  delay (1000);

    // // Дополнительные стилизованные:
    // buzzer_ios_click();
    // delay (1000);

    // buzzer_android_notify();
    // delay (1000);

    // buzzer_robot();
    // delay (1000);

    // buzzer_microwave_done();
    // delay (1000);

    // buzzer_winxp_msg();
    // delay (1000);

    // buzzer_startup();delay (1000);
    //  buzzer_shutdown();delay (1000);
    //   buzzer_charging();delay (1000);
    //  buzzer_button_hold();delay (1000);


    // gotoDeepSleep();

    printf ("Go...\n");

    while (1) {

        // PWR_EnterSTANDBYMode (PWR_STANDBYEntry_WFE);

        b.tick();

        if (screen == Screen::NORMAL) {
            ScreenNormal();

            if (comandMotorOn) {
                pwm.enable();
                LED_ON;
            } else {
                pwm.disable();
                LED_OFF;
            }
        }

        if (screen == Screen::SET_POWER) {
            ScreenNormal();  // TODO
        }

        if (screen == Screen::SET_BOOST_POWER) {
            ScreenBoostPower();
            comandMotorOn = 0;
        }

        if (screen == Screen::SET_BOOST_ENABLE) {
            ScreenBoostEnable();
            comandMotorOn = 0;
        }
    }
}

void ScreenBoostPower (void) {

    if (b.hold()) {
        screen = Screen::NORMAL;
        buzzer_shutdown();
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
    //  GPIO_Init (GPIOC, &GPIO_InitStructure);
    //  GPIO_Init (GPIOD, &GPIO_InitStructure);

    // // === ОТКЛЮЧИТЬ ВСЕ ПЕРИФЕРИЙНЫЕ БЛОКИ ===
    // // APB2
    // RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOA |
    //                             RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO |
    //                             RCC_APB2Periph_ADC1 | RCC_APB2Periph_TIM1 |
    //                             RCC_APB2Periph_SPI1 | RCC_APB2Periph_USART1,
    //                         DISABLE);

    // // APB1
    // RCC_APB1PeriphClockCmd (RCC_APB1Periph_TIM2 | RCC_APB1Periph_WWDG |
    //                             RCC_APB1Periph_I2C1,
    //                         DISABLE);

    // === ОТКЛЮЧИТЬ HSI (внутренний генератор 24 МГц) ===
    // Это даст основную экономию!
    //  RCC_HSICmd (DISABLE);

    // // === ВОЙТИ В STANDBY БЕЗ ВОЗВРАТА ===
    PWR_EnterSTANDBYMode (PWR_STANDBYEntry_WFI);
}
