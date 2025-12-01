#include <debug.h>

#include "buzzer_tunes.h"

#include "uButton.h"

#include "EEPROM.h"

uButton b;

Pwm pwm;

extern "C" void tone1 (uint16_t frequency, uint16_t duration_ms);

void TIM1_PWMOut_CH2N_Init (u16 arr, u16 psc, u16 ccp);

void ScreenBoostPower (void);
void ScreenNormal (void);
void ScreenBoostEnable (void);

enum class Screen {
    NORMAL,
    BUSY,
    SET_POWER,
    SET_BOOST_ENABLE,
    SET_BOOST_POWER,
};


Screen screen = {Screen::NORMAL};
uint16_t configBoostEnable = 0;    // §¯§Ñ§ã§ä§â§à§Û§Ü§Ñ §ä§à§Ô§à §é§ä§à §Ò§å§Õ§Ö§ä §Ú§ã§á§à§Ý§î§Ù§à§Ó§Ñ§ä§î§ã§ñ §Ò§å§ã§ä
uint16_t configBoostTime = 100;    // §£§â§Ö§Þ§ñ §Ò§å§ã§ä§Ñ §Ó ms
uint16_t configCurrentPower = 10;  // §´§Ö§Ü§å§ë§Ñ§ñ §Þ§à§ë§ß§à§ã§ä§î 0..100

/* Global define */
// §µ§Õ§à§Ò§ß§í§Ö §Þ§Ñ§Ü§â§à§ã§í (§Þ§à§Ø§ß§à §á§à§Ý§à§Ø§Ú§ä§î §Ó §à§ä§Õ§Ö§Ý§î§ß§í§Û .h)
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

    // §£§Ü§Ý§ð§é§Ú§ä§î AFIO §Õ§Ý§ñ §ß§Ñ§ã§ä§â§à§Û§Ü§Ú EXTI
    RCC_APB2PeriphClockCmd (RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC, ENABLE);

    // §¯§Ñ§ã§ä§â§à§Ú§ä§î PC4 §Ü§Ñ§Ü §Ó§ç§à§Õ §ã §á§à§Õ§ä§ñ§Ø§Ü§à§Û §Ó§Ó§Ö§â§ç
    // GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    // GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // §±§à§Õ§ä§ñ§Ø§Ü§Ñ §Ü VCC
    // GPIO_Init(GPIOC, &GPIO_InitStructure);

    // §±§â§Ú§Ó§ñ§Ù§Ñ§ä§î EXTI4 §Ü §á§à§â§ä§å C (PC4)
    GPIO_EXTILineConfig (GPIO_PortSourceGPIOC, GPIO_PinSource4);

    // §¯§Ñ§ã§ä§â§à§Ú§ä§î EXTI4
    EXTI_InitStructure.EXTI_Line = EXTI_Line4;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Event;          // EVENT, §ß§Ö Interrupt!
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;  // §±§à §ß§Ñ§Ø§Ñ§ä§Ú§ð (0)
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init (&EXTI_InitStructure);
}

void userInitVarEEPROM (uint8_t id, uint16_t *value, uint16_t def) {

    // §¹§Ú§ä§Ñ§Ö§Þ boostEnable
    // §±§â§à§Ó§Ö§â§Ü§Ñ §ã§å§ë§Ö§ã§ä§Ó§à§Ó§Ñ§ß§Ú§ñ §á§Ö§â§Ö§Þ§Ö§ß§ß§à§Û
    uint32_t temp = EEPROM_readVar (id);
    // printf ("temp: %d\n", temp);
    if (temp == -1) {
        // §±§Ö§â§Ö§Þ§Ö§ß§ß§à§Û §ß§Ö§ä
        printf ("EEPROM id:%d ! Not Present !\n", id);
        uint16_t res = EEPROM_saveVar (id, def);  // §³§à§ç§â§Ñ§ß§Ú§ä§î §á§à §å§Þ§à§Ý§é§Ñ§ß§Ú§ð 0
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

    // §²§Ñ§Ù§Ò§Ý§à§Ü§Ú§â§å§Ö§Þ §ß§à§â§Þ§Ñ§Ý§î§ß§í§Û §Õ§Ó§å§ç§á§â§à§Ó§à§Õ§ß§í§Û §à§ä§Ý§Ñ§Õ§à§é§ß§í§Û §Ú§ß§ä§Ö§â§æ§Ö§Û§ã §ß§Ñ§Ó§ã§Ö§Ô§Õ§Ñ
    // (§Ù§Ñ§á§Ú§ã§í§Ó§Ñ§Ö§ä§ã§ñ §Ó §à§á§è§Ú§Ú-§Ò§Ñ§Û§ä§í §á§â§Ú §á§Ö§â§Ó§à§Û §á§â§à§ê§Ú§Ó§Ü§Ö)


    GPIO_InitTypeDef GPIO_InitStructure = {0};

    NVIC_PriorityGroupConfig (NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();
    Delay_Ms (500);

    RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD, ENABLE);
    RCC_APB1PeriphClockCmd (RCC_APB1Periph_PWR, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;

    GPIO_Init (GPIOA, &GPIO_InitStructure);

    // GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 |  GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
    // GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All & ~GPIO_Pin_4;
    // GPIO_Init (GPIOD, &GPIO_InitStructure);

    GPIO_Init (GPIOC, &GPIO_InitStructure);

    // LED
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init (GPIOC, &GPIO_InitStructure);

    // BUZZER
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init (GPIOC, &GPIO_InitStructure);

    // KEY
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init (GPIOC, &GPIO_InitStructure);


    // BUZZER_ON;
    LED_ON;
    Delay_Ms (5);
    LED_OFF;

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

    SystemCoreClockUpdate();

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
    SysTick->CTLR = 0xB;

    // pwm.init (100, 9, 50);

    tone1 (1000, 100);
    tone1 (1500, 150);

    // delay (1000);
    //  --- §³§­§µ§¨§¦§¢§¯§½§¦ §©§£§µ§¬§ª ---
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

    //  buzzer_click(); //§¬§à§â§à§ä§Ü§Ú§Û
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

    // // §¥§à§á§à§Ý§ß§Ú§ä§Ö§Ý§î§ß§í§Ö §ã§ä§Ú§Ý§Ú§Ù§à§Ó§Ñ§ß§ß§í§Ö:
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
        // Delay_Ms(1000);

        // PWR_EnterSTANDBYMode (PWR_STANDBYEntry_WFE);


        b.tick();

        if (screen == Screen::NORMAL) {
            ScreenNormal();
        }

        if (screen == Screen::SET_POWER){
            ScreenNormal();//TODO
        }
        
        if (screen == Screen::SET_BOOST_POWER) {
            ScreenBoostPower();
        }

        if (screen == Screen::SET_BOOST_ENABLE) {
            ScreenBoostEnable();
        }
    }
}

void ScreenNormal (void) {

    if (b.press()) {
        printf ("Press\n");
        LED_ON;
    }

    if (b.click()) {
        printf ("Click\n");
        buzzer_ios_click();
    }

    if (b.hold()) {
        printf ("Hold\n");
        buzzer_warning();
    }

    if (b.releaseHold()) {
        printf ("ReleaseHold\n");
    }

    if (b.step()) {
        printf ("Step\n");

        tone1 (400, 100);
    }


    if (b.releaseStep())
        printf ("releaseStep\n");

    if (b.release()) {
        printf ("Release\n");
        LED_OFF;
    }

    if (b.hasClicks()) {
        printf ("Clicks: %d\n", b.getClicks());


        if (b.getClicks() == 2) {
            // SET_POWER
            screen = Screen::SET_POWER;
        }

        if (b.getClicks() == 3) {
            // SET_POWER
            screen = Screen::SET_POWER;
        }

        if (b.getClicks() == 4) {
            // SET_POWER
            screen = Screen::SET_BOOST_ENABLE;
            buzzer_ios_click();
            delay (200);
            buzzer_ios_click();
            delay (200);
            buzzer_ios_click();
            delay (200);
            buzzer_ios_click();
        }


        if (b.getClicks() == 5) {
            buzzer_shutdown();
            buzzer_shutdown();
            buzzer_shutdown();
            buzzer_shutdown();

            __disable_irq();  // §à§ä§Ü§Ý§ð§é§Ñ§Ö§Þ §Ó§ã§Ö §á§â§Ö§â§í§Ó§Ñ§ß§Ú§ñ
            NVIC_SystemReset();
            while (1);
        }
    }

    if (b.timeout()) {
        printf ("Timeout\n");
        buzzer_robot();
        // gotoDeepSleep();
    }
}

void ScreenBoostPower (void) {

     if (b.hold()) {
        screen = Screen::NORMAL;
        buzzer_shutdown();
    }

}

void ScreenBoostEnable (void) {
    if (configBoostEnable)
        LED_ON;

    else
        LED_OFF;


    if (b.hasClicks()) {
        printf ("Clicks: %d\n", b.getClicks());

        if (b.getClicks() == 2) {
            if (configBoostEnable)
                configBoostEnable = 0;
            else
                configBoostEnable = 1;
            buzzer_ok();
        }
    }

    if (b.hold()) {
        screen = Screen::NORMAL;
        buzzer_shutdown();
    }
}

void gotoDeepSleep (void) {


    GPIO_InitTypeDef GPIO_InitStructure = {0};


    // === §¬§²§ª§´§ª§¹§¦§³§¬§ª §£§¡§¨§¯§°: §°§ä§Ü§Ý§ð§é§Ú§ä§î §à§ä§Ý§Ñ§Õ§Ü§å ===
    // §¿§ä§à §¥§°§­§¨§¯§° §Ò§í§ä§î §á§Ö§â§Ó§í§Þ!
    RCC_APB2PeriphClockCmd (RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig (GPIO_Remap_SDI_Disable, ENABLE);  // §°§ä§Ü§Ý§ð§é§Ú§ä§î SWD

    // §£§Ü§Ý§ð§é§Ú§ä§î GPIO §Ú PWR
    // RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD, ENABLE);
    // RCC_APB1PeriphClockCmd (RCC_APB1Periph_PWR, ENABLE);

    // === §£§³§¦ GPIO §£ §¡§¯§¡§­§°§¤§°§£§½§« §²§¦§¨§ª§® (§Þ§Ú§ß§Ú§Þ§å§Þ §ä§à§Ü§Ñ) ===
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;  // ANALOG INPUT - §Þ§Ú§ß§Ú§Þ§å§Þ!

                                                   //  GPIO_Init (GPIOA, &GPIO_InitStructure);
    //  GPIO_Init (GPIOC, &GPIO_InitStructure);
    //  GPIO_Init (GPIOD, &GPIO_InitStructure);

    // // === §°§´§¬§­§À§¹§ª§´§¾ §£§³§¦ §±§¦§²§ª§¶§¦§²§ª§«§¯§½§¦ §¢§­§°§¬§ª ===
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

    // === §°§´§¬§­§À§¹§ª§´§¾ HSI (§Ó§ß§å§ä§â§Ö§ß§ß§Ú§Û §Ô§Ö§ß§Ö§â§Ñ§ä§à§â 24 §®§¤§è) ===
    // §¿§ä§à §Õ§Ñ§ã§ä §à§ã§ß§à§Ó§ß§å§ð §ï§Ü§à§ß§à§Þ§Ú§ð!
    //  RCC_HSICmd (DISABLE);

    // // === §£§°§«§´§ª §£ STANDBY §¢§¦§© §£§°§©§£§²§¡§´§¡ ===
    PWR_EnterSTANDBYMode (PWR_STANDBYEntry_WFI);
}
