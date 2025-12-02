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




// §¤§Ö§ß§Ö§â§Ñ§è§Ú§ñ §ä§à§ß§Ñ §ã §Ô§â§à§Þ§Ü§à§ã§ä§î§ð (0-100%)
void tone1_vol(uint16_t frequency, uint16_t duration_ms, uint8_t volume) {
    if(frequency == 0 || volume == 0) {
        BUZZER_OFF;
        delay(duration_ms);
        return;
    }
    
    // §°§Ô§â§Ñ§ß§Ú§é§Ú§ä§î §Ô§â§à§Þ§Ü§à§ã§ä§î 0-100%
    if(volume > 100) volume = 100;
    
    uint32_t period_us = 1000000UL / frequency;
    uint32_t half_period_us = period_us / 2;
    uint32_t cycles = (uint32_t)duration_ms * 1000UL / period_us;
    
    // §£§â§Ö§Þ§ñ ON §Ó §á§â§à§è§Ö§ß§ä§Ñ§ç §à§ä §á§à§Ý§å§á§Ö§â§Ú§à§Õ§Ñ (duty cycle)
    uint32_t on_time_us = (half_period_us * volume) / 100;
    uint32_t off_time_us = half_period_us - on_time_us;
    
    for(uint32_t i = 0; i < cycles; i++) {
        // §±§à§Ý§à§Ø§Ú§ä§Ö§Ý§î§ß§Ñ§ñ §á§à§Ý§å§Ó§à§Ý§ß§Ñ
        BUZZER_ON;
        delayUsTone(on_time_us);
        BUZZER_OFF;
        delayUsTone(off_time_us);
        
        // §°§ä§â§Ú§è§Ñ§ä§Ö§Ý§î§ß§Ñ§ñ §á§à§Ý§å§Ó§à§Ý§ß§Ñ (§á§Ñ§å§Ù§Ñ)
        delayUsTone(half_period_us);
    }
}



















// ------------------------
//   §³§­§µ§¨§¦§¢§¯§½§¦ §©§£§µ§¬§ª
// ------------------------

void buzzer_ok(void) {
    tone1(1000, 80);
    tone1(1500, 80);
}


void buzzer_error(void) {
    tone1(400, 150);
    tone1(200, 200);
}

void buzzer_error_strong(void) {
    tone1(300, 200);
    tone1(0, 50);
    tone1(300, 200);
}

void buzzer_warning(void) {
    tone1(800, 150);
}

void buzzer_warning_double(void) {
    tone1(800, 120);
    tone1(0, 60);
    tone1(800, 120);
}

void buzzer_click(void) {
    tone1(1200, 40);
}

void buzzer_success_long(void) {
    tone1(800, 150);
    tone1(1000, 150);
    tone1(1200, 200);
}

void buzzer_critical(void) {
    for (int i = 0; i < 3; i++) {
        tone1(250, 200);
        tone1(0, 80);
    }
}

void buzzer_beepboop(void) {
    tone1(1000, 60);
    tone1(700, 60);
}

void buzzer_access_denied(void) {
    tone1(600, 70);
    tone1(400, 120);
}

void buzzer_notify(void) {
    tone1(1000, 100);
    tone1(800, 150);
}

// ------------------------
//   §³§´§ª§­§ª§©§°§£§¡§¯§¯§½§¦ §©§£§µ§¬§ª
// ------------------------

void buzzer_ios_click(void) {
    tone1(1800, 25);
}

void buzzer_android_notify(void) {
    tone1(1200, 70);
    tone1(1500, 120);
}

void buzzer_robot(void) {
    tone1(800, 70);
    tone1(1000, 70);
    tone1(600, 90);
}

void buzzer_microwave_done(void) {
    tone1(1000, 200);
    tone1(0, 100);
    tone1(1000, 200);
}

void buzzer_winxp_msg(void) {
    tone1(900, 80);
    tone1(1200, 80);
    tone1(1000, 120);
}


// ------------------------
//     STARTUP
// ------------------------
// §®§ñ§Ô§Ü§Ñ§ñ §Ó§à§ã§ç§à§Õ§ñ§ë§Ñ§ñ §Þ§Ö§Ý§à§Õ§Ú§ñ, §Ü§Ñ§Ü §Ó§Ü§Ý§ð§é§Ö§ß§Ú§Ö §å§ã§ä§â§à§Û§ã§ä§Ó§Ñ
void buzzer_startup(void) {
    tone1(800, 120);
    tone1(1000, 120);
    tone1(1300, 160);
}

// ------------------------
//     SHUTDOWN
// ------------------------
// §°§Ò§â§Ñ§ä§ß§Ñ§ñ, §ß§Ú§ã§ç§à§Õ§ñ§ë§Ñ§ñ
void buzzer_shutdown(void) {
    tone1(1200, 150);
    tone1(900, 150);
    tone1(600, 180);
}

// ------------------------
//     CHARGING
// ------------------------
// §©§Ó§å§Ü "§á§Ú§ß§Ô", §Ü§Ñ§Ü §á§à§Õ§Ü§Ý§ð§é§Ö§ß§Ú§Ö §á§Ú§ä§Ñ§ß§Ú§ñ
void buzzer_charging(void) {
    tone1(1500, 80);
    tone1(1800, 150);
}

// ------------------------
//     BUTTON HOLD
// ------------------------
// §¯§Ñ§â§Ñ§ã§ä§Ñ§ð§ë§Ú§Û "§Õ§å§Õ§å-§Õ§å§Õ§å" §á§â§Ú §å§Õ§Ö§â§Ø§Ñ§ß§Ú§Ú §Ü§ß§à§á§Ü§Ú
void buzzer_button_hold(void) {
    for (int i = 0; i < 2; i++) {
        tone1(700, 80);
        tone1(900, 80);
    }
}



// === 0 - §¯§°§­§¾ ===
void say_Zero(void) {
    // §¯-§°-§­-§¾
    tone1_vol(CONS_N, 80, 70);   // §ß
    tone1_vol(VOWEL_O, 200, 90); // §à
    tone1_vol(CONS_N, 100, 70);  // §Ý (§Ü§Ñ§Ü §ß)
    tone1_vol(VOWEL_I, 80, 50);  // §î (§Þ§ñ§Ô§Ü§Ú§Û)
}

// === 1 - §°§¥§ª§¯ ===
void say_One(void) {
    // §°-§¥-§ª-§¯
    tone1_vol(VOWEL_O, 150, 80);  // §à
    tone1_vol(CONS_D, 60, 90);    // §Õ
    tone1_vol(VOWEL_I, 180, 90);  // §Ú (§å§Õ§Ñ§â§ß§í§Û)
    tone1_vol(CONS_N, 100, 70);   // §ß
}

// === 2 - §¥§£§¡ ===
void say_Two(void) {
    // §¥-§£-§¡
    tone1_vol(CONS_D, 70, 90);    // §Õ
    tone1_vol(CONS_V, 60, 80);    // §Ó
    tone1_vol(VOWEL_A, 200, 100); // §Ñ (§å§Õ§Ñ§â§ß§í§Û)
}

// === 3 - §´§²§ª ===
void say_Three(void) {
    // §´-§²-§ª
    tone1_vol(CONS_T, 50, 85);    // §ä
    tone1_vol(CONS_R, 90, 80);    // §â (§Ó§Ú§Ò§â§Ñ§è§Ú§ñ)
    tone1_vol(CONS_R, 90, 75);    // §â (§á§â§à§Õ§à§Ý§Ø§Ö§ß§Ú§Ö)
    tone1_vol(VOWEL_I, 180, 100); // §Ú (§å§Õ§Ñ§â§ß§í§Û)
}

// === 4 - §¹§¦§´§½§²§¦ ===
void say_Four(void) {
    // §¹-§¦-§´-§½-§²-§¦
    tone1_vol(CONS_SH, 80, 75);   // §é (§Ü§Ñ§Ü §Þ§ñ§Ô§Ü§à§Ö §ê)
    tone1_vol(VOWEL_E, 120, 85);  // §Ö
    tone1_vol(CONS_T, 50, 70);    // §ä
    tone1_vol(VOWEL_Y, 150, 90);  // §í (§å§Õ§Ñ§â§ß§í§Û)
    tone1_vol(CONS_R, 80, 70);    // §â
    tone1_vol(VOWEL_E, 100, 60);  // §Ö
}

// === 5 - §±§Á§´§¾ ===
void say_Five(void) {
    // §±-§Á-§´-§¾
    tone1_vol(CONS_P, 70, 80);    // §á
    tone1_vol(VOWEL_A, 180, 100); // §ñ (§Ü§Ñ§Ü §Ñ, §å§Õ§Ñ§â§ß§í§Û)
    tone1_vol(CONS_T, 60, 75);    // §ä
    tone1_vol(VOWEL_I, 80, 50);   // §î
}

// === 6 - §º§¦§³§´§¾ ===
void say_Six(void) {
    // §º-§¦-§³-§´-§¾
    tone1_vol(CONS_SH, 100, 85);  // §ê
    tone1_vol(VOWEL_E, 180, 100); // §Ö (§å§Õ§Ñ§â§ß§í§Û)
    tone1_vol(CONS_S, 80, 80);    // §ã
    tone1_vol(CONS_T, 50, 70);    // §ä
    tone1_vol(VOWEL_I, 60, 50);   // §î
}

// === 7 - §³§¦§®§¾ ===
void say_Seven(void) {
    // §³-§¦-§®-§¾
    tone1_vol(CONS_S, 90, 80);    // §ã
    tone1_vol(VOWEL_E, 200, 100); // §Ö (§å§Õ§Ñ§â§ß§í§Û)
    tone1_vol(CONS_M, 120, 80);   // §Þ
    tone1_vol(VOWEL_I, 70, 50);   // §î
}

// === 8 - §£§°§³§¦§®§¾ ===
void say_Eight(void) {
    // §£-§°-§³-§¦-§®-§¾
    tone1_vol(CONS_V, 70, 75);    // §Ó
    tone1_vol(VOWEL_O, 150, 90);  // §à
    tone1_vol(CONS_S, 70, 80);    // §ã
    tone1_vol(VOWEL_E, 180, 100); // §Ö (§å§Õ§Ñ§â§ß§í§Û)
    tone1_vol(CONS_M, 100, 80);   // §Þ
    tone1_vol(VOWEL_I, 70, 50);   // §î
}

// === 9 - §¥§¦§£§Á§´§¾ ===
void say_Nine(void) {
    // §¥-§¦-§£-§Á-§´-§¾
    tone1_vol(CONS_D, 60, 80);    // §Õ
    tone1_vol(VOWEL_E, 120, 85);  // §Ö
    tone1_vol(CONS_V, 60, 75);    // §Ó
    tone1_vol(VOWEL_A, 180, 100); // §ñ (§å§Õ§Ñ§â§ß§í§Û)
    tone1_vol(CONS_T, 50, 70);    // §ä
    tone1_vol(VOWEL_I, 70, 50);   // §î
}

// === 10 - §¥§¦§³§Á§´§¾ ===
void say_Ten(void) {
    // §¥-§¦-§³-§Á-§´-§¾
    tone1_vol(CONS_D, 60, 80);    // §Õ
    tone1_vol(VOWEL_E, 180, 100); // §Ö (§å§Õ§Ñ§â§ß§í§Û)
    tone1_vol(CONS_S, 70, 80);    // §ã
    tone1_vol(VOWEL_A, 120, 85);  // §ñ
    tone1_vol(CONS_T, 50, 70);    // §ä
    tone1_vol(VOWEL_I, 70, 50);   // §î
}

// === 11 - §°§¥§ª§¯§¯§¡§¥§¸§¡§´§¾ ===
void say_Eleven(void) {
    // §°-§¥§ª§¯-§¯§¡-§¥§¸§¡-§´§¾
    tone1_vol(VOWEL_O, 120, 75);  // §à
    tone1_vol(CONS_D, 50, 80);    // §Õ
    tone1_vol(VOWEL_I, 150, 90);  // §Ú
    tone1_vol(CONS_N, 80, 80);    // §ß
    tone1_vol(VOWEL_A, 100, 75);  // §ß§Ñ
    tone1_vol(CONS_D, 50, 70);    // §Õ
    tone1_vol(CONS_T, 50, 70);    // §è
    tone1_vol(VOWEL_A, 150, 90);  // §Ñ (§å§Õ§Ñ§â§ß§í§Û)
    tone1_vol(CONS_T, 50, 65);    // §ä
    tone1_vol(VOWEL_I, 60, 50);   // §î
}

// === 20 - §¥§£§¡§¥§¸§¡§´§¾ ===
void say_Twenty(void) {
    // §¥§£§¡-§¥§¸§¡-§´§¾
    tone1_vol(CONS_D, 60, 85);    // §Õ
    tone1_vol(CONS_V, 50, 80);    // §Ó
    tone1_vol(VOWEL_A, 150, 95);  // §Ñ
    tone1_vol(CONS_D, 50, 75);    // §Õ
    tone1_vol(CONS_T, 50, 75);    // §è
    tone1_vol(VOWEL_A, 180, 100); // §Ñ (§å§Õ§Ñ§â§ß§í§Û)
    tone1_vol(CONS_T, 50, 70);    // §ä
    tone1_vol(VOWEL_I, 60, 50);   // §î
}

// === 30 - §´§²§ª§¥§¸§¡§´§¾ ===
void say_Thirty(void) {
    // §´§²§ª-§¥§¸§¡-§´§¾
    tone1_vol(CONS_T, 45, 85);    // §ä
    tone1_vol(CONS_R, 80, 80);    // §â
    tone1_vol(VOWEL_I, 150, 90);  // §Ú
    tone1_vol(CONS_D, 50, 75);    // §Õ
    tone1_vol(CONS_T, 50, 75);    // §è
    tone1_vol(VOWEL_A, 180, 100); // §Ñ (§å§Õ§Ñ§â§ß§í§Û)
    tone1_vol(CONS_T, 50, 70);    // §ä
    tone1_vol(VOWEL_I, 60, 50);   // §î
}

// === 100 - §³§´§° ===
void say_Hundred(void) {
    // §³-§´-§°
    tone1_vol(CONS_S, 90, 85);    // §ã
    tone1_vol(CONS_T, 50, 80);    // §ä
    tone1_vol(VOWEL_O, 200, 100); // §à (§å§Õ§Ñ§â§ß§í§Û)
}



