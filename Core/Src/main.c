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

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM16_Init(void);
static void MX_CAN1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Example for 4-bit LED pattern: 1,0,1,0
  // uint32_t led_pattern[] = {32000, 16000, 8000, 4000};
  // uint16_t len = sizeof(led_pattern)/sizeof(led_pattern[0]);
  #define NUM_STEPS 256
  #define LOW 30
  #define HI 41

uint32_t led_pattern[128];
uint32_t ledNum =0;

// void generate_led_pattern(void){
//     // uint32_t start = 32000;
//     // uint32_t end   = 4000;

//     // for(int i = 0; i < NUM_STEPS; i++){
//     //     // Linear interpolation
//     //     // led_pattern[i] = start - ((start - end) * i) / (NUM_STEPS - 1);
//     //     led_pattern[i] = i;
//     // }

//    // uint32_t duty_cycles [8] = {40, 40, 50, 50, 0, 0, 0, 0}; // red
//    // uint32_t duty_cycles [4] = {50, 40, 40, 40}; // white
//    // uint32_t duty_cycles [4] = {40, 50, 40, 40}; // dim white
//    // uint32_t duty_cycles [4] = {40, 40, 50, 40}; // red
//    // uint32_t duty_cycles [4] = {40, 40, 40, 50}; // white for a moment then goes to red. think white is a bug

// 	//					              B    R
// 	// uint32_t duty_cycles [4] = {40, 50, 50, 40} // purple
// 	// uint32_t duty_cycles [4] = {40, 50, 40, 50}; // striped R and white pattern
// 	// uint32_t duty_cycles [4] = {50, 50, 40, 40}; // white
// 	// uint32_t duty_cycles [4] = {40, 50, 40, 0}; // dim white
// 	// uint32_t duty_cycles [4] = {0, 50, 0, 0}; // dim white


//   // all blue 
// 	// uint32_t duty_cycles [20] = {30, 30, 41, 41,
// 	// 							 30, 30, 41, 41,
// 	// 							 30, 30, 41, 41,
// 	// 							 30, 30, 41, 41,
//   //                30, 30, 41, 41};

//   // white 
// 	// uint32_t duty_cycles [20] = {30, 41, 41, 41, // off
//   //                              30, 41, 41, 30, // red
//   //                              30, 41, 30, 30, // yellow
//   //                              30, 30, 30, 30};// white
//   // uint32_t duty_cycles [20] = {30, 41, 41, 41, // off
//   //                              30, 41, 41, 30, // green
//   //                              30, 41, 30, 30, // yellow
//   //                              30, 30, 30, 30};// white

//   //                           A   R   G   B
// 	// uint32_t duty_cycles [16] = {30, 41, 30, 41, // purple
//   //                              30, 41, 30, 41, // purple
//   //                              30, 41, 30, 41, // purple
//   //                              30, 41, 30, 41};// purple
//                                A   R   G   B
// 	uint32_t duty_cycles [16] = {41, 30, 30, 30, // white
//                               30, 41, 30, 30, // red
//                               30, 30, 41, 30, // green
//                               30, 30, 30, 41};// blue
//      for (int j = 0; j < 16; j++){
//       for(int i = j*8; i < (j*8+8); i ++){
//         // led_pattern[i] = (j+1) * 6; // 6, 12, 18, 24, 30, 36, 42, 48
//         led_pattern[i] = duty_cycles[j];        // led_pattern[i]=50;
//         // 5%, 11.84%, 17.84%, 23.6% 
//       }
//     }
// }

// void singleWRGBTransition(uint32_t ledNum){
//   uint32_t circularRange = ledNum%4;
  
//   for(int led=0; led<4; led++){
//     uint32_t A=0;
//     uint32_t R=0;
//     uint32_t G=0;
//     uint32_t B=0;

//     if (led==circularRange){
//       if     (led==0){ A=HI;  R=LOW; G=LOW; B=LOW;}
//       else if(led==1){ A=LOW; R=HI;  G=LOW; B=LOW;}
//       else if(led==2){ A=LOW; R=LOW; G=HI;  B=LOW;}
//       else           { A=LOW; R=LOW; G=LOW; B=HI; }
//     } 
//     else{ A=LOW; R=LOW; G=LOW; B=LOW; }

//     for(int j = 0; j < 4; j++){ // byte
//       for(int i = j*8 + (led*32); i < (j*8+8) + (led*32); i ++){
//         if(j == 0){ led_pattern[i] =  A; }
//         if(j == 1){ led_pattern[i] =  R; }
//         if(j == 2){ led_pattern[i] =  G; }
//         if(j == 3){ led_pattern[i] =  B; }
//       }
//     }

//     // led_pattern 32bits = led0, 32 bits = led1, 32 bits = l3d3 

//   }
// }


  // const uint8_t sin_table[256] = { 128,131,134,137,140,143,146,149,152,155,158,162,165,168,171,174, 177,180,183,186,189,192,195,198,201,204,207,210,213,216,219,222, 224,227,230,233,236,238,241,244,246,249,251,254,255,255,255,255, 255,255,255,255,255,255,255,255,254,251,249,246,244,241,238,236, 233,230,227,224,222,219,216,213,210,207,204,201,198,195,192,189, 186,183,180,177,174,171,168,165,162,158,155,152,149,146,143,140, 137,134,131,128,124,121,118,115,112,109,106,103,100,97,94,90, 87,84,81,78,75,72,69,66,63,60,57,54,51,48,45,42, 39,36,33,30,27,24,22,19,16,13,11,8,6,3,1,0, 0,0,0,0,0,0,0,0,1,3,6,8,11,13,16,19, 22,24,27,30,33,36,39,42,45,48,51,54,57,60,63,66, 69,72,75,78,81,84,87,90,94,97,100,103,106,109,112,115, 118,121,124,127 };
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
    uint32_t color = smooth_rainbow_int(); // 32 bits of A, R, G, B
    for(int led = 0; led < 4; led ++)  {  // have all 4 LEDs be the same changing
      uint32_t bit_index = 0;
      for(int i = 31; i >= 0; i --){
        uint32_t bit = color & (1 << i);
        if(bit == 0){ led_pattern[bit_index + (led * 32)] = LOW; }
        if(bit != 0){ led_pattern[bit_index + (led * 32)] = HI;  }
        bit_index ++;
      }
  }
  }


//

 void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim){
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_12);
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_12);
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_12);
    if (htim->Instance == TIM16){
      HAL_TIM_PWM_Stop_DMA(&htim16, TIM_CHANNEL_1);
    }
}

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
  /* USER CODE BEGIN 2 */
  CAN_TxHeaderTypeDef txHeader;
  uint8_t txData[8];
  uint32_t txMailbox;

  CAN_FilterTypeDef filterConfig;
  filterConfig.FilterBank = 0;
  filterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  filterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  filterConfig.FilterIdHigh = 0x0000;
  filterConfig.FilterIdLow = 0x0000;
  filterConfig.FilterMaskIdHigh = 0x0000;
  filterConfig.FilterMaskIdLow = 0x0000;
  filterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  filterConfig.FilterActivation = ENABLE;
  filterConfig.SlaveStartFilterBank = 14;

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
      txData[0] = (uint8_t)(counter & 0xFF);
      txData[1] = (uint8_t)((counter >> 8) & 0xFF);
      txData[2] = (uint8_t)((counter >> 16) & 0xFF);
      txData[3] = (uint8_t)((counter >> 24) & 0xFF);
      // Heartbeat: Toggle GPIOB Pin 11 to show the code is running
      HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_11);
      if(HAL_CAN_GetTxMailboxesFreeLevel(&hcan1)){
        // if mailboxes are not full
        // HAL_Delay(1);
        if (HAL_CAN_AddTxMessage(&hcan1, &txHeader, txData, &txMailbox) != HAL_OK){
            // Mailboxes are full or another error occurred.
        }else{
          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3,0);
          counter++;
        }
      }else{
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3,1);
      }
      if(count == 10){ matthews_pattner(); count = 0; } count++;
      HAL_TIM_PWM_Start_DMA(&htim16, TIM_CHANNEL_1, led_pattern, NUM_STEPS);
      HAL_Delay(1);
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
  hcan1.Init.Prescaler = 10;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_12TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_3TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = ENABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
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
