#include "debug.h"
#include "uButton.h"

extern uButton b;
extern Screen screen;

void ScreenBoostEnable (void) {

    if (configBoostEnable)
        LED_ON;

    else
        LED_OFF;

    if (b.hasClicks()) {
        printf ("Clicks: %d\n", b.getClicks());

        if (b.getClicks() == 1) {
            if (configBoostEnable)
                configBoostEnable = 0;
            else
                configBoostEnable = 1;
            buzzer_ok();
        }
        if (b.getClicks() == 2) {
            screen = Screen::NORMAL;
            buzzer_shutdown();
            b.reset();
            LED_OFF;
        }
    }
}

void ScreenNormal (void) {

    static int step = 0;
    // if (b.press()) {
    //     printf ("Press\n");
    //     LED_ON;
    // }

    if (b.click()) {
        printf ("Click\n");
        buzzer_ios_click();

        if (comandMotorOn)
            comandMotorOn = 0;
        else
            comandMotorOn = 1;
    }

    if (b.hold()) {
        printf ("Hold\n");
        buzzer_warning();
        step = -1;
        comandMotorOn = 0;
    }

    if (b.releaseHold()) {
        printf ("ReleaseHold\n");
    }

    if (b.step()) {
        step++;
        printf ("Step %d\n", step);
        buzzer_ios_click();
        LED_ON;
        delay (5);
        LED_OFF;
    }


    if (b.releaseStep()) {
        printf ("releaseStep\n");

        if (step == 3) {
            screen = Screen::SET_BOOST_ENABLE;
            delay (1000);
            LED_ON;
            buzzer_ios_click();
            LED_OFF;
            delay (200);
            LED_ON;
            buzzer_ios_click();
            LED_OFF;
            delay (200);
            LED_ON;
            buzzer_ios_click();
            LED_OFF;
            delay (200);
            LED_ON;
            buzzer_ios_click();
            LED_OFF;
        }
    }

    if (b.release()) {
        printf ("Release\n");
        LED_OFF;
    }

    if (b.hasClicks()) {
        printf ("Clicks: %d\n", b.getClicks());

        // if (b.getClicks() == 2) {
        //     // SET_POWER
        //     // screen = Screen::SET_POWER;

        //  buzzer_ios_click();
        //  delay (200);
        //  buzzer_ios_click();
        // }

        // if (b.getClicks() == 3) {
        //     // SET_POWER
        //     // screen = Screen::SET_POWER;

        //  buzzer_ios_click();
        //  delay (200);
        //  buzzer_ios_click();
        //  delay (200);
        //  buzzer_ios_click();
        // }

        // if (b.getClicks() == 4) {
        //     // SET_POWER
        //     screen = Screen::SET_BOOST_ENABLE;
        //     buzzer_ios_click();
        //     delay (200);
        //     buzzer_ios_click();
        //     delay (200);
        //     buzzer_ios_click();
        //     delay (200);
        //     buzzer_ios_click();
        // }


        if (b.getClicks() == 5) {
            buzzer_shutdown();
            buzzer_shutdown();
            buzzer_shutdown();
            buzzer_shutdown();

            __disable_irq();  // отключаем все прерывания
            NVIC_SystemReset();
            while (1);
        }
    }

    if (b.timeout()) {
        printf ("Timeout\n");
        buzzer_robot();
        if (!comandMotorOn) {
            gotoDeepSleep();
        }
    }
}
