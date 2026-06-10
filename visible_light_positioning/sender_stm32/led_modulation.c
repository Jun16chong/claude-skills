/**
 * led_modulation.c — 3路LED PWM调制模块实现
 *
 * 硬件连接:
 *   LED1: PA8 → AO3400 MOSFET Gate → LED阴极 → 12V
 *         LED阳极 → 12V (共阳接法, MOSFET控制阴极通断)
 *   LED2: PA0 → AO3400 MOSFET Gate
 *   LED3: PA6 → AO3400 MOSFET Gate
 *
 * 注意:
 *   - 3W LED需配铝基板+散热片
 *   - MOSFET选型 AO3400 (Vgs(th)<1V, 便宜, N-ch, 30V/5.8A)
 *   - 限流电阻计算: R = (12V - Vf_LED - Vds_on) / I_LED
 *     示例: 3W白光LED Vf≈3.3V, I=350mA
 *     R = (12-3.3-0.1) / 0.35 ≈ 24.5Ω → 选27Ω/2W
 *   - 需在12V与LED阳极间串联限流电阻
 */

#include "led_modulation.h"

/* 全局LED状态数组 */
static led_state_t g_leds[LED_COUNT];

/* ===== 初始化 ===== */

void led_modulation_init(void)
{
    /* LED1: 1kHz, TIM1_CH1, PA8 */
    g_leds[LED1].id = LED1;
    g_leds[LED1].base_freq = LED1_PWM_FREQ;
    g_leds[LED1].pwm_arr = PWM_DUTY_MAX(LED1_PWM_FREQ);  /* 72000-1=71999 */
    g_leds[LED1].base_duty = g_leds[LED1].pwm_arr / 2;    /* 50% */
    g_leds[LED1].current_duty = 0;
    g_leds[LED1].htim = &LED1_TIM;
    g_leds[LED1].channel = LED1_CHANNEL;
    g_leds[LED1].mode = LED_MODE_IDLE;

    /* LED2: 2kHz, TIM2_CH1, PA0 */
    g_leds[LED2].id = LED2;
    g_leds[LED2].base_freq = LED2_PWM_FREQ;
    g_leds[LED2].pwm_arr = PWM_DUTY_MAX(LED2_PWM_FREQ);  /* 36000-1=35999 */
    g_leds[LED2].base_duty = g_leds[LED2].pwm_arr / 2;
    g_leds[LED2].current_duty = 0;
    g_leds[LED2].htim = &LED2_TIM;
    g_leds[LED2].channel = LED2_CHANNEL;
    g_leds[LED2].mode = LED_MODE_IDLE;

    /* LED3: 3kHz, TIM3_CH1, PA6 */
    g_leds[LED3].id = LED3;
    g_leds[LED3].base_freq = LED3_PWM_FREQ;
    g_leds[LED3].pwm_arr = PWM_DUTY_MAX(LED3_PWM_FREQ);  /* 24000-1=23999 */
    g_leds[LED3].base_duty = g_leds[LED3].pwm_arr / 2;
    g_leds[LED3].current_duty = 0;
    g_leds[LED3].htim = &LED3_TIM;
    g_leds[LED3].channel = LED3_CHANNEL;
    g_leds[LED3].mode = LED_MODE_IDLE;

    /* 启动所有PWM通道，初始占空比=0（LED灭） */
    HAL_TIM_PWM_Start(g_leds[LED1].htim, g_leds[LED1].channel);
    HAL_TIM_PWM_Start(g_leds[LED2].htim, g_leds[LED2].channel);
    HAL_TIM_PWM_Start(g_leds[LED3].htim, g_leds[LED3].channel);

    /* 设置初始占空比为0 */
    __HAL_TIM_SET_COMPARE(g_leds[LED1].htim, g_leds[LED1].channel, 0);
    __HAL_TIM_SET_COMPARE(g_leds[LED2].htim, g_leds[LED2].channel, 0);
    __HAL_TIM_SET_COMPARE(g_leds[LED3].htim, g_leds[LED3].channel, 0);
}

/* ===== 占空比控制 ===== */

void led_set_duty(led_id_t led, uint32_t duty)
{
    if (led >= LED_COUNT) return;

    /* 限幅 */
    if (duty > g_leds[led].pwm_arr) {
        duty = g_leds[led].pwm_arr;
    }

    g_leds[led].current_duty = duty;
    __HAL_TIM_SET_COMPARE(g_leds[led].htim, g_leds[led].channel, duty);
}

void led_set_duty_percent(led_id_t led, float percent)
{
    if (led >= LED_COUNT) return;

    if (percent < 0.0f) percent = 0.0f;
    if (percent > 1.0f) percent = 1.0f;

    uint32_t duty = (uint32_t)(g_leds[led].pwm_arr * percent);
    led_set_duty(led, duty);
}

/* ===== 模式控制 ===== */

void led_set_mode(led_id_t led, led_mode_t mode)
{
    if (led >= LED_COUNT) return;

    g_leds[led].mode = mode;

    switch (mode) {
    case LED_MODE_IDLE:
        led_set_duty(led, 0);
        break;
    case LED_MODE_CARRIER:
        /* 仅定位载波: 50%占空比 */
        led_set_duty(led, g_leds[led].base_duty);
        break;
    default:
        /* 其他模式由 service() 动态更新 */
        led_set_duty(led, g_leds[led].base_duty);
        break;
    }
}

void led_all_carrier_mode(void)
{
    for (int i = 0; i < LED_COUNT; i++) {
        led_set_mode((led_id_t)i, LED_MODE_CARRIER);
    }
}

void led_all_off(void)
{
    for (int i = 0; i < LED_COUNT; i++) {
        led_set_mode((led_id_t)i, LED_MODE_IDLE);
    }
}

void led_enable(led_id_t led, uint8_t on)
{
    if (led >= LED_COUNT) return;

    if (on) {
        led_set_mode(led, LED_MODE_CARRIER);
    } else {
        led_set_mode(led, LED_MODE_IDLE);
    }
}

/* ===== 数字调制 ===== */

void led_set_digital_bit(led_id_t led, uint8_t bit)
{
    if (led >= LED_COUNT) return;

    g_leds[led].digital_bit = bit;
    g_leds[led].bit_tick = 0;

    /* 立即更新占空比 */
    float percent = bit ? DIGITAL_BIT_1_DUTY : DIGITAL_BIT_0_DUTY;
    uint32_t duty = (uint32_t)(g_leds[led].pwm_arr * percent);
    led_set_duty(led, duty);
}

/* ===== 音频调制 ===== */

void led_set_audio_sample(led_id_t led, int16_t sample)
{
    if (led >= LED_COUNT) return;

    g_leds[led].audio_sample = sample;
}

/* ===== 服务函数 ===== */

void led_modulation_service(void)
{
    /*
     * 根据各LED当前模式更新PWM占空比
     * 调用频率: 建议每1ms (或与音频采样率同步)
     */

    for (int i = 0; i < LED_COUNT; i++) {
        led_state_t *ls = &g_leds[i];
        uint32_t new_duty = ls->base_duty;

        switch (ls->mode) {
        case LED_MODE_IDLE:
            new_duty = 0;
            break;

        case LED_MODE_CARRIER:
            new_duty = ls->base_duty;
            break;

        case LED_MODE_DIGITAL: {
            /*
             * 数字调制: 改变占空比
             * bit=1 → 75%, bit=0 → 25%
             */
            float pct = ls->digital_bit ? DIGITAL_BIT_1_DUTY : DIGITAL_BIT_0_DUTY;
            new_duty = (uint32_t)(ls->pwm_arr * pct);
            break;
        }

        case LED_MODE_AUDIO: {
            /*
             * AM音频调制:
             * duty = base_duty + (sample - 2048) * mod_depth * (arr/4096)
             * 其中4096是12位ADC满量程, mod_depth=0.4防止过调制
             */
            float offset = (float)(ls->audio_sample - (int32_t)AUDIO_ADC_MID)
                         * AUDIO_MOD_DEPTH
                         * (float)ls->pwm_arr / 4096.0f;
            int32_t duty_val = (int32_t)ls->base_duty + (int32_t)offset;
            if (duty_val < 0) duty_val = 0;
            if (duty_val > (int32_t)ls->pwm_arr) duty_val = (int32_t)ls->pwm_arr;
            new_duty = (uint32_t)duty_val;
            break;
        }

        case LED_MODE_DIGITAL_AUDIO: {
            /*
             * 数字+音频同时调制:
             * 在数字bit的占空比基值上叠加音频
             */
            float base_pct = ls->digital_bit ? DIGITAL_BIT_1_DUTY : DIGITAL_BIT_0_DUTY;
            uint32_t bit_duty = (uint32_t)(ls->pwm_arr * base_pct);

            float offset = (float)(ls->audio_sample - (int32_t)AUDIO_ADC_MID)
                         * AUDIO_MOD_DEPTH * 0.5f  /* 额外降低调制深度 */
                         * (float)ls->pwm_arr / 4096.0f;
            int32_t duty_val = (int32_t)bit_duty + (int32_t)offset;
            if (duty_val < 0) duty_val = 0;
            if (duty_val > (int32_t)ls->pwm_arr) duty_val = (int32_t)ls->pwm_arr;
            new_duty = (uint32_t)duty_val;
            break;
        }
        } /* switch */

        /* 更新PWM（如占空比变化） */
        if (new_duty != ls->current_duty) {
            led_set_duty(ls->id, new_duty);
        }
    }
}

/* ===== 状态查询 ===== */

const led_state_t* led_get_state(led_id_t led)
{
    if (led >= LED_COUNT) return NULL;
    return &g_leds[led];
}
