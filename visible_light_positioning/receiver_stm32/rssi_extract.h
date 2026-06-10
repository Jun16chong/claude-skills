/**
 * rssi_extract.h — RSSI提取与归一化模块
 *
 * 功能:
 *   - 从Goertzel检测结果中提取3路RSSI值
 *   - 环境光直流偏置估计与补偿
 *   - RSSI归一化 (消除LED发光强度差异和距离衰减的系统偏差)
 *   - 动态范围自适应
 *
 * RSSI归一化策略:
 *   原始问题: 不同LED发光强度可能不同, 距离不同导致信号强度变化巨大
 *   归一化: RSSI_norm[i] = RSSI[i] / sum(RSSI)
 *      → 消除绝对强度影响, 保留3个LED的相对强度关系
 *      → 相对强度关系取决于传感器到各LED的距离比
 *      → 可用于三边定位
 */

#ifndef RSSI_EXTRACT_H
#define RSSI_EXTRACT_H

#include "platform.h"
#include "goertzel.h"

/* RSSI数据结构 */
typedef struct {
    float raw_mag[3];       /* 原始Goertzel幅度 (LED1/LED2/LED3) */
    float norm_rssi[3];     /* 归一化RSSI值 (和=1.0) */
    float ratio_12;         /* RSSI1/RSSI2 比值 */
    float ratio_13;         /* RSSI1/RSSI3 比值 */
    float ratio_23;         /* RSSI2/RSSI3 比值 */
    float confidence;       /* 置信度 (0~1, 信噪比越高置信度越高) */
    uint8_t valid;          /* 数据有效标志 */
    uint16_t dc_bias;       /* 当前直流偏置估计值 */
} rssi_data_t;

/* ===== API ===== */

/**
 * 初始化RSSI提取模块
 */
void rssi_extract_init(void);

/**
 * 从3个Goertzel检测器提取RSSI
 * @param dets   [goertzel_detector_t*; 3] 3个检测器 (f1/f2/f3)
 * @param output RSSI数据输出
 */
void rssi_extract(goertzel_detector_t *dets[3], rssi_data_t *output);

/**
 * 估计环境光直流偏置
 *
 * 方法: 在LED全灭时读取ADC值 → 该值为环境光导致的直流偏置
 * 正常工作时从每个样本减去此偏置值
 *
 * @param samples ADC采样数组
 * @param count   采样数量
 * @return 估计的直流偏置值
 */
uint16_t rssi_estimate_dc(const uint16_t *samples, uint16_t count);

/**
 * 自适应更新直流偏置 (慢跟踪环境光变化)
 * @param new_sample 新的ADC采样值
 */
void rssi_update_dc_adaptive(uint16_t new_sample);

/**
 * 获取当前直流偏置
 */
uint16_t rssi_get_dc_bias(void);

/**
 * 检查RSSI信号质量
 * @return 置信度 0~1 (1=完美, <0.2=不可靠)
 */
float rssi_check_quality(goertzel_detector_t *dets[3]);

#endif /* RSSI_EXTRACT_H */
