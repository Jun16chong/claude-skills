/**
 * platform.h — 发送端（LED控制电路）硬件平台定义
 *
 * MCU: STM32F103C8T6, 72MHz
 * 功能: 3路LED PWM调制 + 4x4键盘扫描 + 3路音频ADC采样
 *
 * 引脚分配:
 *   LED1 PWM: PA8  (TIM1_CH1)  1kHz
 *   LED2 PWM: PA0  (TIM2_CH1)  2kHz
 *   LED3 PWM: PA6  (TIM3_CH1)  3kHz
 *   键盘行:   PB0~PB3  (GPIO输入,上拉)
 *   键盘列:   PB4~PB7  (GPIO输出)
 *   音频ADC1: PA1      (ADC1_IN1)
 *   音频ADC2: PA2      (ADC1_IN2)
 *   音频ADC3: PA3      (ADC1_IN3)
 *   调试UART: PA9=TX, PA10=RX
 *   SWD:      PA13=SWDIO, PA14=SWCLK
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ===== 系统时钟 ===== */
#define SYSTEM_CLOCK_HZ     72000000U

/* ===== LED PWM 参数 ===== */
/*
 * 载波频率选择: f > 100Hz (人眼不可感知)
 * f1=1000Hz, f2=2000Hz, f3=3000Hz
 * ARR = 72MHz / freq - 1 (定时器时钟=72MHz, 不分频)
 */
#define LED1_PWM_FREQ       1000U       // 1kHz 载波
#define LED2_PWM_FREQ       2000U       // 2kHz 载波
#define LED3_PWM_FREQ       3000U       // 3kHz 载波

#define LED1_TIM            htim1
#define LED2_TIM            htim2
#define LED3_TIM            htim3
#define LED1_CHANNEL        TIM_CHANNEL_1
#define LED2_CHANNEL        TIM_CHANNEL_1
#define LED3_CHANNEL        TIM_CHANNEL_1

/* PWM 占空比范围 (0 ~ ARR) */
#define PWM_DUTY_MAX(pwm_freq)   ((SYSTEM_CLOCK_HZ) / (pwm_freq) - 1)

/* 默认50%占空比 */
#define LED_DEFAULT_DUTY(pwm_freq)  (PWM_DUTY_MAX(pwm_freq) / 2)

/*
 * 音频AM调制参数
 * 调制深度: 40% (防止过调制导致LED截止或饱和)
 * duty = base_duty + (audio_sample - 2048) * mod_depth
 */
#define AUDIO_MOD_DEPTH     0.4f
#define AUDIO_ADC_MID       2048U       // 12位ADC中值

/* ===== 音频ADC参数 ===== */
#define AUDIO_SAMPLE_RATE   8000U       // 8kHz采样率
#define AUDIO_ADC_BUF_SIZE  64          // 每通道缓冲大小

/* ===== 数字通信参数 ===== */
/*
 * 在定位载波上叠加数字调制
 * bit 1 → 占空比 75%
 * bit 0 → 占空比 25%
 * 波特率: 300bps (保守, LED响应慢)
 */
#define DIGITAL_BAUD_RATE   300U
#define DIGITAL_BIT_1_DUTY  0.75f
#define DIGITAL_BIT_0_DUTY  0.25f

/* ===== 键盘参数 ===== */
#define KEYPAD_ROWS         4
#define KEYPAD_COLS         4
#define KEYPAD_DEBOUNCE_MS  30U

/* 键盘映射表 (4x4矩阵): 0-9, A-D, *, # */
extern const char KEYPAD_MAP[KEYPAD_ROWS][KEYPAD_COLS];

/* ===== 调试 ===== */
#define DEBUG_UART          huart1
#define DEBUG_BAUDRATE      115200U

#ifdef DEBUG_ENABLE
  #define DBG_PRINT(fmt, ...)  printf("[SENDER] " fmt "\r\n", ##__VA_ARGS__)
#else
  #define DBG_PRINT(fmt, ...)  ((void)0)
#endif

/* ===== 外部定时器句柄 (CubeMX生成) ===== */
extern TIM_HandleTypeDef LED1_TIM;
extern TIM_HandleTypeDef LED2_TIM;
extern TIM_HandleTypeDef LED3_TIM;
extern ADC_HandleTypeDef hadc1;
extern UART_HandleTypeDef DEBUG_UART;
extern TIM_HandleTypeDef htim_audio;   /* 音频ADC触发定时器 */

#endif /* PLATFORM_H */
