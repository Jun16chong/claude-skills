#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "Serial.h"
#include "MatrixKey.h"

uint8_t KeyNum;

int main(void)
{
    Serial_Init();
    MatrixKey_Init();

    Serial_SendString("Matrix KeyBoard Ready!\r\n");

    while (1)
    {
        KeyNum = MatrixKey_GetKey();

        if (KeyNum >= 1 && KeyNum <= 16)
        {
            Serial_SendNum(KeyNum);
            Serial_SendString("\r\n");
        }
    }
}
