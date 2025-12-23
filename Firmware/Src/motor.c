/*
 * motor.c
 *
 *  Created on: Nov 27, 2025
 *      Author: SSAFY
 */


#include "motor.h"
#include "gpio.h"
#include "tim.h"

void Motor_Init(void)
{
  HAL_GPIO_WritePin(GPIOC, MOTOR_IN1_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOC, MOTOR_IN2_Pin, GPIO_PIN_RESET);

  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);

  HAL_GPIO_WritePin(GPIOC, MOTOR_IN1_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOC, MOTOR_IN2_Pin, GPIO_PIN_RESET);

  uint32_t period  = __HAL_TIM_GET_AUTORELOAD(&htim2);
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
}

void Motor_SetDirection(MotorDir_t dir)
{
  switch (dir)
  {
    case MOTOR_FORWARD:
      HAL_GPIO_WritePin(GPIOC, MOTOR_IN1_Pin, GPIO_PIN_SET);
      HAL_GPIO_WritePin(GPIOC, MOTOR_IN2_Pin, GPIO_PIN_RESET);
      break;

    case MOTOR_BACKWARD:
      HAL_GPIO_WritePin(GPIOC, MOTOR_IN1_Pin, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(GPIOC, MOTOR_IN2_Pin, GPIO_PIN_SET);
      break;

    case MOTOR_STOP:
    default:
      HAL_GPIO_WritePin(GPIOC, MOTOR_IN1_Pin, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(GPIOC, MOTOR_IN2_Pin, GPIO_PIN_RESET);
      break;
  }
}

// PWM duty (0~100%)
static void Motor_SetDuty(uint8_t duty_percent)
{
    if (duty_percent > 100)
        duty_percent = 100;

    uint32_t period  = __HAL_TIM_GET_AUTORELOAD(&htim2);
    uint32_t compare = (period + 1) * duty_percent / 100;

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, compare);
}

// speed_percent: -100 ~ 100
void Motor_SetSpeedPercent(int8_t speed_percent)
{
    if (speed_percent == 0) {
        Motor_SetDirection(MOTOR_STOP);
        Motor_SetDuty(0);
        return;
    }

    // 방향 설정
    if (speed_percent > 0) {
        Motor_SetDirection(MOTOR_FORWARD);
    } else {
        Motor_SetDirection(MOTOR_BACKWARD);
        speed_percent = -speed_percent;
    }

    if (speed_percent > 100)
        speed_percent = 100;

    // 1~100%  →  63~100%로 선형 매핑
    const uint8_t DUTY_MIN = 63;
    const uint8_t DUTY_MAX = 100;
    uint8_t duty = DUTY_MIN + (uint8_t)(((uint16_t)speed_percent * (DUTY_MAX - DUTY_MIN)) / 100);

    Motor_SetDuty(duty);
}

