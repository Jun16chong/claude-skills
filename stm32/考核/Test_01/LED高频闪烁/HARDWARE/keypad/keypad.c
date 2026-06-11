/**
 * 4×4 矩阵键盘驱动 (基于已验证的 MatrixKey 逻辑)
 * 行: PA4~PA7 (推挽输出), 列: PB12~PB15 (上拉输入)
 *
 * 丝印交叉点:
 *          C1(PB12) C2(PB13) C3(PB14) C4(PB15)
 * R4(PA7)  S4(A)    S3(3)   S2(2)   S1(1)    ← 键盘第1行(顶)
 * R3(PA6)  S8(B)    S7(6)   S6(5)   S5(4)    ← 键盘第2行
 * R2(PA5)  S12(C)   S11(9)  S10(8)  S9(7)    ← 键盘第3行
 * R1(PA4)  S16(D)   S15(#)  S14(0)  S13(*)   ← 键盘第4行(底)
 *
 * 扫描原理: 轮流拉低一行, 读取列电平 — 被拉低的那列即对应按键按下
 */

#include "keypad.h"
#include "delay.h"
#include "usart3.h"

/* ── 按键映射表 [行][列] ──
 *
 * 丝印交叉点 (来自 MatrixKey 已验证映射):
 *          C1(PB12) C2(PB13) C3(PB14) C4(PB15)
 * R4(PA7)  S4(A)    S3(3)   S2(2)   S1(1)    ← 键盘第1行(顶)
 * R3(PA6)  S8(B)    S7(6)   S6(5)   S5(4)    ← 键盘第2行
 * R2(PA5)  S12(C)   S11(9)  S10(8)  S9(7)    ← 键盘第3行
 * R1(PA4)  S16(D)   S15(#)  S14(0)  S13(*)   ← 键盘第4行(底)
 *
 * 列序与扫描一致: col 0→PB12, col 1→PB13, col 2→PB14, col 3→PB15
 */
static const u8 key_map[4][4] = {
    { KEY_D,    KEY_HASH, KEY_0,    KEY_STAR },   /* PA4 — 第4行(底) */
    { KEY_C,    KEY_9,    KEY_8,    KEY_7    },   /* PA5 — 第3行 */
    { KEY_B,    KEY_6,    KEY_5,    KEY_4    },   /* PA6 — 第2行 */
    { KEY_A,    KEY_3,    KEY_2,    KEY_1    },   /* PA7 — 第1行(顶) */
};

/* 行引脚数组 (扫描顺序: PA4→PA5→PA6→PA7) */
static const u16 row_pins[4] = {
    KEYPAD_ROW1_PIN,   /* PA4 */
    KEYPAD_ROW2_PIN,   /* PA5 */
    KEYPAD_ROW3_PIN,   /* PA6 */
    KEYPAD_ROW4_PIN,   /* PA7 */
};

/* 列引脚数组 (读取顺序: PB12→PB13→PB14→PB15) */
static const u16 col_pins[4] = {
    KEYPAD_COL1_PIN,   /* PB12 */
    KEYPAD_COL2_PIN,   /* PB13 */
    KEYPAD_COL3_PIN,   /* PB14 */
    KEYPAD_COL4_PIN,   /* PB15 */
};

/**
 * 初始化矩阵键盘 GPIO
 * 行 → 推挽输出, 默认高电平
 * 列 → 上拉输入
 */
void Keypad_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 使能 GPIOA, GPIOB 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /* 行引脚 PA4~PA7 → 推挽输出 */
    GPIO_InitStructure.GPIO_Pin   = KEYPAD_ROW_PINS;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(KEYPAD_ROW_PORT, &GPIO_InitStructure);

    /* 列引脚 PB12~PB15 → 上拉输入 */
    GPIO_InitStructure.GPIO_Pin   = KEYPAD_COL_PINS;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_Init(KEYPAD_COL_PORT, &GPIO_InitStructure);

    /* 所有行初始拉高 (无效状态) */
    GPIO_SetBits(KEYPAD_ROW_PORT, KEYPAD_ROW_PINS);
}

/**
 * 原始扫描 — 返回当前按下的键值 (无消抖)
 * 无按键时返回 KEY_NONE (0xFF)
 */
u8 Keypad_Scan(void)
{
    u8 row, col;

    for (row = 0; row < 4; row++)
    {
        /* 拉低当前行 */
        GPIO_ResetBits(KEYPAD_ROW_PORT, row_pins[row]);

        /* 等待电平稳定 */
        delay_us(10);

        /* 逐列检测 */
        for (col = 0; col < 4; col++)
        {
            if (GPIO_ReadInputDataBit(KEYPAD_COL_PORT, col_pins[col]) == 0)
            {
                /* 恢复行电平后返回 */
                GPIO_SetBits(KEYPAD_ROW_PORT, row_pins[row]);
                return key_map[row][col];
            }
        }

        /* 恢复当前行 */
        GPIO_SetBits(KEYPAD_ROW_PORT, row_pins[row]);
    }

    return KEY_NONE;
}

/**
 * 带消抖的按键读取 (参考已验证的 MatrixKey 实现)
 * 消抖流程: 按下消抖 20ms → 再次确认 → 等待释放 → 释放消抖 20ms
 * 返回键值, 无按键时返回 KEY_NONE
 */
u8 Keypad_GetKey(void)
{
    u8 row, col;
    u8 key = KEY_NONE;

    for (row = 0; row < 4; row++)
    {
        /* 拉低当前行 */
        GPIO_ResetBits(KEYPAD_ROW_PORT, row_pins[row]);

        /* 等待电平稳定 */
        delay_us(10);

        /* 逐列检测 */
        for (col = 0; col < 4; col++)
        {
            if (GPIO_ReadInputDataBit(KEYPAD_COL_PORT, col_pins[col]) == 0)
            {
                /* 按下消抖 20ms */
                delay_ms(20);

                /* 再次确认按键是否仍然按下 */
                if (GPIO_ReadInputDataBit(KEYPAD_COL_PORT, col_pins[col]) == 0)
                {
                    key = key_map[row][col];

                    /* 等待按键释放 (阻塞) */
                    while (GPIO_ReadInputDataBit(KEYPAD_COL_PORT, col_pins[col]) == 0);

                    /* 释放消抖 20ms */
                    delay_ms(20);
                }
            }
        }

        /* 恢复当前行 */
        GPIO_SetBits(KEYPAD_ROW_PORT, row_pins[row]);
    }

    return key;
}

/**
 * 调试用: 打印所有行列的原始引脚状态
 * 不按任何键时所有列应读到 1 (上拉)
 * 按下按键时对应列应读到 0
 */
void Keypad_DebugScan(void)
{
    u8 row, col;
    u8 col_states[4];
    u16 row_pin, col_pin;

    printf3("--- Keypad Raw Scan ---\r\n");

    for (row = 0; row < 4; row++)
    {
        /* 获取当前行引脚号 */
        if (row == 0)      row_pin = KEYPAD_ROW1_PIN;
        else if (row == 1) row_pin = KEYPAD_ROW2_PIN;
        else if (row == 2) row_pin = KEYPAD_ROW3_PIN;
        else               row_pin = KEYPAD_ROW4_PIN;

        /* 拉低当前行 */
        GPIO_ResetBits(KEYPAD_ROW_PORT, row_pin);
        delay_us(10);

        /* 读取所有列状态 */
        for (col = 0; col < 4; col++)
        {
            if (col == 0)      col_pin = KEYPAD_COL1_PIN;
            else if (col == 1) col_pin = KEYPAD_COL2_PIN;
            else if (col == 2) col_pin = KEYPAD_COL3_PIN;
            else               col_pin = KEYPAD_COL4_PIN;

            col_states[col] = (GPIO_ReadInputDataBit(KEYPAD_COL_PORT, col_pin) == 0) ? 0 : 1;
        }

        printf3("  R%d: C0=%d C1=%d C2=%d C3=%d\r\n",
                (int)row,
                (int)col_states[0],
                (int)col_states[1],
                (int)col_states[2],
                (int)col_states[3]);

        /* 恢复当前行 */
        GPIO_SetBits(KEYPAD_ROW_PORT, row_pin);
    }

    printf3("------------------------\r\n");
}
