/*
 * uart_app.h
 *
 *  Created on: Nov 27, 2025
 *      Author: SSAFY
 */

#ifndef INC_UART_APP_H_
#define INC_UART_APP_H_

#include "main.h"

void Uart_App_Init(void);
void Uart_App_Task(void);
void Control_Task(void);
void Uart_Print(const char *fmt, ...);

#endif
