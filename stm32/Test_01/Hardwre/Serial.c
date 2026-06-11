#include "stm32f10x.h"
#include "Serial.h"

/**
 * @brief  串口3初始化
 *         TX: PB10 (复用推挽输出)
 *         RX: PB11 (浮空输入)
 *         波特率: 115200, 8-N-1
 *         时钟: 72MHz (HCLK)
 */
void Serial_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    // PB10 — USART3_TX (复用推挽输出)
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // PB11 — USART3_RX (浮空输入)
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_11;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // USART3 配置: 115200, 8-N-1
    USART_InitStructure.USART_BaudRate            = 115200;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl  = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART3, &USART_InitStructure);

    USART_Cmd(USART3, ENABLE);
}

/**
 * @brief  发送单个字节
 */
void Serial_SendByte(uint8_t byte)
{
    USART_SendData(USART3, byte);
    while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
}

/**
 * @brief  发送字符串
 */
void Serial_SendString(char *str)
{
    while (*str)
    {
        Serial_SendByte(*str++);
    }
}

/**
 * @brief  发送数字（十进制）
 *         例如: 1 → "1", 16 → "16"
 */
void Serial_SendNum(uint8_t num)
{
    if (num >= 100)
    {
        Serial_SendByte('0' + num / 100);
        num %= 100;
    }
    if (num >= 10)
    {
        Serial_SendByte('0' + num / 10);
    }
    Serial_SendByte('0' + num % 10);
}
