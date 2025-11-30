#include <debug.h>

#include "buzzer_tunes.h"

#include "uButton.h"

uButton b;

extern "C" void tone1 (uint16_t frequency, uint16_t duration_ms);

void TIM1_PWMOut_CH2N_Init (u16 arr, u16 psc, u16 ccp);

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
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Event;                 // EVENT, §ß§Ö Interrupt!
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;  // §±§à §ß§Ñ§Ø§Ñ§ä§Ú§ð (0)
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init (&EXTI_InitStructure);
}

int main (void) {

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
    GPIO_Init (GPIOD, &GPIO_InitStructure);
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
    // LED_ON;
    // Delay_Ms (5);
    // BUZZER_OFF;
    // LED_OFF;

    EXTI_INT_INIT();
    // PWR_EnterSTANDBYMode (PWR_STANDBYEntry_WFI);

    USART_Printf_Init (115200);

    printf ("SystemClk:%d\r\n", SystemCoreClock);
    // printf ("ChipID:%08x\r\n", DBGMCU_GetCHIPID());
    // printf ("Standby Mode Test\r\n");
    // printf ("0x1FFFF800-%08x\r\n", *(u32 *)0x1FFFF800);

    // RCC_ClocksTypeDef RCC_ClocksStatus={0};

    SystemCoreClockUpdate();

    // printf ("SystemClk:%d\r\n", SystemCoreClock);
    // printf ("ChipID:%08x\r\n", DBGMCU_GetCHIPID());

    // RCC_GetClocksFreq (&RCC_ClocksStatus);
    // printf ("SYSCLK_Frequency:%d\r\n", RCC_ClocksStatus.SYSCLK_Frequency);
    // printf ("HCLK_Frequency:%d\r\n", RCC_ClocksStatus.HCLK_Frequency);
    // printf ("PCLK1_Frequency:%d\r\n", RCC_ClocksStatus.PCLK1_Frequency);
    // printf ("PCLK2_Frequency:%d\r\n", RCC_ClocksStatus.PCLK2_Frequency);
    // //Delay_Ms(5500);

    // RCC_LSICmd (ENABLE);
    // while (RCC_GetFlagStatus (RCC_FLAG_LSIRDY) == RESET);
    // PWR_AWU_SetPrescaler (PWR_AWU_Prescaler_128);
    // PWR_AWU_SetWindowValue (10);
    // PWR_AutoWakeUpCmd (ENABLE);
    // PWR_EnterSTANDBYMode (PWR_STANDBYEntry_WFE);

    // #if (SDI_PRINT == SDI_PR_OPEN)
    //     SDI_Printf_Enable();
    // #else
    //     USART_Printf_Init (115200);
    // #endif
    // printf ("\r\n Auto wake up \r\n");

    // beep_Coin();


    NVIC_EnableIRQ (SysTick_IRQn);
    SysTick->SR &= ~(1 << 0);
    SysTick->CMP = 1000 - 1;
    SysTick->CNT = 0;
    SysTick->CTLR = 0xB;


    TIM1_PWMOut_CH2N_Init (100, 9, 50);

    // §µ§ã§á§Ö§ç
    //     tone1(1500, 80);
    //  delay(50);
    //  tone1(2000, 80);

    // Coin pickup (Mario)
    //  tone1(1500, 60);
    //  tone1(2000, 80);

    // Jump
    //  tone1(800, 50);
    //  delay(30);
    //  tone1(1200, 70);

    tone1 (1800, 30);
    delay (1150);
    tone1 (400, 100);
    delay (1150);
    tone1 (1200, 40);
    delay (1150);
    tone1 (1200, 35);
    tone1 (1600, 35);
    delay (1150);


    tone1 (1000, 40);
    tone1 (1400, 40);
    tone1 (1800, 60);

    delay (1150);

    tone1 (1300, 60);
    tone1 (1700, 60);
    tone1 (2000, 80);
    // tone1(1319, 125);  // E
    // tone1(1175, 125);  // D
    // tone1(740, 250);   // F#
    // tone1(831, 250);   // G#
    // delay(125);
    // tone1(1245, 125);  // C#
    // tone1(1109, 125);  // B
    // tone1(587, 250);   // D
    // tone1(740, 250);   // E
    // delay(125);
    // tone1(1109, 125);  // B
    // tone1(1046, 125);  // A
    // tone1(554, 250);   // C#
    // tone1(740, 250);   // E
    // tone1(988, 500);   // D


    //  beep_PowerOn();
    // delay(2000);
    // beep_PowerOff();
    //  delay(2000);
    // delay(2000);
    //  beep_Click();
    // delay(2000);
    //  beep_OK();
    // delay(2000);


    // PWR_EnterSTANDBYMode(PWR_STANDBYEntry_WFE);

   // gotoDeepSleep();

    while (1) {
        // Delay_Ms(1000);

        // PWR_EnterSTANDBYMode (PWR_STANDBYEntry_WFE);
        //  sleep_count++;

        // printf ("SysTick %d\r\n", millisec);


        b.tick();


        if (b.press()) {
            printf ("Press\n");
            LED_ON;
        }
        if (b.click())
            printf ("Click\n");
        if (b.hold())
            printf ("Hold\n");
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
        }
        if (b.timeout()) {
            printf ("Timeout\n");
           // gotoDeepSleep();
        }
        // printf ("Run in main\r\n");
        //  printf (BOLD FG (226) "ZEPHYR + RTT 256\r\n" RESET);
        // Delay_Ms (1);
        //   printf ("Run in main\r\n");
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

/* PWM Output Mode Definition */
#define PWM_MODE1 0
#define PWM_MODE2 1

/* PWM Output Mode Selection */
// #define PWM_MODE PWM_MODE1
#define PWM_MODE PWM_MODE2

/*********************************************************************
 * @fn      TIM1_PWMOut_Init
 *
 * @brief   Initializes TIM1 PWM Output.
 *
 * @param   arr - the period value.
 *          psc - the prescaler value.
 *          ccp - the pulse value.
 *
 * @return  none
 */
void TIM1_PWMOut_CH2N_Init(uint16_t arr, uint16_t psc, uint16_t ccp)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    TIM_OCInitTypeDef TIM_OCInitStructure = {0};

    /* 1) §´§Ñ§Ü§ä§Ú§â§à§Ó§Ñ§ß§Ú§Ö: GPIOA, AFIO §Ú TIM1 (TIM1 §ß§Ñ APB2 §å CH32V003) */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO | RCC_APB2Periph_TIM1, ENABLE);

    /* 2) §¯§Ñ§ã§ä§â§à§Û§Ü§Ñ §á§Ú§ß§Ñ PA2 §Ü§Ñ§Ü AF Push-Pull */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;         // PA2 -> TIM1_CH2N
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;   // Alternate Function Push-Pull
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 3) §¢§Ñ§Ù§à§Ó§Ñ§ñ §ß§Ñ§ã§ä§â§à§Û§Ü§Ñ §ä§Ñ§Û§Þ§Ö§â§Ñ */
    TIM_TimeBaseInitStructure.TIM_Period = arr;
    TIM_TimeBaseInitStructure.TIM_Prescaler = psc;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);

    /* 4) §¬§Ñ§ß§Ñ§Ý 2 (OC2), PWM §â§Ö§Ø§Ú§Þ */
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;       // §à§ã§ß§à§Ó§ß§à§Û §Ó§í§ç§à§Õ (OC2)
    TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable;     // N-§Ó§í§ç§à§Õ (OC2N) ¡ª §Ó§Ñ§Ø§ß§à
    TIM_OCInitStructure.TIM_Pulse = ccp;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
    TIM_OC2Init(TIM1, &TIM_OCInitStructure);

    /* 5) §£§Ü§Ý§ð§é§Ñ§Ö§Þ §á§â§Ö§Õ§Ù§Ñ§Ô§â§å§Ù§Ü§å §Õ§Ý§ñ CCR §Ú ARR */
    TIM_OC2PreloadConfig(TIM1, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM1, ENABLE);

    /* 6) §Á§Ó§ß§à §Ù§Ñ§á§Ú§ã§í§Ó§Ñ§Ö§Þ CCR2 (§ß§Ñ §Ó§ã§ñ§Ü§Ú§Û §ã§Ý§å§é§Ñ§Û) */
    TIM_SetCompare2(TIM1, ccp);

    /* 7) §£§Ü§Ý§ð§é§Ñ§Ö§Þ §Ô§Ý§Ñ§Ó§ß§í§Û §Ó§í§ç§à§Õ (MOE) ¡ª §à§Ò§ñ§Ù§Ñ§ä§Ö§Ý§î§ß§à §Õ§Ý§ñ TIM1 complementary outputs */
    TIM_CtrlPWMOutputs(TIM1, ENABLE);

    /* 8) §£§Ü§Ý§ð§é§Ñ§Ö§Þ §ä§Ñ§Û§Þ§Ö§â */
    TIM_Cmd(TIM1, ENABLE);
}


void PWM_SetDuty(uint16_t duty)
{
    TIM_SetCompare2(TIM1, duty);
}

void PWM_Enable()
{
    TIM_Cmd(TIM1, ENABLE);
}

void PWM_Disable()
{
    TIM_Cmd(TIM1, DISABLE);  // §ä§Ñ§Û§Þ§Ö§â §à§ã§ä§Ñ§ß§à§Ó§Ý§Ö§ß
}
