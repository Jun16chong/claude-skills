/**
 * audio_input.h — 3路音频ADC采样 + AM调制模块
 *
 * 功能:
 *   - 3路模拟音频输入 (3.5mm插座)
 *   - LM358运放进行直流偏置 (1.65V = ADC中点)
 *   - ADC采样 8kHz, 12位
 *   - 将音频采样值传递给led_modulation模块进行AM调制
 *
 * 硬件:
 *   音频输入 → 隔直电容(10μF) → LM358同相放大(增益=2)
 *            → 电阻分压偏置(Vcc/2=1.65V) → ADC输入
 *   ADC1_IN1: PA1 (音频1)
 *   ADC1_IN2: PA2 (音频2)
 *   ADC1_IN3: PA3 (音频3)
 *
 * 音频AM调制原理:
 *   载波: LED PWM频率 (1kHz/2kHz/3kHz)
 *   调制信号: 音频采样值 (0-4095, 2048=静音)
 *   LED占空比 = 50% + (sample - 2048) * mod_depth / 4096
 *
 *   接收端通过包络检波恢复音频信号
 */

#ifndef AUDIO_INPUT_H
#define AUDIO_INPUT_H

#include "platform.h"

/* 音频通道号 */
typedef enum {
    AUDIO_CH1 = 0,      // 对应 LED1 (1kHz)
    AUDIO_CH2 = 1,      // 对应 LED2 (2kHz)
    AUDIO_CH3 = 2,      // 对应 LED3 (3kHz)
    AUDIO_CH_COUNT
} audio_channel_t;

/* ===== API ===== */

/**
 * 初始化3路音频ADC
 * 需要在CubeMX中配置:
 *   - ADC1 时钟12MHz, 采样时间 28.5 cycles
 *   - 扫描模式, 3个规则通道 (IN1/IN2/IN3)
 *   - TIM触发 (TIM4_TRGO), 8kHz触发频率
 *   - DMA循环模式传输
 */
void audio_input_init(void);

/**
 * 启动音频采样
 */
void audio_input_start(void);

/**
 * 停止音频采样
 */
void audio_input_stop(void);

/**
 * 获取指定通道的最近音频采样值
 * @param ch 通道号
 * @return 12位ADC值 (0-4095), 2048=静音/中点
 */
uint16_t audio_get_sample(audio_channel_t ch);

/**
 * 获取所有3路音频采样值
 * @param samples 输出数组 [3]
 */
void audio_get_all_samples(uint16_t samples[3]);

/**
 * 音频处理服务函数 (应在主循环中调用)
 * 读取DMA缓冲中的ADC数据并更新LED调制
 */
void audio_input_service(void);

/**
 * DMA传输完成回调 (由HAL调用)
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);

#endif /* AUDIO_INPUT_H */
