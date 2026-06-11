#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "MatrixKey.h"

/*
 * 矩阵键盘引脚定义
 *
 * 键盘连接器 (8-Pin):
 *   Pin1=C4, Pin2=C3, Pin3=C2, Pin4=C1
 *   Pin5=R1, Pin6=R2, Pin7=R3, Pin8=R4
 *
 * STM32 接线:
 *   行 (Row):  R1→PA4, R2→PA5, R3→PA6, R4→PA7  (推挽输出)
 *   列 (Col):  C1→PB12, C2→PB13, C3→PB14, C4→PB15 (上拉输入)
 *
 * 键盘丝印与矩阵交叉点对应关系（修正后）：
 *           C1(PB12) C2(PB13) C3(PB14) C4(PB15)
 *   R1(PA4)   S16      S15      S14      S13
 *   R2(PA5)   S12      S11      S10      S9
 *   R3(PA6)   S8       S7       S6       S5
 *   R4(PA7)   S4       S3       S2       S1
 */

#define ROW_GPIO    GPIOA
#define ROW_PIN4    GPIO_Pin_4   // R1
#define ROW_PIN5    GPIO_Pin_5   // R2
#define ROW_PIN6    GPIO_Pin_6   // R3
#define ROW_PIN7    GPIO_Pin_7   // R4
#define ROW_COUNT   4

#define COL_GPIO    GPIOB
#define COL_PIN12   GPIO_Pin_12  // C1
#define COL_PIN13   GPIO_Pin_13  // C2
#define COL_PIN14   GPIO_Pin_14  // C3
#define COL_PIN15   GPIO_Pin_15  // C4
#define COL_COUNT   4

static const uint16_t RowPins[ROW_COUNT] = {
    GPIO_Pin_4,   // R1 → PA4
    GPIO_Pin_5,   // R2 → PA5
    GPIO_Pin_6,   // R3 → PA6
    GPIO_Pin_7    // R4 → PA7
};

static const uint16_t ColPins[COL_COUNT] = {
    GPIO_Pin_12,  // C1 → PB12
    GPIO_Pin_13,  // C2 → PB13
    GPIO_Pin_14,  // C3 → PB14
    GPIO_Pin_15   // C4 → PB15
};

// 键值映射表 [行][列] — 与键盘丝印 S1~S16 对应（修正后）
static const uint8_t KeyMap[ROW_COUNT][COL_COUNT] = {
    {16, 15, 14, 13},   // R1(PA4): S16 S15 S14 S13
    {12, 11, 10,  9},   // R2(PA5): S12 S11 S10 S9
    { 8,  7,  6,  5},   // R3(PA6): S8  S7  S6  S5
    { 4,  3,  2,  1}    // R4(PA7): S4  S3  S2  S1
};

/**
  * @brief  矩阵键盘初始化
  *         行引脚(PA4~PA7): 推挽输出，初始高电平
  *         列引脚(PB12~PB15): 上拉输入
  */
void MatrixKey_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // 行引脚 — 推挽输出
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin   = ROW_PIN4 | ROW_PIN5 | ROW_PIN6 | ROW_PIN7;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(ROW_GPIO, &GPIO_InitStructure);

    // 列引脚 — 上拉输入
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin   = COL_PIN12 | COL_PIN13 | COL_PIN14 | COL_PIN15;
    GPIO_Init(COL_GPIO, &GPIO_InitStructure);

    // 所有行输出高电平（无效状态）
    GPIO_SetBits(ROW_GPIO, ROW_PIN4 | ROW_PIN5 | ROW_PIN6 | ROW_PIN7);
}

/**
  * @brief  矩阵键盘扫描，返回按下的键值 (S1~S16)
  * @retval 键值 1~16，无按键按下返回 0
  */
uint8_t MatrixKey_GetKey(void)
{
    uint8_t row, col;
    uint8_t key = 0;

    for (row = 0; row < ROW_COUNT; row++)
    {
        // 当前行输出低电平
        GPIO_ResetBits(ROW_GPIO, RowPins[row]);
        Delay_us(10);

        // 检测各列
        for (col = 0; col < COL_COUNT; col++)
        {
            if (GPIO_ReadInputDataBit(COL_GPIO, ColPins[col]) == 0)
            {
                Delay_ms(20);   // 按下消抖
                if (GPIO_ReadInputDataBit(COL_GPIO, ColPins[col]) == 0)
                {
                    key = KeyMap[row][col];
                    while (GPIO_ReadInputDataBit(COL_GPIO, ColPins[col]) == 0); // 等待释放
                    Delay_ms(20);   // 释放消抖
                }
            }
        }

        // 恢复当前行为高电平
        GPIO_SetBits(ROW_GPIO, RowPins[row]);
    }

    return key;
}
