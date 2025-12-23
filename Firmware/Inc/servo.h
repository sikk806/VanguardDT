/*
 * servo.h
 *
 *  Created on: Nov 27, 2025
 *      Author: SSAFY
 */

#ifndef INC_SERVO_H_
#define INC_SERVO_H_

#include "main.h"

void Servo_Init(void);
void Servo_SetAngle(uint8_t angle);   // 0~180도
void Servo_SetSteerPercent(int8_t steer_percent);

#endif /* INC_SERVO_H_ */
