
#pragma once

#ifdef __cplusplus

/* PWM Output Mode Definition */
#define PWM_MODE1 0
#define PWM_MODE2 1

/* PWM Output Mode Selection */
// #define PWM_MODE PWM_MODE1
#define PWM_MODE PWM_MODE2

class Pwm {
  public:
    Pwm() {
    }

        uint16_t arr = 100;
    uint16_t ccp = 50;

    /*********************************************************************
     * @fn      TIM1_PWMOut_Init
     *
     * @brief   Initializes TIM1 PWM Output.
     *
     * @param   arr - the period value.    100
     *          psc - the prescaler value. 9
     *          ccp - the pulse value.     50
     *
     * @return  none
     */
    void init (uint16_t _arr, uint16_t psc, uint16_t _ccp) {


        if (_arr == 0)
            _arr = 1;
        arr = _arr;


        if (_ccp > arr)
            ccp = arr - 1;
        else
            ccp = _ccp;


        GPIO_InitTypeDef GPIO_InitStructure = {0};
        TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
        TIM_OCInitTypeDef TIM_OCInitStructure = {0};

        /* 1) Тактирование: GPIOA, AFIO и TIM1 (TIM1 на APB2 у CH32V003) */
        RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO | RCC_APB2Periph_TIM1, ENABLE);

        /* 2) Настройка пина PA2 как AF Push-Pull */
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;        // PA2 -> TIM1_CH2N
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  // Alternate Function Push-Pull
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
        GPIO_Init (GPIOA, &GPIO_InitStructure);

        /* 3) Базовая настройка таймера */
        TIM_TimeBaseInitStructure.TIM_Period = arr;
        TIM_TimeBaseInitStructure.TIM_Prescaler = psc;
        TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
        TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
        TIM_TimeBaseInit (TIM1, &TIM_TimeBaseInitStructure);

        /* 4) Канал 2 (OC2), PWM режим */
        TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
        TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;    // основной выход (OC2)
        TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable;  // N-выход (OC2N) — важно
        TIM_OCInitStructure.TIM_Pulse = ccp;
        TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
        TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
        TIM_OC2Init (TIM1, &TIM_OCInitStructure);

        /* 5) Включаем предзагрузку для CCR и ARR */
        TIM_OC2PreloadConfig (TIM1, TIM_OCPreload_Enable);
        TIM_ARRPreloadConfig (TIM1, ENABLE);

        /* 6) Явно записываем CCR2 (на всякий случай) */
        setDuty (ccp);

        /* 7) Включаем главный выход (MOE) — обязательно для TIM1 complementary outputs */
        TIM_CtrlPWMOutputs (TIM1, ENABLE);

        /* 8) Включаем таймер */
        enable();
    }

    void setDuty (uint16_t duty) {
        TIM_SetCompare2 (TIM1, duty);
    }

    void enable() {
        TIM_Cmd (TIM1, ENABLE);
    }

    void disable() {
        TIM_Cmd (TIM1, DISABLE);  // таймер остановлен
    }



};

#endif  // __cplusplus