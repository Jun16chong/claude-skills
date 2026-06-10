/**
 * goertzel.c — Goertzel 算法实现
 *
 * 性能分析 (STM32F103C8T6, 72MHz):
 *   单次检测 (N=400): ~400×3浮点运算 ≈ 1200次操作
 *   3个频率并行: ~3600次操作
 *   每50ms完成一轮检测 → 约72k FLOPS → STM32F103轻松胜任
 *
 * 浮点对比:
 *   Goertzel (3 freq × 400 pts): ~3600次操作
 *   FFT (512点): ~4608次操作 + 还需要额外的频率映射
 *   结论: Goertzel在这个场景下更有优势 (精确频率点，计算量更小)
 */

#include "goertzel.h"
#include <math.h>
#include <stdlib.h>

/* 平滑系数 (0~1, 越大越平滑但响应越慢) */
#define SMOOTH_ALPHA    0.3f

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ===== 初始化 ===== */

void goertzel_init(goertzel_detector_t *det, float freq, float fs, uint16_t n)
{
    det->target_freq = freq;
    det->sample_rate = fs;
    det->block_size = n;

    /*
     * 计算Goertzel系数:
     * k = round(f·N/Fs)  — 目标频率对应的DFT bin
     * ω₀ = 2π·k/N
     * coeff = 2·cos(ω₀)
     */
    float k = roundf(freq * (float)n / fs);
    float omega = 2.0f * (float)M_PI * k / (float)n;
    det->coeff = 2.0f * cosf(omega);

    goertzel_reset(det);
}

void goertzel_reset(goertzel_detector_t *det)
{
    det->s0 = 0.0f;
    det->s1 = 0.0f;
    det->s2 = 0.0f;
    det->sample_count = 0;
    /* 不清零 magnitude — 保留上一次的结果供查询 */
}

/* ===== 样本处理 ===== */

void goertzel_process_sample(goertzel_detector_t *det, float sample)
{
    /*
     * Goertzel 递推:
     *   s[n] = x[n] + coeff·s[n-1] - s[n-2]
     */
    det->s0 = sample + det->coeff * det->s1 - det->s2;
    det->s2 = det->s1;
    det->s1 = det->s0;
    det->sample_count++;
}

void goertzel_process_block(goertzel_detector_t *det,
                            const float *samples, uint16_t count)
{
    for (uint16_t i = 0; i < count; i++) {
        goertzel_process_sample(det, samples[i]);
    }
}

void goertzel_process_block_u16(goertzel_detector_t *det,
                                const uint16_t *samples, uint16_t count,
                                uint16_t dc_bias)
{
    /*
     * 将uint16 ADC值转为float并减去直流偏置
     * 直流偏置 = 环境光导致的ADC偏移 (传感器在无调制光时的ADC值)
     */
    for (uint16_t i = 0; i < count; i++) {
        float val = (float)((int32_t)samples[i] - (int32_t)dc_bias);
        goertzel_process_sample(det, val);
    }
}

/* ===== 幅度计算 ===== */

float goertzel_finish(goertzel_detector_t *det)
{
    /*
     * 幅度² = s[N-1]² + s[N-2]² - coeff·s[N-1]·s[N-2]
     * 其中 s[N-1]=s1, s[N-2]=s2 (处理完N个样本后)
     */
    float mag_sq = det->s1 * det->s1
                 + det->s2 * det->s2
                 - det->coeff * det->s1 * det->s2;

    /* 防止负数 (数值误差) */
    if (mag_sq < 0.0f) mag_sq = 0.0f;

    float mag = sqrtf(mag_sq);

    /* 归一化: 除以 N/2 (Goertzel输出幅度 = N/2 × 输入幅度) */
    mag = mag / ((float)det->block_size / 2.0f);

    det->magnitude = mag;

    /* 指数平滑 */
    float old_smooth = det->magnitude_smooth;
    det->magnitude_smooth = SMOOTH_ALPHA * mag + (1.0f - SMOOTH_ALPHA) * old_smooth;

    return mag;
}

/* ===== 结果查询 ===== */

float goertzel_get_magnitude(goertzel_detector_t *det)
{
    return det->magnitude;
}

float goertzel_get_smooth_magnitude(goertzel_detector_t *det)
{
    return det->magnitude_smooth;
}

float goertzel_get_snr_db(goertzel_detector_t *det, float noise_floor)
{
    if (noise_floor <= 0.0f) return 99.0f;

    float ratio = det->magnitude_smooth / noise_floor;
    if (ratio <= 0.0f) return -99.0f;

    return 20.0f * log10f(ratio);
}

float goertzel_noise_floor(goertzel_detector_t *detectors[3])
{
    /*
     * 简单噪声估计: 取3个检测器中幅度最小的
     * 假设至少1个LED的信号较弱 = 接近噪声水平
     */
    float min_mag = detectors[0]->magnitude_smooth;
    for (int i = 1; i < 3; i++) {
        if (detectors[i]->magnitude_smooth < min_mag) {
            min_mag = detectors[i]->magnitude_smooth;
        }
    }
    return min_mag * 0.5f;  /* 保守估计: 噪声 ≈ 最小信号的50% */
}

/* ===== 辅助函数 ===== */

/*
 * 快速整数取整 (不用math.h的roundf以节省flash)
 */
static float fast_roundf(float x)
{
    return (float)(int)(x + 0.5f);
}

/*
 * 用标准库的roundf替代
 */
#define roundf(x)  ((float)(int)((x) + 0.5f))
