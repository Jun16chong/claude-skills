/**
 * platform.h — 接收端DSP核心 (测量电路) 硬件平台定义
 *
 * MCU: STM32F103C8T6, 72MHz
 * 功能: OPT101光信号ADC采集 → Goertzel频率分离 →
 *       RSSI提取 → 三边定位解算 → UART发送结果给K230
 *
 * 引脚分配:
 *   OPT101信号:  PA0      (ADC1_IN0)
 *   →K230 UART:  PA9=TX, PA10=RX  (UART1, 115200)
 *   调试UART:    PA2=TX, PA3=RX   (UART2, 115200, CH340)
 *   SWD:         PA13=SWDIO, PA14=SWCLK
 *   状态LED:     PC13 (板载LED, 低电平亮)
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

/* ===== 系统时钟 ===== */
#define SYSTEM_CLOCK_HZ     72000000U
#define APB2_TIMER_CLOCK    SYSTEM_CLOCK_HZ  /* APB2定时器时钟 = 72MHz */

/* ===== ADC 采样参数 ===== */
#define ADC_SAMPLE_RATE     8000U           /* 8kHz 采样率 */
#define ADC_SAMPLE_PERIOD_US  125U          /* 125μs */
#define ADC_BUFFER_SIZE     512U            /* DMA缓冲大小 (采样点数) */
#define ADC_HALF_BUFFER     (ADC_BUFFER_SIZE / 2)

/* ===== Goertzel 算法参数 ===== */
/*
 * 三个检测频率 (与发送端LED载波对应):
 *   F1 = 1000 Hz
 *   F2 = 2000 Hz
 *   F3 = 3000 Hz
 *
 * 采样点数 N = 400:
 *   覆盖 1kHz 整数50周期、2kHz 整数100周期、3kHz 整数150周期
 *   (前提: 采样率 = 8000Hz)
 */
#define GOERTZEL_N          400U            /* 采样点数 */
#define GOERTZEL_F1         1000.0f         /* LED1 载波 */
#define GOERTZEL_F2         2000.0f         /* LED2 载波 */
#define GOERTZEL_F3         3000.0f         /* LED3 载波 */
#define GOERTZEL_FS         8000.0f         /* 采样率 */

/* ===== 定位参数 ===== */
/*
 * 立方空间 80×80×80cm
 * 底部坐标: X∈[-40, +40], Y∈[-40, +40] cm
 * LED在顶部 (Z=80cm), 传感器在底部 (Z=0)
 *
 * LED典型安装位置 (三角布局):
 *   LED1: (-20, -20, 80) cm    1kHz
 *   LED2: (+20, -20, 80) cm    2kHz
 *   LED3: (  0, +20, 80) cm    3kHz
 *
 * 区域划分:
 *   A: x∈[-20,+20], y∈[-20,+20]  (中心区域)
 *   B: x∈[-40,+40], y∈[  0,+40]  (X轴上方)
 *   C: x∈[  0,+40], y∈[-40,+40]  (Y轴右侧)
 *   D: x∈[-40,+40], y∈[-40,  0]  (X轴下方)
 *   E: x∈[-40,  0], y∈[-40,+40]  (Y轴左侧)
 */
#define CUBE_SIZE_CM        80.0f
#define CUBE_HALF_CM        40.0f
#define LED_HEIGHT_CM       80.0f           /* LED安装高度 Z坐标 */

/* LED安装位置 (三角布局) */
#define LED1_X_CM           -20.0f
#define LED1_Y_CM           -20.0f
#define LED2_X_CM           20.0f
#define LED2_Y_CM           -20.0f
#define LED3_X_CM           0.0f
#define LED3_Y_CM           20.0f

/* 定位精度目标 */
#define POS_ACCURACY_BASIC  10.0f           /* 基本要求 ≤10cm */
#define POS_ACCURACY_ADV    3.0f            /* 发挥要求 ≤3cm */
#define POS_LOCK_TIMEOUT_MS 5000U           /* 位置锁定超时5秒 */

/* 每100ms输出一次定位结果 (10Hz) */
#define POS_UPDATE_PERIOD_MS 100U

/* RSSI指纹库参数 */
#define FINGERPRINT_GRID_SIZE   5           /* 5×5 粗网格标定点 */
#define FINGERPRINT_MAX_POINTS  (FINGERPRINT_GRID_SIZE * FINGERPRINT_GRID_SIZE)

/* ===== UART 通信协议 (→K230) ===== */
#define UART_FRAME_HEADER1  0xFF
#define UART_FRAME_HEADER2  0xFE
#define UART_FRAME_SIZE     16U            /* 定长16字节帧 */

/* 命令定义 */
#define CMD_POSITION        0x01    /* 定位结果 X,Y */
#define CMD_DIGITAL_RX      0x02    /* 数字接收 AUDIO字节=数字 */
#define CMD_AUDIO_DATA      0x03    /* 音频PCM数据 */
#define CMD_SYS_STATUS      0x04    /* 系统状态 */

/* K230 UART */
#define K230_UART           huart1
#define K230_BAUDRATE       115200U

/* ===== 调试 ===== */
#define DEBUG_UART          huart2
#define DEBUG_BAUDRATE      115200U

#ifdef DEBUG_ENABLE
  #define DBG_PRINT(fmt, ...)  printf("[RECEIVER] " fmt "\r\n", ##__VA_ARGS__)
#else
  #define DBG_PRINT(fmt, ...)  ((void)0)
#endif

/* ===== 外部句柄 (CubeMX生成) ===== */
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim_adc_trig;  /* ADC触发定时器 */
extern UART_HandleTypeDef K230_UART;
extern UART_HandleTypeDef DEBUG_UART;

#endif /* PLATFORM_H */
