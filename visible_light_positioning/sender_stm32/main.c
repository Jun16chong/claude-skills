/**
 * main.c — 发送端 (LED控制电路) 主程序
 *
 * 功能总览:
 *   1. 产生3路PWM载波 (1kHz/2kHz/3kHz) 驱动3个白光LED
 *   2. 键盘数字输入 (发挥部分2)
 *   3. 3路音频ADC采样 + AM调制叠加到LED光 (发挥部分3)
 *   4. 系统状态管理 (校准→定位→数字发送→音频发送)
 *
 * 按键定义:
 *   K1 (数字'1'): LED1单独开关 (调试)
 *   K2 (数字'2'): LED2单独开关
 *   K3 (数字'3'): LED3单独开关
 *   K4 (数字'4'): 全部LED载波模式 (定位状态)
 *   K5 (数字'5'): 全部LED关闭
 *   'A': 启动数字发送模式
 *   'B': 启动音频发送模式
 *   'C': 清除输入缓冲
 *   '#': 确认/发送当前输入
 *
 * CubeMX 配置速查:
 *   System Core → SYS: Debug=Serial Wire
 *   System Core → RCC: HSE=Crystal/Ceramic Resonator
 *   Timers → TIM1: Clock Source=Internal, Channel1=PWM Gen, ARR=71999, Pulse=36000
 *   Timers → TIM2: Clock Source=Internal, Channel1=PWM Gen, ARR=35999, Pulse=18000
 *   Timers → TIM3: Clock Source=Internal, Channel1=PWM Gen, ARR=23999, Pulse=12000
 *   Timers → TIM4: Clock Source=Internal, ARR=8999, Trigger=Update
 *   Analog → ADC1: Scan=Enable, Number=3, ExtTrig=TIM4_TRGO, DMA=Circular
 *   Connectivity → USART1: Mode=Async, Baud=115200
 */

#include "platform.h"
#include "led_modulation.h"
#include "keypad.h"
#include "audio_input.h"

/* 系统状态 */
typedef enum {
    SYS_INIT = 0,       // 初始化中
    SYS_IDLE,           // 空闲 (LED全灭)
    SYS_POSITIONING,    // 定位模式 (3LED载波)
    SYS_DIGITAL_TX,     // 数字发送模式
    SYS_AUDIO_TX,       // 音频发送模式
    SYS_DIGITAL_AUDIO_TX // 数字+音频同时
} system_state_t;

static system_state_t g_sys_state = SYS_INIT;
static uint32_t g_last_scan_ms = 0;
static uint32_t g_digital_tx_index = 0;
static char g_digital_msg[32] = {0};
static int g_digital_msg_len = 0;

/* 函数声明 */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART1_UART_Init(void);
static void process_key_input(key_info_t key_info);
static void digital_tx_service(void);

int main(void)
{
    /* HAL库初始化 */
    HAL_Init();
    SystemClock_Config();

    /* 外设初始化 (CubeMX生成) */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_TIM1_Init();
    MX_TIM2_Init();
    MX_TIM3_Init();
    MX_TIM4_Init();
    MX_ADC1_Init();
    MX_USART1_UART_Init();

    /* 应用模块初始化 */
    led_modulation_init();
    keypad_init();
    audio_input_init();

    /* 进入空闲状态 */
    g_sys_state = SYS_IDLE;

    DBG_PRINT("=== LED Control Circuit Ready ===");
    DBG_PRINT("System State: IDLE");
    DBG_PRINT("Press '4'=Positioning, '5'=Off, 'A'=Digital, 'B'=Audio");

    /* 主循环 */
    uint32_t now;
    while (1) {
        now = HAL_GetTick();

        /* --- 键盘扫描 (每20ms) --- */
        if (now - g_last_scan_ms >= 20) {
            g_last_scan_ms = now;
            key_info_t key = keypad_scan();
            if (key.event == KEY_EVENT_PRESS) {
                process_key_input(key);
            }
        }

        /* --- LED调制服务 (每1ms) --- */
        led_modulation_service();

        /* --- 音频服务 --- */
        if (g_sys_state == SYS_AUDIO_TX || g_sys_state == SYS_DIGITAL_AUDIO_TX) {
            audio_input_service();
        }

        /* --- 数字发送服务 --- */
        if (g_sys_state == SYS_DIGITAL_TX || g_sys_state == SYS_DIGITAL_AUDIO_TX) {
            digital_tx_service();
        }
    }
}

/* ===== 按键处理 ===== */

static void process_key_input(key_info_t key_info)
{
    char k = key_info.key;

    DBG_PRINT("Key: %c", k);

    switch (k) {
    case '1': led_enable(LED1, !led_get_state(LED1)->current_duty); break;
    case '2': led_enable(LED2, !led_get_state(LED2)->current_duty); break;
    case '3': led_enable(LED3, !led_get_state(LED3)->current_duty); break;

    case '4':
        /* 定位模式: 3LED全部载波 */
        g_sys_state = SYS_POSITIONING;
        led_all_carrier_mode();
        DBG_PRINT("Mode: POSITIONING (3 LEDs carrier)");
        break;

    case '5':
        /* 全灭 */
        g_sys_state = SYS_IDLE;
        led_all_off();
        keypad_clear_buffer();
        DBG_PRINT("Mode: IDLE (all off)");
        break;

    case 'A':
        /* 数字发送模式 */
        g_sys_state = SYS_DIGITAL_TX;
        keypad_clear_buffer();
        DBG_PRINT("Mode: DIGITAL TX — Enter digits, press # to send");
        break;

    case 'B':
        /* 音频发送模式 */
        g_sys_state = SYS_AUDIO_TX;
        audio_input_start();
        led_all_carrier_mode();
        for (int i = 0; i < LED_COUNT; i++) {
            led_set_mode((led_id_t)i, LED_MODE_AUDIO);
        }
        DBG_PRINT("Mode: AUDIO TX");
        break;

    case 'C':
        /* 清除缓冲 */
        keypad_clear_buffer();
        DBG_PRINT("Buffer cleared");
        break;

    case '#':
        /* 确认发送数字 */
        if (g_sys_state == SYS_DIGITAL_TX) {
            int len = keypad_get_digit_string(g_digital_msg, sizeof(g_digital_msg));
            if (len > 0) {
                g_digital_msg_len = len;
                g_digital_tx_index = 0;
                DBG_PRINT("Sending digits: %s", g_digital_msg);
            }
            keypad_clear_buffer();
        }
        break;

    case 'D':
        /* 数字+音频同时模式 */
        g_sys_state = SYS_DIGITAL_AUDIO_TX;
        audio_input_start();
        led_all_carrier_mode();
        for (int i = 0; i < LED_COUNT; i++) {
            led_set_mode((led_id_t)i, LED_MODE_DIGITAL_AUDIO);
        }
        DBG_PRINT("Mode: DIGITAL+AUDIO TX");
        break;

    default:
        break;
    }
}

/* ===== 数字发送 ===== */

/*
 * 数字传输协议:
 *   每个LED发送相同的数字信息
 *   帧格式: [起始位=1] [4bit数据 MSB先] [停止位=0]
 *   每位持续 1/BAUD_RATE 秒
 *   波特率 300bps → 每位 3.33ms
 *
 *   发送完当前字符后自动发送下一个
 */
static void digital_tx_service(void)
{
    if (g_digital_msg_len == 0) return;

    /*
     * 简单实现: 将数字字符串整体作为一帧发送
     * 通过改变LED占空比发送bit
     *
     * 时序控制依赖于定周期调用 (已在主循环中每1ms调用)
     * 此处用HAL_GetTick()进行粗略时序控制
     */

    static uint32_t last_bit_tick = 0;
    static int bit_index = -1;  /* -1 = 空闲 */
    static char current_char = 0;
    static int char_index = 0;

    uint32_t now = HAL_GetTick();
    uint32_t bit_period_ms = 1000 / DIGITAL_BAUD_RATE;  /* ~3ms */

    if (bit_index < 0) {
        /* 开始发送下一个字符 */
        if (char_index >= g_digital_msg_len) {
            /* 全部发送完成 */
            g_digital_msg_len = 0;
            char_index = 0;
            /* 恢复载波模式 */
            for (int i = 0; i < LED_COUNT; i++) {
                led_set_mode((led_id_t)i, LED_MODE_CARRIER);
            }
            DBG_PRINT("Digital TX complete");
            return;
        }
        current_char = g_digital_msg[char_index];
        bit_index = 0;
        char_index++;
        last_bit_tick = now;
    }

    if (now - last_bit_tick < bit_period_ms) {
        return;  /* 等待当前bit发送完成 */
    }
    last_bit_tick = now;

    /* 发送当前bit */
    uint8_t bit;
    if (bit_index == 0) {
        bit = 1;  /* 起始位 */
    } else if (bit_index <= 4) {
        /* 4位数据, MSB先 (ASCII数字→BCD) */
        uint8_t digit = current_char - '0';
        if (digit > 9) digit = 0;
        bit = (digit >> (4 - bit_index)) & 0x01;
    } else {
        bit = 0;  /* 停止位 */
    }

    for (int i = 0; i < LED_COUNT; i++) {
        led_set_digital_bit((led_id_t)i, bit);
    }

    bit_index++;
    if (bit_index > 5) {
        /* 当前字符发送完成 */
        bit_index = -1;
    }
}

/* ===== CubeMX 自动生成代码占位 ===== */
/*
 * 以下函数由STM32CubeMX生成，此处仅提供框架
 * 实际开发时在CubeMX中配置外设后自动生成在 main.c 中
 */

static void SystemClock_Config(void)
{
    /* HSE 8MHz → PLL ×9 → SYSCLK 72MHz → APB2 72MHz, APB1 36MHz */
    /* 具体代码由CubeMX生成 */
}

static void MX_GPIO_Init(void)
{
    /* 键盘行/列 GPIO 初始化 */
}

static void MX_DMA_Init(void)
{
    /* DMA1 Channel1 → ADC1 循环模式 */
}

static void MX_TIM1_Init(void)
{
    /* TIM1: PWM CH1, PA8, 1kHz, ARR=71999 */
}

static void MX_TIM2_Init(void)
{
    /* TIM2: PWM CH1, PA0, 2kHz, ARR=35999 */
}

static void MX_TIM3_Init(void)
{
    /* TIM3: PWM CH1, PA6, 3kHz, ARR=23999 */
}

static void MX_TIM4_Init(void)
{
    /* TIM4: 音频ADC触发, 8kHz, ARR=8999, TRGO=Update */
}

static void MX_ADC1_Init(void)
{
    /* ADC1: 3通道 (IN1=PA1, IN2=PA2, IN3=PA3), TIM4触发, DMA */
}

static void MX_USART1_UART_Init(void)
{
    /* USART1: PA9=TX, PA10=RX, 115200, 8N1 */
}

/* ===== 中断处理 ===== */

void SysTick_Handler(void)
{
    HAL_IncTick();
}
