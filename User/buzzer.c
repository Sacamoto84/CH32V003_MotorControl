#include "buzzer_tunes.h"

extern void delay (int time);

void delayUsTone(uint32_t n)
{
    uint32_t i;

    SysTick->SR &= ~(1 << 0);
    i = (uint32_t)n;

    SysTick->CMP = 9999999;
    SysTick->CNT = 0;
    //SysTick->CTLR |=(1 << 0);

    while(SysTick->CNT <= i);
    //SysTick->CTLR &= ~(1 << 0);
    //SysTick->CTLR = 0xB;
    
    SysTick->CNT = 0;
    SysTick->CMP = 999;
    
}

// §¤§Ö§ß§Ö§â§Ñ§è§Ú§ñ §ä§à§ß§Ñ §Ù§Ñ§Õ§Ñ§ß§ß§à§Û §é§Ñ§ã§ä§à§ä§í §Ú §Õ§Ý§Ú§ä§Ö§Ý§î§ß§à§ã§ä§Ú
void tone1(uint16_t frequency, uint16_t duration_ms) {
    if(frequency == 0) {
        BUZZER_OFF;
        delay(duration_ms);
        return;
    }
    
    uint32_t period_us = 1000000UL / frequency;
    uint32_t half_period_us = period_us / 2;
    uint32_t cycles = (uint32_t)duration_ms * 1000UL / period_us;
    
    for(uint32_t i = 0; i < cycles; i++) {
        BUZZER_ON;
        delayUsTone(half_period_us);
        BUZZER_OFF;
        delayUsTone(half_period_us);
    }
}

// §±§Ñ§å§Ù§Ñ (silence)
void silence(uint16_t duration_ms) {
    BUZZER_OFF;
    delay(duration_ms);
}