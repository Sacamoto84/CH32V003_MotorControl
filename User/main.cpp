/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2023/12/25
 * Description        : Main program body.
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/*
 *@Note
 *Low power, standby mode routine:
 *AWU automatically wakes up
 *This example demonstrates that WFI enters standby mode and wakes up automatically.
 *Note: In order to reduce power consumption as much as possible, it is recommended
 *to set the unused GPIO to pull-down mode.
 *
 */

#include "debug.h"

#include "uButton.h"

uButton b;

extern uint32_t millisec;

void gotoDeepSleep (void);

/* Global define */
// §µ§Õ§à§Ò§ß§í§Ö §Þ§Ñ§Ü§â§à§ã§í (§Þ§à§Ø§ß§à §á§à§Ý§à§Ø§Ú§ä§î §Ó §à§ä§Õ§Ö§Ý§î§ß§í§Û .h)
#define CLRscr "\033[2J\033[H"
#define FG(color) "\033[38;5;" #color "m"
#define BG(color) "\033[48;5;" #color "m"
#define RESET1 "\033[0m"
#define BOLD "\033[1m"
#define UNDERLINE "\033[4m"


#define LED_ON GPIO_WriteBit (GPIOC, GPIO_Pin_2, Bit_SET)
#define LED_OFF GPIO_WriteBit (GPIOC, GPIO_Pin_2, Bit_RESET)

#define BUZZER_ON GPIO_WriteBit (GPIOC, GPIO_Pin_1, Bit_SET)
#define BUZZER_OFF GPIO_WriteBit (GPIOC, GPIO_Pin_1, Bit_RESET)

#define KEY GPIO_ReadInputDataBit (GPIOC, GPIO_Pin_4)

void EXTI_INT_INIT (void) {

    //EXTI_InitTypeDef EXTI_InitStructure = {0};
    // RCC_APB2PeriphClockCmd (RCC_APB2Periph_AFIO, ENABLE);
    // EXTI_InitStructure.EXTI_Line = EXTI_Line9;
    // EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Event;
    // EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    // EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    // EXTI_Init (&EXTI_InitStructure);


    EXTI_InitTypeDef EXTI_InitStructure = {0};
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    
    // §£§Ü§Ý§ð§é§Ú§ä§î AFIO §Õ§Ý§ñ §ß§Ñ§ã§ä§â§à§Û§Ü§Ú EXTI
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC, ENABLE);
    
    // §¯§Ñ§ã§ä§â§à§Ú§ä§î PC4 §Ü§Ñ§Ü §Ó§ç§à§Õ §ã §á§à§Õ§ä§ñ§Ø§Ü§à§Û §Ó§Ó§Ö§â§ç
    //GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    //GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // §±§à§Õ§ä§ñ§Ø§Ü§Ñ §Ü VCC
    //GPIO_Init(GPIOC, &GPIO_InitStructure);
    
    // §±§â§Ú§Ó§ñ§Ù§Ñ§ä§î EXTI4 §Ü §á§à§â§ä§å C (PC4)
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource4);
    
    // §¯§Ñ§ã§ä§â§à§Ú§ä§î EXTI4
    EXTI_InitStructure.EXTI_Line = EXTI_Line4;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Event;  // EVENT, §ß§Ö Interrupt!
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;  // §±§à §ß§Ñ§Ø§Ñ§ä§Ú§ð (0)
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);



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

    BUZZER_ON;
    LED_ON;
    Delay_Ms (5);
    BUZZER_OFF;
    LED_OFF;

    EXTI_INT_INIT();
   // PWR_EnterSTANDBYMode (PWR_STANDBYEntry_WFI);

#if (SDI_PRINT == SDI_PR_OPEN)
    SDI_Printf_Enable();
#else
    USART_Printf_Init (115200);
#endif
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

    NVIC_EnableIRQ (SysTick_IRQn);
    SysTick->SR &= ~(1 << 0);
    SysTick->CMP = 1000 - 1;
    SysTick->CNT = 0;
    SysTick->CTLR = 0xB;


    //PWR_EnterSTANDBYMode(PWR_STANDBYEntry_WFE);

    gotoDeepSleep();

    while (1) {
        // Delay_Ms(1000);

        //PWR_EnterSTANDBYMode (PWR_STANDBYEntry_WFE);
        // sleep_count++;

        // printf ("SysTick %d\r\n", millisec);


        b.tick();


        if (b.press()){
            printf ("Press\n");
            LED_ON;}
        if (b.click())
            printf ("Click\n");
        if (b.hold())
            printf ("Hold\n");
        if (b.releaseHold()){
            printf ("ReleaseHold\n");
           
        }
        if (b.step())
            printf ("Step\n");
        if (b.releaseStep())
            printf ("releaseStep\n");
        if (b.release()){
            printf ("Release\n");
            LED_OFF;
        }
        if (b.hasClicks()) {
            printf ("Clicks: %d\n", b.getClicks());
        }
        if (b.timeout()){
            printf ("Timeout\n");
             gotoDeepSleep();
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
    //RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD, ENABLE);
    //RCC_APB1PeriphClockCmd (RCC_APB1Periph_PWR, ENABLE);

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
