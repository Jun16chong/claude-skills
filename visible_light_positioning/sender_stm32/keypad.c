/**
 * keypad.c — 4x4矩阵键盘驱动实现
 *
 * 扫描原理:
 *   依次将各列置低，读取行值来判断按键位置
 *   列C1=0时，若R1读到0，则R1C1(数字1)被按下
 */

#include "keypad.h"

/* 键盘映射表 */
const char KEYPAD_MAP[KEYPAD_ROWS][KEYPAD_COLS] = {
    {'1', '2', '3', 'A'},   /* R1 */
    {'4', '5', '6', 'B'},   /* R2 */
    {'7', '8', '9', 'C'},   /* R3 */
    {'*', '0', '#', 'D'}    /* R4 */
};

/* GPIO端口和引脚定义 */
#define KEYPAD_ROW_PORT     GPIOB
#define KEYPAD_ROW_PINS     (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3)
#define KEYPAD_COL_PORT     GPIOB
#define KEYPAD_COL_PINS     (GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7)

static const uint16_t row_pins[KEYPAD_ROWS] = {
    GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3
};
static const uint16_t col_pins[KEYPAD_COLS] = {
    GPIO_PIN_4, GPIO_PIN_5, GPIO_PIN_6, GPIO_PIN_7
};

/* 按键去抖状态 */
static char g_last_key = '\0';
static char g_stable_key = '\0';
static uint32_t g_key_press_ms = 0;
static uint32_t g_key_release_ms = 0;
static uint8_t g_key_stable = 0;
static uint32_t g_debounce_count = 0;

/* 数字输入缓冲 */
#define DIGIT_BUF_SIZE  32
static char g_digit_buf[DIGIT_BUF_SIZE];
static int g_digit_count = 0;

/* ===== 初始化 ===== */

void keypad_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* 行: 输入 + 上拉 */
    gpio.Pin = KEYPAD_ROW_PINS;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(KEYPAD_ROW_PORT, &gpio);

    /* 列: 输出, 初始高电平 */
    gpio.Pin = KEYPAD_COL_PINS;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(KEYPAD_COL_PORT, &gpio);

    /* 所有列初始高 */
    KEYPAD_COL_PORT->BSRR = KEYPAD_COL_PINS;

    /* 清空缓冲 */
    memset(g_digit_buf, 0, sizeof(g_digit_buf));
    g_digit_count = 0;
}

/* ===== 扫描 ===== */

key_info_t keypad_scan(void)
{
    key_info_t info = {0};
    info.key = '\0';
    info.event = KEY_EVENT_NONE;

    char detected = '\0';

    /*
     * 逐列扫描:
     * 1. 将当前列置低
     * 2. 读取4行，找到低电平的行=按键按下
     * 3. 恢复当前列为高
     */
    for (int col = 0; col < KEYPAD_COLS; col++) {
        /* 当前列置低 */
        KEYPAD_COL_PORT->BRR = col_pins[col];

        /* 短暂延时让信号稳定 */
        for (volatile int d = 0; d < 10; d++) { __NOP(); }

        /* 扫描行 */
        for (int row = 0; row < KEYPAD_ROWS; row++) {
            if (!(KEYPAD_ROW_PORT->IDR & row_pins[row])) {
                detected = KEYPAD_MAP[row][col];
                break;
            }
        }

        /* 恢复列为高 */
        KEYPAD_COL_PORT->BSRR = col_pins[col];

        if (detected != '\0') break;
    }

    /* 去抖 */
    if (detected != '\0') {
        /* 有按键 */
        if (detected == g_last_key) {
            g_debounce_count++;
            if (g_debounce_count >= 3) {
                /* 稳定按下 */
                if (!g_key_stable) {
                    /* 首次确认按下 */
                    g_stable_key = detected;
                    g_key_stable = 1;
                    g_key_press_ms = HAL_GetTick();
                    info.key = detected;
                    info.event = KEY_EVENT_PRESS;

                    /* 存入数字缓冲 */
                    if (g_digit_count < DIGIT_BUF_SIZE - 1) {
                        g_digit_buf[g_digit_count++] = detected;
                        g_digit_buf[g_digit_count] = '\0';
                    }
                } else if (detected == g_stable_key) {
                    /* 持续按住 */
                    info.key = detected;
                    info.hold_ms = HAL_GetTick() - g_key_press_ms;
                    if (info.hold_ms > 1000) {
                        info.event = KEY_EVENT_HOLD;
                    }
                }
            }
        } else {
            g_last_key = detected;
            g_debounce_count = 0;
        }
    } else {
        /* 无按键 */
        if (g_key_stable) {
            /* 按键释放 */
            info.key = g_stable_key;
            info.event = KEY_EVENT_RELEASE;
            info.hold_ms = HAL_GetTick() - g_key_press_ms;
            g_key_stable = 0;
            g_stable_key = '\0';
            g_key_press_ms = 0;
        }
        g_last_key = '\0';
        g_debounce_count = 0;
    }

    return info;
}

/* ===== 缓冲访问 ===== */

char keypad_get_char(void)
{
    /* 返回最近一次稳定按下的键 */
    return g_stable_key;
}

uint8_t keypad_is_pressed(char key)
{
    return (g_key_stable && g_stable_key == key) ? 1 : 0;
}

int keypad_get_digit_string(char *buf, int size)
{
    if (buf == NULL || size <= 0) return 0;

    int copy_len = (g_digit_count < size - 1) ? g_digit_count : size - 1;
    memcpy(buf, g_digit_buf, copy_len);
    buf[copy_len] = '\0';
    return copy_len;
}

void keypad_clear_buffer(void)
{
    memset(g_digit_buf, 0, sizeof(g_digit_buf));
    g_digit_count = 0;
}
