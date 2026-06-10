/**
 * keypad.h — 4x4矩阵键盘驱动
 *
 * 引脚:
 *   行 R1-R4: PB0-PB3 (GPIO输入, 内部上拉)
 *   列 C1-C4: PB4-PB7 (GPIO输出)
 *
 * 键盘布局 (默认):
 *   C1   C2   C3   C4
 *   [1]  [2]  [3]  [A]  R1
 *   [4]  [5]  [6]  [B]  R2
 *   [7]  [8]  [9]  [C]  R3
 *   [*]  [0]  [#]  [D]  R4
 *
 * 功能: 数字输入用于发挥部分(2)，功能键用于系统控制
 */

#ifndef KEYPAD_H
#define KEYPAD_H

#include "platform.h"

/* 按键事件类型 */
typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_PRESS,        // 按下
    KEY_EVENT_RELEASE,      // 释放
    KEY_EVENT_HOLD          // 长按 (>1s)
} key_event_t;

/* 按键信息 */
typedef struct {
    char key;               // 按键字符 ('0'-'9', 'A'-'D', '*', '#', '\0'=无效)
    key_event_t event;
    uint32_t hold_ms;       // 按住时长 (ms)
} key_info_t;

/* ===== API ===== */

/**
 * 初始化键盘GPIO
 * 行: 输入+上拉, 列: 输出(初始低电平)
 */
void keypad_init(void);

/**
 * 扫描键盘 (应在主循环中定期调用, 建议每20ms)
 * @return 当前检测到的按键信息
 *         若无按键, key='\0', event=KEY_EVENT_NONE
 */
key_info_t keypad_scan(void);

/**
 * 获取最后有效的按键输入 (用于数字输入缓冲)
 * 与 keypad_scan() 不同，此函数返回去抖后的稳定按键
 * @return 按键字符, 或'\0'表示无新输入
 */
char keypad_get_char(void);

/**
 * 检查指定按键是否被按下
 */
uint8_t keypad_is_pressed(char key);

/**
 * 获取输入的数字字符串(用于发挥部分2)
 * 格式: "1234" 表示键盘输入了1-2-3-4
 * @param buf  输出缓冲区
 * @param size 缓冲区大小
 * @return 已输入的字符数
 */
int keypad_get_digit_string(char *buf, int size);

/**
 * 清除输入缓冲
 */
void keypad_clear_buffer(void);

#endif /* KEYPAD_H */
