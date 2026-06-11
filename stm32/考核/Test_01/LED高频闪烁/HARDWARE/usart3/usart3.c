/**
 * USART3 驱动 — 调试串口
 * TX: PB10, RX: PB11, 波特率 9600
 *
 *   1. printf3 — 格式化输出到串口 (TXE 等待 + 发送)
 *   2. 上电自检 — 启动后发送 "USART3 OK\r\n"
 *   3. RX 中断 — 仅清除标志位，不做命令解析
 *   4. RX 引脚 — 上拉输入，避免悬空触发噪声中断
 */

#include "usart3.h"
#include "stdio.h"
#include <stdarg.h>

/* ---- 全局变量 (UART 备用控制接口) ---- */
u8 flag_led = 0;    /* 0=无选中, 1=LED A, 2=LED B, 3=LED C */
u8 flag_num = 0;    /* 要发送的数字 (0~9) */
u8 flag     = 0;    /* 发送触发标志 */

/* ---- 内部辅助 ---- */

/**
 * 发送单个字节（阻塞，等 TXE）
 */
static void usart3_putc(u8 ch)
{
    /* 等待发送数据寄存器空 */
    while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
    USART_SendData(USART3, ch);
}

/**
 * 等待全部字节发送完毕（等 TC）
 */
static void usart3_wait_tc(void)
{
    while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
}

/* ---- 公开函数 ---- */

/**
 * USART3 初始化
 * @param bound: 波特率 (如 9600)
 */
void USART3_Init(u32 bound)
{
    GPIO_InitTypeDef GPIO_InitStrue;
    USART_InitTypeDef USART_InitStrue;
    NVIC_InitTypeDef NVIC_InitStrue;

    /* 时钟使能 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
    USART_DeInit(USART3);

    /* TX — PB10, 复用推挽 */
    GPIO_InitStrue.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStrue.GPIO_Pin   = GPIO_Pin_10;
    GPIO_InitStrue.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStrue);

    /* RX — PB11, 上拉输入 (防悬空噪声) */
    GPIO_InitStrue.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStrue.GPIO_Pin   = GPIO_Pin_11;
    GPIO_Init(GPIOB, &GPIO_InitStrue);

    /* 串口参数配置 */
    USART_InitStrue.USART_BaudRate            = bound;
    USART_InitStrue.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStrue.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_InitStrue.USART_Parity              = USART_Parity_No;
    USART_InitStrue.USART_StopBits            = USART_StopBits_1;
    USART_InitStrue.USART_WordLength          = USART_WordLength_8b;
    USART_Init(USART3, &USART_InitStrue);

    /* 使能串口 + 接收中断 */
    USART_Cmd(USART3, ENABLE);
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

    /* 中断优先级配置 */
    NVIC_InitStrue.NVIC_IRQChannel                   = USART3_IRQn;
    NVIC_InitStrue.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_InitStrue.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStrue.NVIC_IRQChannelSubPriority        = 1;
    NVIC_Init(&NVIC_InitStrue);

    /* ---- 上电自检: 直接发送测试字符串 ---- */
    usart3_putc('U');
    usart3_putc('S');
    usart3_putc('A');
    usart3_putc('R');
    usart3_putc('T');
    usart3_putc('3');
    usart3_putc(' ');
    usart3_putc('O');
    usart3_putc('K');
    usart3_putc('\r');
    usart3_putc('\n');
    usart3_wait_tc();
}

/**
 * 格式化打印到 USART3
 *
 * 用法同 printf:
 *   printf3("LED=%d, Num=%d\r\n", led, num);
 */
void printf3(char *fmt, ...)
{
    char buffer[CMD_BUFFER_LEN + 1];
    va_list arg_ptr;
    u8 i = 0;

    /* 格式化字符串 */
    va_start(arg_ptr, fmt);
    vsnprintf(buffer, CMD_BUFFER_LEN + 1, fmt, arg_ptr);
    va_end(arg_ptr);

    /* 逐字节发送 */
    while (i < CMD_BUFFER_LEN && buffer[i] != '\0')
    {
        usart3_putc((u8)buffer[i]);
        i++;
    }

    /* 等待最后一个字节的停止位发出 */
    usart3_wait_tc();
}

/**
 * 发送单个字节 (兼容旧接口)
 */
void Usart3_send(u8 com)
{
    usart3_putc(com);
    usart3_wait_tc();
}

/**
 * 发送单个字节 (兼容旧接口，不等 TC)
 */
void uart3_senddata(u8 ch)
{
    usart3_putc(ch);
}

/* ---- USART3 接收中断服务函数 ---- */

void USART3_IRQHandler(void)
{
    /* 仅用于调试输出，收到数据直接丢弃 */
    if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
    {
        USART_ReceiveData(USART3);  /* 清除 RXNE 标志 */
    }
}
