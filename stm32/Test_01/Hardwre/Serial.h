#ifndef __SERIAL_H
#define __SERIAL_H

#include "stm32f10x.h"

void Serial_Init(void);
void Serial_SendByte(uint8_t byte);
void Serial_SendString(char *str);
void Serial_SendNum(uint8_t num);

#endif
