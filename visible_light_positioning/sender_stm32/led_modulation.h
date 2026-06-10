/**
 * led_modulation.h — 3路LED PWM调制模块
 *
 * 功能:
 *   - 分别控制3个白光LED以不同频率(1kHz/2kHz/3kHz)闪烁
 *   - 人眼无感知（>100Hz载波），但光电传感器可检测
 *   - 支持AM音频调制叠加（用于发挥部分3）
 *   - 支持OOK数字调制叠加（用于发挥部分2）
 *
 * 原理:
 *   - 频分复用(FDM): 3个LED各占一个独立载波频率
 *   - 接收端通过Goertzel算法分离3个频率成分
 *   - 根据各频率的信号强度(RSSI)进行三边定位
 */

#ifndef LED_MODULATION_H
#define LED_MODULATION_H

#include "platform.h"

/* LED编号 */
typedef enum {
    LED1 = 0,   // 1kHz 载波
    LED2 = 1,   // 2kHz 载波
    LED3 = 2,   // 3kHz 载波
    LED_COUNT
} led_id_t;

/* LED调制模式 */
typedef enum {
    LED_MODE_IDLE = 0,      // 空闲（占空比0=灭）
    LED_MODE_CARRIER,       // 仅定位载波（50%占空比）
    LED_MODE_DIGITAL,       // 定位载波 + 数字调制
    LED_MODE_AUDIO,         // 定位载波 + 音频调制
    LED_MODE_DIGITAL_AUDIO  // 定位载波 + 数字 + 音频（同时）
} led_mode_t;

/* LED状态结构体 */
typedef struct {
    led_id_t id;
    led_mode_t mode;
    uint32_t base_freq;         // 载波频率 (Hz)
    uint32_t pwm_arr;           // PWM自动重载值 (period)
    uint32_t base_duty;         // 基础占空比 (50%)
    uint32_t current_duty;      // 当前实际占空比
    TIM_HandleTypeDef *htim;    // 对应定时器
    uint32_t channel;           // 对应通道
    /* 数字调制状态 */
    uint8_t digital_bit;        // 当前发送的bit
    uint32_t bit_tick;          // 当前bit的tick计数
    /* 音频调制状态 */
    int16_t audio_sample;       // 当前音频采样值
} led_state_t;

/* ===== API ===== */

/**
 * 初始化所有3路LED PWM
 * 调用前需确保CubeMX已配置对应TIM为PWM模式
 */
void led_modulation_init(void);

/**
 * 设置LED工作模式
 * @param led  LED编号
 * @param mode 工作模式
 */
void led_set_mode(led_id_t led, led_mode_t mode);

/**
 * 设置LED占空比 (直接控制)
 * @param led   LED编号
 * @param duty  占空比值 (0 ~ pwm_arr)
 *              0=常低(灭), pwm_arr=常高, pwm_arr/2=50%
 */
void led_set_duty(led_id_t led, uint32_t duty);

/**
 * 设置LED PWM占空比百分比 (0.0 ~ 1.0)
 */
void led_set_duty_percent(led_id_t led, float percent);

/**
 * 更新数字调制bit
 * @param led LED编号
 * @param bit 要发送的bit (0或1)
 */
void led_set_digital_bit(led_id_t led, uint8_t bit);

/**
 * 更新音频采样值（用于AM调制）
 * @param led    LED编号
 * @param sample 12位ADC采样值 (0~4095)
 */
void led_set_audio_sample(led_id_t led, int16_t sample);

/**
 * 主循环中调用（如需要定时更新数字/音频调制）
 * 应在1ms周期或更快频率调用
 */
void led_modulation_service(void);

/**
 * 获取LED当前状态
 */
const led_state_t* led_get_state(led_id_t led);

/**
 * 全部LED设为载波模式（定位状态）
 */
void led_all_carrier_mode(void);

/**
 * 全部LED关闭
 */
void led_all_off(void);

/**
 * 单个LED开关（调试用）
 */
void led_enable(led_id_t led, uint8_t on);

#endif /* LED_MODULATION_H */
