/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch32v00x_it.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2023/12/25
 * Description        : Main Interrupt Service Routines.
*********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for 
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/
#include <ch32v00x_it.h>

void NMI_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void SysTick_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

/*********************************************************************
 * @fn      NMI_Handler
 *
 * @brief   This function handles NMI exception.
 *
 * @return  none
 */
void NMI_Handler(void)
{
  while (1)
  {
    printf("NMI_Handler\r\n");
  }
}

/*********************************************************************
 * @fn      HardFault_Handler
 *
 * @brief   This function handles Hard Fault exception.
 *
 * @return  none
 */
void HardFault_Handler(void)
{
  NVIC_SystemReset();
  while (1)
  {
    printf("HardFault_Handler\r\n");
  }
}

uint64_t millisec = 0;

/*********************************************************************
 * @fn      SysTick_IRQHandler
 *
 * @brief   SysTick Interrupt Service Function.
 *
 * @return  none
 */
void SysTick_Handler(void)
{
    //printf("Systick\r\n");
    millisec++;
    SysTick->SR = 0;
}

// §²§Ñ§ã§é§Ö§ä §Ó§â§Ö§Þ§Ö§ß§Ú §ã§ß§Ñ
// LSI = 128 §Ü§¤§è
// Prescaler = 128 ¡ú freq = 128000/128 = 1000 §¤§è
// WindowValue = 10 ¡ú §á§Ö§â§Ú§à§Õ = 10/1000 = 10 §Þ§ã




