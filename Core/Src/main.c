/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stm32l4xx_hal_can.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan1;

TIM_HandleTypeDef htim16;
DMA_HandleTypeDef hdma_tim16_ch1_up;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM16_Init(void);
static void MX_CAN1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
uint32_t smooth_palette();
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
  #define MATTHEW_NUM_QUAD_CHIPS 8
  #define NUM_STEPS (32 * 4 * MATTHEW_NUM_QUAD_CHIPS)
  #define LOW 30
  #define HI 41
  uint32_t led_pattern[32 * 4 * MATTHEW_NUM_QUAD_CHIPS];
  uint32_t ledNum =0;

  uint8_t rgb=1;
  uint8_t burntOrange=0;
  uint8_t fade=0;

  #define BURNT_ORANGE_R 199
  #define BURNT_ORANGE_G 104
  #define BURNT_ORANGE_B 35
  #define BURNT_ORAGNE_ARGB ((BURNT_ORANGE_R << 16) | (BURNT_ORANGE_G << 8)  | (BURNT_ORANGE_B))
  
  const uint8_t sin_table[256] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,1,1,1,1,1,2,2,2,3,3,4,4,5,
  5,6,7,8,9,10,11,12,13,15,16,17,19,20,22,24,
  25,27,29,31,33,35,37,40,42,44,47,49,52,55,58,61,
  64,67,70,73,77,80,84,87,91,95,99,103,107,111,115,119,
  123,128,132,137,141,146,151,156,161,166,171,176,181,187,192,198,
  203,209,214,220,226,232,238,244,250,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,250,244,238,232,226,220,
  214,209,203,198,192,187,181,176,171,166,161,156,151,146,141,137,
  132,128,123,119,115,111,107,103,99,95,91,87,84,80,77,73,
  70,67,64,61,58,55,52,49,47,44,42,40,37,35,33,31,
  29,27,25,24,22,20,19,17,16,15,13,12,11,10,9,8,
  7,6,5,5,4,4,3,3,2,2,2,1,1,1,1,1,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
  };
  uint32_t smooth_rainbow_int() {
      static int t = 0; t++; 
      uint8_t r = sin_table[(t) & 255];
      uint8_t g = sin_table[(t + 85) & 255];   // 256/3 ≈ 85
      uint8_t b = sin_table[(t + 170) & 255];  // 2×85
      return (r << 16) | (g << 8) | b;
  }
 uint32_t get_32bit_value(){

    static uint32_t counter = 0;
    counter ++;

    int x = counter % 1536;

    int r, g, b;

    if (x < 256)          { r = 255;        g = x;        b = 0;
    } else if (x < 512)   { r = 511 - x;    g = 255;      b = 0;
    } else if (x < 768)   { r = 0;          g = 255;      b = x - 512;
    } else if (x < 1024)  { r = 0;          g = 1023 - x; b = 255;
    } else if (x < 1280)  { r = x - 1024;   g = 0;        b = 255;
    } else                { r = 255;        g = 0;        b = 1535 - x; }

    uint32_t color = (r << 16) | (g << 8) | b;
    return color;
 }

  void matthews_pattner(){
    uint32_t color;
    if (rgb==1){ color = smooth_rainbow_int(); /* 32 bits of A, R, G, B */ }
    else if (burntOrange) { color = BURNT_ORAGNE_ARGB; } 
    else if (fade) { color = smooth_palette(); }
    else{
      color = 0;
    }
    for(int led = 0; led < (4*MATTHEW_NUM_QUAD_CHIPS); led ++)  {  // have all 4 LEDs be the same changing
      uint32_t bit_index = 0;
      for(int i = 31; i >= 0; i --){
        uint32_t bit = color & (1 << i);
        if(bit == 0){ led_pattern[bit_index + (led * 32)] = LOW; }
        if(bit != 0){ led_pattern[bit_index + (led * 32)] = HI;  }
        bit_index ++;
      }
    }
  }
  void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim){
    // HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_12);
    // HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_12);
    // HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_12);
    if (htim->Instance == TIM16){
      HAL_TIM_PWM_Stop_DMA(&htim16, TIM_CHANNEL_1);
    }
}


// New matthew code 
// ---------------------------------------------------------------
typedef struct {
    uint8_t r, g, b;
} Color;

const Color palette[] = {
    {255,255,255},  // White
    {254,239,180},
    {254,202,88},
    {254,134,38},
    {229,29,124},
    {115,28,151},
    {32,77,142},    // Blue

    {115,28,151},
    {229,29,124},
    {254,134,38},
    {254,202,88},
    {254,239,180},
    {255,255,255}   // White
};

#define NUM_COLORS (sizeof(palette)/sizeof(palette[0]))

static inline uint8_t lerp(uint8_t a, uint8_t b, uint8_t t)
{
    return a + (((int)(b - a) * t) >> 8);
}

uint8_t compute_alpha(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t y = (77 * r + 150 * g + 29 * b) >> 8;

    // compress effect so colors stay saturated
    return (y * 180) >> 8;  // 0–180 instead of 0–255
}

uint32_t smooth_palette()
{
    static uint16_t t = 0;
    t += 10;

    int segment = (t >> 8) % (NUM_COLORS - 1);
    uint8_t local_t = t & 0xFF;

    Color a = palette[segment];
    Color b = palette[segment + 1];

    uint8_t r = lerp(a.r, b.r, local_t);
    uint8_t g = lerp(a.g, b.g, local_t);
    uint8_t bcol = lerp(a.b, b.b, local_t);

    uint8_t alpha = compute_alpha(r, g, bcol);

    return (alpha << 24) | (r << 16) | (g << 8) | bcol;
}

// ---------------------------------------------------------------
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
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
  MX_DMA_Init();
  MX_TIM16_Init();
  MX_CAN1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  CAN_TxHeaderTypeDef txHeader;
  uint8_t txData[8];
  uint32_t txMailbox;
  CAN_FilterTypeDef filterConfig;
  filterConfig.FilterIdHigh = 0x0000;
  filterConfig.FilterIdLow  = 0x0000;
  filterConfig.FilterMaskIdHigh = 0x0000;
  filterConfig.FilterMaskIdLow  = 0x0000;
  filterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  filterConfig.FilterBank = 0;
  filterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  filterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  filterConfig.FilterActivation = ENABLE;
  filterConfig.SlaveStartFilterBank = 0;
  HAL_CAN_ConfigFilter(&hcan1, &filterConfig);
  HAL_CAN_Start(&hcan1);
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
  HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);

  /* Fill TX header */
  txHeader.StdId = 0x123; // 11-bit standard CAN ID
  txHeader.ExtId = 0x00;  // Not used for standard ID
  txHeader.IDE = CAN_ID_STD; // Standard identifier
  txHeader.RTR = CAN_RTR_DATA; // Data frame (not remote)
  txHeader.DLC = 8; // 8 data bytes
  txHeader.TransmitGlobalTime = DISABLE; // TTCM disabled → must be DISABLE

  /* Fill payload (example sensor data) */
  uint32_t counter = 0; 
  txData[4] = 0x00;
  txData[5] = 0x00;
  txData[6] = 0x00;
  txData[7] = 0x00;

  HAL_CAN_Start(&hcan1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t count = 0;
  while (1)
  {
      HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_11);
      matthews_pattner(); 
      __HAL_TIM_SET_COUNTER(&htim16, 0);  // reset counter so first pulse is clean
      HAL_TIM_PWM_Start_DMA(&htim16, TIM_CHANNEL_1, led_pattern, NUM_STEPS);
      HAL_Delay(10);

  }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 20;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_13TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = ENABLE;
  hcan1.Init.AutoWakeUp = ENABLE;
  hcan1.Init.AutoRetransmission = ENABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = ENABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  /* USER CODE END CAN1_Init 2 */

}

/**
  * @brief TIM16 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM16_Init(void)
{

  /* USER CODE BEGIN TIM16_Init 0 */

  /* USER CODE END TIM16_Init 0 */

  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM16_Init 1 */

  /* USER CODE END TIM16_Init 1 */
  htim16.Instance = TIM16;
  htim16.Init.Prescaler = 0;
  htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim16.Init.Period = 50;
  htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim16.Init.RepetitionCounter = 0;
  htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 25;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim16, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim16, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM16_Init 2 */

  /* USER CODE END TIM16_Init 2 */
  HAL_TIM_MspPostInit(&htim16);

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);

  /*Configure GPIO pin : PB11 */
  GPIO_InitStruct.Pin = GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PA12 */
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
// interrupt service routine for CANRX
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    if (hcan->Instance == CAN1) {

        CAN_RxHeaderTypeDef recieve;
        uint8_t data[8];   // <-- FIXED: now 8-byte buffer

        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &recieve, data) == HAL_OK) {

            HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_11);

            // ----- Print ID, DLC, and all data bytes -----
            char msg[100];
            int len = snprintf(msg, sizeof(msg),
                               "ID=0x%03lX DLC=%ld DATA: %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                               recieve.StdId,
                               recieve.DLC,
                               data[0], data[1], data[2], data[3],
                               data[4], data[5], data[6], data[7]);

            HAL_UART_Transmit(&huart1, (uint8_t*)msg, len, HAL_MAX_DELAY);

            // ----- Your original behavior logic -----
            uint8_t d = data[0]; // Interpret first byte as command

            if (d & 0x01) {
                rgb = 1;
                burntOrange = 0;
                fade = 0;
            }
            else if (d & 0x02) {
                burntOrange = 1;
                rgb = 0;
                fade = 0;
            }
            else if (d & 0x04) {
                fade = 1;
                rgb = 0;
                burntOrange = 0;
            }
            else{
              rgb = 0;
              burntOrange = 0;
              fade = 0;
            }
        }
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
