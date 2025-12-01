#ifndef BUZZER_TUNES_H
#define BUZZER_TUNES_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "debug.h"

// §®§Ñ§Ü§â§à§ã§í §Õ§Ý§ñ §å§á§â§Ñ§Ó§Ý§Ö§ß§Ú§ñ buzzer (§à§á§â§Ö§Õ§Ö§Ý§Ú§ä§Ö §Ó main.h §Ú§Ý§Ú §Ù§Õ§Ö§ã§î)
#ifndef BUZZER_ON
#define BUZZER_ON GPIO_WriteBit(GPIOC, GPIO_Pin_1, Bit_SET)
#endif

#ifndef BUZZER_OFF
#define BUZZER_OFF GPIO_WriteBit(GPIOC, GPIO_Pin_1, Bit_RESET)
#endif

extern void melody_Nokia(void);
extern void melody_iPhoneMarimba(void);
extern void melody_SamsungWhistle(void);

// §ª§Ô§â§í
extern void melody_SuperMario(void);
extern void melody_Tetris(void);
extern void melody_StarWars(void);
extern void melody_ImperialMarch(void);
extern void melody_ZeldaSecret(void);
extern void melody_Pacman(void);

// §±§â§Ñ§Ù§Õ§ß§Ú§é§ß§í§Ö
extern void melody_HappyBirthday(void);
extern void melody_JingleBells(void);
extern void melody_MerryChristmas(void);

// §±§à§á§å§Ý§ñ§â§ß§í§Ö
extern void melody_PinkPanther(void);
extern void melody_MissionImpossible(void);
extern void melody_TakeOnMe(void);
extern void melody_FurElise(void);

// §¬§Ý§Ñ§ã§ã§Ú§Ü§Ñ
extern void melody_CanonInD(void);
extern void melody_OdeToJoy(void);




// --- §³§­§µ§¨§¦§¢§¯§½§¦ §©§£§µ§¬§ª ---
extern void buzzer_ok(void);
extern void buzzer_ok2(void);
extern void buzzer_error(void);
extern void buzzer_error_strong(void);
extern void buzzer_warning(void);
extern void buzzer_warning_double(void);
extern void buzzer_click(void);
extern void buzzer_success_long(void);
extern void buzzer_critical(void);
extern void buzzer_beepboop(void);
extern void buzzer_access_denied(void);
extern void buzzer_notify(void);

// §¥§à§á§à§Ý§ß§Ú§ä§Ö§Ý§î§ß§í§Ö §ã§ä§Ú§Ý§Ú§Ù§à§Ó§Ñ§ß§ß§í§Ö:
extern void buzzer_ios_click(void);
extern void buzzer_android_notify(void);
extern void buzzer_robot(void);
extern void buzzer_microwave_done(void);
extern void buzzer_winxp_msg(void);


// --- §¥§°§¢§¡§£§­§¦§¯§¯§½§¦ ---
extern void buzzer_startup(void);
extern void buzzer_shutdown(void);
extern void buzzer_charging(void);
extern void buzzer_button_hold(void);






#ifdef __cplusplus
}
#endif

#endif // BUZZER_TUNES_H