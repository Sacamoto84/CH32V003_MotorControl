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

// === §³§ª§³§´§¦§®§¯§½§¦ §©§£§µ§¬§ª ===
extern void beep_PowerOn(void);
extern void beep_PowerOff(void);
extern void beep_Click(void);
extern void beep_OK(void);
extern void beep_Error(void);
extern void beep_Warning(void);
extern void beep_Success(void);

// === §µ§£§¦§¥§°§®§­§¦§¯§ª§Á ===
extern void beep_Message(void);
extern void beep_TripleBeep(void);
extern void beep_MorseA(void);
extern void beep_SOS_Short(void);

// === §³§ª§²§¦§¯§½ §ª §´§²§¦§£§°§¤§ª ===
extern void beep_Alarm(void);
extern void beep_Pulse(void);
extern void beep_FastPulse(void);
extern void beep_Siren(void);
extern void beep_Reverse(void);

// === §ª§¤§²§°§£§½§¦ §©§£§µ§¬§ª ===
extern void beep_LevelUp(void);
extern void beep_PowerUp(void);
extern void beep_GameOver(void);
extern void beep_Coin(void);
extern void beep_Hit(void);

// === §´§¦§­§¦§¶§°§¯§¯§½§¦ §³§ª§¤§¯§¡§­§½ ===
extern void beep_IncomingCall(void);
extern void beep_Busy(void);
extern void beep_Waiting(void);

// === §³§±§¦§¸§ª§¡§­§¾§¯§½§¦ §±§¡§´§´§¦§²§¯§½ ===
extern void beep_SOS(void);
extern void beep_LowBattery(void);
extern void beep_Countdown(void);
extern void beep_Rhythm(void);
extern void beep_Trill(void);
extern void beep_Chirp(void);

#ifdef __cplusplus
}
#endif

#endif // BUZZER_TUNES_H