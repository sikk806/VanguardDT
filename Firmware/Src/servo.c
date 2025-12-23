/*
 * servo.c
 *
 *  Created on: Nov 27, 2025
 *      Author: SSAFY
 */


#include "servo.h"
#include "tim.h"

#define SERVO_CENTER_ANGLE 75   // 센터 값
#define SERVO_RANGE_ANGLE  75   // ±75도 → 0~150도

// TIM3_CH1, PA6에 PWM 설정
void Servo_Init(void)
{
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
}

void Servo_SetAngle(uint8_t angle)
{
  if (angle > 180) angle = 180;

  // 타이머: 1us 해상도, Period = 19999 (20ms)
  // 1ms ~ 2ms 펄스 → 0~180도
  const uint16_t min_pulse = 1000;   // 1.0ms
  const uint16_t max_pulse = 2000;   // 2.0ms

  uint16_t pulse = min_pulse + (uint16_t)((max_pulse - min_pulse) * angle / 180);

  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pulse);
}

// steer_percent: -100 ~ 100 (좌/우 조향 퍼센트)
void Servo_SetSteerPercent(int8_t steer_percent)
{
    // 범위 클램프
    if (steer_percent > 100)  steer_percent = 100;
    if (steer_percent < -100) steer_percent = -100;

    // -100 → CENTER - RANGE  (0도 근처)
    //  0   → CENTER          (75도)
    // +100 → CENTER + RANGE  (150도 근처)
    int16_t angle = SERVO_CENTER_ANGLE + (int16_t)((int32_t)steer_percent * SERVO_RANGE_ANGLE / 100);

    if (angle < 0)   angle = 0;
    if (angle > 180) angle = 180;

    Servo_SetAngle((uint8_t)angle);
}

