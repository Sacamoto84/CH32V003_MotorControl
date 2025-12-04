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
        // buzzer_warning();
        buzzer_startup();
        step = 0;
        comandMotorOn = 0;
    }

    // if (b.releaseHold()) {
    //     printf ("ReleaseHold\n");
    //     buzzer_shutdown();
    // }

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

        if (step == Screen::SET_POWER) {
            screen = Screen::SET_POWER;
            delay (1000);
            for (int i = 0; i < Screen::SET_POWER; i++) {
                LED_ON;
                buzzer_ios_click();
                LED_OFF;
                delay (200);
            }
        }

        if (step == Screen::SET_BOOST_ENABLE) {
            screen = Screen::SET_BOOST_ENABLE;
            delay (1000);
            for (int i = 0; i < Screen::SET_BOOST_ENABLE; i++) {
                LED_ON;
                buzzer_ios_click();
                LED_OFF;
                delay (200);
            }
        }

        if (step == Screen::SET_BOOST_POWER) {
            screen = Screen::SET_BOOST_POWER;
            delay (1000);
            for (int i = 0; i < Screen::SET_BOOST_POWER; i++) {
                LED_ON;
                buzzer_ios_click();
                LED_OFF;
                delay (200);
            }
        }


        if (step == Screen::SET_BOOST_TIME) {
            screen = Screen::SET_BOOST_TIME;
            delay (1000);
            for (int i = 0; i < Screen::SET_BOOST_TIME; i++) {
                LED_ON;
                buzzer_ios_click();
                LED_OFF;
                delay (200);
            }
        }
    }

    if (b.release()) {
        printf ("Release\n");
        LED_OFF;
    }

    if (b.hasClicks()) {
        printf ("Clicks: %d\n", b.getClicks());
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
