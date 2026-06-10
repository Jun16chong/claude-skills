/**
 * audio_input.c — 3路音频ADC采样 + AM调制实现
 */

#include "audio_input.h"
#include "led_modulation.h"

/* DMA双缓冲: 每缓冲存3通道×64采样=192个半字 */
#define ADC_BUF_SAMPLES     64U     /* 每通道每缓冲区采样数 */
#define ADC_BUF_SIZE        (ADC_BUF_SAMPLES * AUDIO_CH_COUNT)  /* 192 */

/* 双缓冲 */
static uint16_t g_adc_buf_a[ADC_BUF_SIZE];
static uint16_t g_adc_buf_b[ADC_BUF_SIZE];
static volatile uint8_t g_active_buf = 0;  /* 0=A, 1=B */
static volatile uint8_t g_buf_ready = 0;   /* 新数据就绪标志 */

/* 最近采样值 (供快速读取) */
static volatile uint16_t g_latest_samples[AUDIO_CH_COUNT] = {2048, 2048, 2048};

/* ===== 初始化 ===== */

void audio_input_init(void)
{
    /*
     * CubeMX 预设配置:
     *   TIM4 配置为 8kHz TRGO 触发:
     *     Prescaler = 0 (72MHz)
     *     ARR = 8999 → 72MHz/9000 = 8kHz
     *     TRGO = Update event
     *
     *   ADC1 配置:
     *     Clock prescaler = PCLK2/6 (12MHz)
     *     Scan mode = Enabled
     *     Number of conversions = 3
     *     External trigger = TIM4_TRGO, Rising edge
     *     Conversion 1: Channel 1 (PA1), 28.5 cycles
     *     Conversion 2: Channel 2 (PA2), 28.5 cycles
     *     Conversion 3: Channel 3 (PA3), 28.5 cycles
     *     DMA = Circular mode, Half-word
     *
     *   DMA配置:
     *     Circular mode
     *     Memory size = Half Word (16-bit)
     *     Peripheral size = Half Word
     */

    /* 清空缓冲 */
    memset(g_adc_buf_a, 0, sizeof(g_adc_buf_a));
    memset(g_adc_buf_b, 0, sizeof(g_adc_buf_b));

    /* 校准ADC */
    HAL_ADCEx_Calibration_Start(&hadc1);

    /*
     * 启动DMA传输 (双缓冲模式)
     * 注意: STM32F1 HAL 不直接支持双缓冲DMA,
     * 可在 HAL_ADC_ConvHalfCpltCallback 和 HAL_ADC_ConvCpltCallback
     * 中分别处理半传输完成和全传输完成来模拟双缓冲
     */
}

void audio_input_start(void)
{
    /* 启动ADC DMA连续采样 */
    HAL_ADC_Start_DMA(&hadc1,
                      (uint32_t*)g_adc_buf_a,
                      ADC_BUF_SIZE);
}

void audio_input_stop(void)
{
    HAL_ADC_Stop_DMA(&hadc1);
}

/* ===== 数据访问 ===== */

uint16_t audio_get_sample(audio_channel_t ch)
{
    if (ch >= AUDIO_CH_COUNT) return 2048;
    return g_latest_samples[ch];
}

void audio_get_all_samples(uint16_t samples[3])
{
    samples[0] = g_latest_samples[AUDIO_CH1];
    samples[1] = g_latest_samples[AUDIO_CH2];
    samples[2] = g_latest_samples[AUDIO_CH3];
}

/* ===== 服务函数 ===== */

void audio_input_service(void)
{
    if (!g_buf_ready) return;

    /*
     * 处理刚完成的半缓冲数据
     * 从DMA缓冲区读取最新3通道采样值
     * 并更新对应LED的音频调制
     */

    uint16_t *buf = g_active_buf ? g_adc_buf_b : g_adc_buf_a;

    /*
     * 取缓冲区中间的采样值（避开DMA边界不稳定的数据）
     * ADC采样顺序: CH1, CH2, CH3, CH1, CH2, CH3, ...
     * 取缓冲中间3个采样 = 索引30-32 (大约位置)
     */
    int mid = (ADC_BUF_SIZE / 2) - (ADC_BUF_SIZE / 2) % AUDIO_CH_COUNT;
    uint16_t s1 = buf[mid + 0];  /* CH1 */
    uint16_t s2 = buf[mid + 1];  /* CH2 */
    uint16_t s3 = buf[mid + 2];  /* CH3 */

    /* 简单移动平均 (与前值平均) */
    g_latest_samples[AUDIO_CH1] =
        (g_latest_samples[AUDIO_CH1] + s1) / 2;
    g_latest_samples[AUDIO_CH2] =
        (g_latest_samples[AUDIO_CH2] + s2) / 2;
    g_latest_samples[AUDIO_CH3] =
        (g_latest_samples[AUDIO_CH3] + s3) / 2;

    /* 更新LED音频调制 */
    led_set_audio_sample(LED1, (int16_t)g_latest_samples[AUDIO_CH1]);
    led_set_audio_sample(LED2, (int16_t)g_latest_samples[AUDIO_CH2]);
    led_set_audio_sample(LED3, (int16_t)g_latest_samples[AUDIO_CH3]);

    g_buf_ready = 0;
}

/* ===== DMA回调 ===== */

/*
 * 半传输完成中断 → 处理缓冲区前半部分
 * （使用双缓冲技巧: 前半=A, 后半=B）
 */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) {
        g_active_buf = 1;   /* 前半=A刚完成，切换到B */
        g_buf_ready = 1;
    }
}

/*
 * 全传输完成中断 → 处理缓冲区后半部分
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) {
        g_active_buf = 0;   /* 后半=B刚完成，切换回A */
        g_buf_ready = 1;
    }
}
