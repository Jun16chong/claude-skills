/**
 * goertzel.h — Goertzel 特定频率幅度检测算法
 *
 * 算法原理:
 *   Goertzel算法是一种高效的DFT单频点计算方法，比完整FFT更快
 *   当只需检测少数已知频率时 (本系统仅3个: 1kHz/2kHz/3kHz)
 *   Goertzel的运算量远小于FFT
 *
 *   Goertzel二阶IIR递推公式:
 *     s[n] = x[n] + 2·cos(ω₀)·s[n-1] - s[n-2]
 *     幅度² = s[N-1]² + s[N-2]² - 2·cos(ω₀)·s[N-1]·s[N-2]
 *
 *     其中 ω₀ = 2π·k/N, k = round(f·N/Fs)
 *
 * 参数选择:
 *   Fs = 8000Hz, N = 400点
 *   f1=1000Hz → k1=50  (精确对齐: 50×8000/400=1000)
 *   f2=2000Hz → k2=100 (精确对齐: 100×8000/400=2000)
 *   f3=3000Hz → k3=150 (精确对齐: 150×8000/400=3000)
 *
 * 窗口时长: 400/8000 = 50ms
 *
 * 参考: "DTMF Detection Using Goertzel Algorithm" (Texas Instruments)
 */

#ifndef GOERTZEL_H
#define GOERTZEL_H

#include "platform.h"

/* Goertzel 检测器实例 */
typedef struct {
    float target_freq;      /* 目标频率 (Hz) */
    float coeff;            /* 预计算系数: 2·cos(2π·k/N) */
    uint16_t block_size;    /* 采样点数 N */
    float sample_rate;      /* 采样率 Fs */
    /* 内部状态 */
    float s0, s1, s2;       /* 递推状态变量 */
    uint16_t sample_count;  /* 当前已处理采样数 */
    float magnitude;        /* 最近一次计算结果 (幅度) */
    float magnitude_smooth; /* 平滑后的幅度 (用于减少噪声) */
} goertzel_detector_t;

/* ===== API ===== */

/**
 * 初始化Goertzel检测器
 * @param det   检测器实例指针
 * @param freq  目标检测频率 (Hz)
 * @param fs    采样率 (Hz)
 * @param n     采样点数 (block size)
 */
void goertzel_init(goertzel_detector_t *det, float freq, float fs, uint16_t n);

/**
 * 重置检测器内部状态 (用于新一轮检测)
 */
void goertzel_reset(goertzel_detector_t *det);

/**
 * 处理单个采样值 (逐个样本送入)
 * @param det    检测器实例
 * @param sample 采样值 (ADC原始值或归一化值)
 */
void goertzel_process_sample(goertzel_detector_t *det, float sample);

/**
 * 批量处理采样数组 (一次处理全部N个样本)
 * @param det     检测器实例
 * @param samples 采样值数组
 * @param count   采样值数量
 */
void goertzel_process_block(goertzel_detector_t *det,
                            const float *samples, uint16_t count);

/**
 * 批量处理uint16采样数组 (ADC原始数据, 自动减去直流偏置)
 * @param det     检测器实例
 * @param samples ADC原始采样值数组
 * @param count   采样值数量
 * @param dc_bias 直流偏置值 (环境光导致的ADC偏置)
 */
void goertzel_process_block_u16(goertzel_detector_t *det,
                                const uint16_t *samples, uint16_t count,
                                uint16_t dc_bias);

/**
 * 完成检测并计算幅度
 * 在送入全部N个样本后调用
 * @return 频率分量的幅度值
 */
float goertzel_finish(goertzel_detector_t *det);

/**
 * 获取最近一次检测的幅度值
 */
float goertzel_get_magnitude(goertzel_detector_t *det);

/**
 * 获取平滑后的幅度值 (推荐用于RSSI)
 */
float goertzel_get_smooth_magnitude(goertzel_detector_t *det);

/**
 * 获取信号的信噪比 (dB)
 * 需要已知噪声基底幅度
 * @param noise_floor 噪声基底幅度
 */
float goertzel_get_snr_db(goertzel_detector_t *det, float noise_floor);

/**
 * 计算噪声基底 (取3个检测器幅度中的最小值作为噪声估计)
 */
float goertzel_noise_floor(goertzel_detector_t *detectors[3]);

#endif /* GOERTZEL_H */
