#ifndef __DEBUG_H
#define __DEBUG_H

#ifdef __cplusplus
 extern "C" {
#endif

#include <ch32v00x.h>
#include <stdio.h>

/* UART Printf Definition */
#define DEBUG_UART1_NoRemap   1  //Tx-PD5
#define DEBUG_UART1_Remap1    2  //Tx-PD0
#define DEBUG_UART1_Remap2    3  //Tx-PD6
#define DEBUG_UART1_Remap3    4  //Tx-PC0

/* DEBUG UATR Definition */
#ifndef DEBUG
#define DEBUG   DEBUG_UART1_Remap2
#endif

/* SDI Printf Definition */
#define SDI_PR_CLOSE   0
#define SDI_PR_OPEN    1

#ifndef SDI_PRINT
#define SDI_PRINT   SDI_PR_CLOSE
#endif

#define LED_ON GPIO_WriteBit (GPIOC, GPIO_Pin_2, Bit_SET)
#define LED_OFF GPIO_WriteBit (GPIOC, GPIO_Pin_2, Bit_RESET)

#define BUZZER_ON GPIO_WriteBit (GPIOC, GPIO_Pin_1, Bit_SET)
#define BUZZER_OFF GPIO_WriteBit (GPIOC, GPIO_Pin_1, Bit_RESET)

#define KEY GPIO_ReadInputDataBit (GPIOC, GPIO_Pin_4)

void Delay_Init(void);
void Delay_Us(uint32_t n);
void Delay_Ms(uint32_t n);
void USART_Printf_Init(uint32_t baudrate);
void SDI_Printf_Enable(void);

extern uint64_t millisec;

extern void gotoDeepSleep (void);

extern void delay (int time);

#ifdef __cplusplus
}
#endif

#endif /* __DEBUG_H */
