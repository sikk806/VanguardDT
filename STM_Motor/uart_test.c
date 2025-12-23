int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  char *start_msg = "STM32 UART Test Start\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t*)start_msg, strlen(start_msg), 100);

  uint32_t last_tick = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // 1) 1초에 한 번 상태 메시지 전송
    if (HAL_GetTick() - last_tick >= 1000) {
      last_tick = HAL_GetTick();
      char *msg = "STM32 UART OK\r\n";
      HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
    }

    // 2) 수신 데이터가 있으면 1바이트 읽어서 다시 에코
    uint8_t ch;
    uint8_t buf[2];
    if (HAL_UART_Receive(&huart2, &ch, 1, 10) == HAL_OK) {
      buf[0] = ch;
      buf[1] = '\n';  // 개행 추가
      HAL_UART_Transmit(&huart2, buf, 2, 10);
    }

    // 너무 바쁘게 돌지 않게 약간 쉼
    HAL_Delay(1);

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}


/*
// usart.c
#include "usart.h"

UART_HandleTypeDef huart2;

void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

*/