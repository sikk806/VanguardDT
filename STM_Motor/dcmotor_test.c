int main(void)
{
  HAL_Init();

  SystemClock_Config();

  MX_GPIO_Init();
  MX_USART2_UART_Init();

  // ENA(PC3) = 1, IN1(PC6) = 1, IN2(PC8) = 0  → 한 방향 회전
  HAL_GPIO_WritePin(GPIOC, Motor_ENA_Pin, GPIO_PIN_SET);   // ENA 켜기
  HAL_GPIO_WritePin(GPIOC, Motor_IN1_Pin, GPIO_PIN_SET);   // IN1 = 1
  HAL_GPIO_WritePin(GPIOC, Motor_IN2_Pin, GPIO_PIN_RESET); // IN2 = 0

  while (1)
  {

  }
}


/*
// gpio.c

void MX_GPIO_Init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOC, Motor_ENA_Pin|Motor_IN1_Pin|Motor_IN2_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = Motor_ENA_Pin|Motor_IN1_Pin|Motor_IN2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}
*/