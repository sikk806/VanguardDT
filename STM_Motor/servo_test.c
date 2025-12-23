void Servo_SetAngle(uint8_t angle)
{
  // 1ms ~ 2ms 펄스 → 0도~180도
  // 1ms = 1000, 2ms = 2000 (1us 단위)
  uint16_t min_pulse = 1000;
  uint16_t max_pulse = 2000;

  uint16_t pulse = min_pulse + (max_pulse - min_pulse) * angle / 180;

  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pulse);
}

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_TIM3_Init();

  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);

  while (1)
  {
      Servo_SetAngle(0); // LEFT
      HAL_Delay(1000);

      Servo_SetAngle(75); // MIDDLE
      HAL_Delay(1000);

      Servo_SetAngle(150); // RIGHT
      HAL_Delay(1000);
  }
}