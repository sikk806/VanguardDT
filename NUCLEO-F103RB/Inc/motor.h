/*
 * motor.h
 *
 *  Created on: Nov 27, 2025
 *      Author: SSAFY
 */

#ifndef INC_MOTOR_H_
#define INC_MOTOR_H_

#include "main.h"

typedef enum {
  MOTOR_STOP = 0,
  MOTOR_FORWARD,
  MOTOR_BACKWARD
} MotorDir_t;

void Motor_Init(void);
void Motor_SetDirection(MotorDir_t dir);
void Motor_SetSpeedPercent(int8_t speed_percent);

#endif /* INC_MOTOR_H_ */
