/* USER CODE BEGIN Header */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
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

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Example for 4-bit LED pattern: 1,0,1,0
  // uint32_t led_pattern[] = {32000, 16000, 8000, 4000};
  // uint16_t len = sizeof(led_pattern)/sizeof(led_pattern[0]);
  #define NUM_STEPS 320

uint32_t led_pattern[NUM_STEPS];

void generate_led_pattern(void){
    // uint32_t start = 32000;
    // uint32_t end   = 4000;

    // for(int i = 0; i < NUM_STEPS; i++){
    //     // Linear interpolation
    //     // led_pattern[i] = start - ((start - end) * i) / (NUM_STEPS - 1);
    //     led_pattern[i] = i;
    // }

   // uint32_t duty_cycles [8] = {40, 40, 50, 50, 0, 0, 0, 0}; // red
   // uint32_t duty_cycles [4] = {50, 40, 40, 40}; // white
   // uint32_t duty_cycles [4] = {40, 50, 40, 40}; // dim white
   // uint32_t duty_cycles [4] = {40, 40, 50, 40}; // red
   // uint32_t duty_cycles [4] = {40, 40, 40, 50}; // white for a moment then goes to red. think white is a bug

	//					              B    R
	// uint32_t duty_cycles [4] = {40, 50, 50, 40} // purple
	// uint32_t duty_cycles [4] = {40, 50, 40, 50}; // striped R and white pattern
	// uint32_t duty_cycles [4] = {50, 50, 40, 40}; // white
	// uint32_t duty_cycles [4] = {40, 50, 40, 0}; // dim white
	// uint32_t duty_cycles [4] = {0, 50, 0, 0}; // dim white


  // all blue 
	// uint32_t duty_cycles [20] = {30, 30, 41, 41,
	// 							 30, 30, 41, 41,
	// 							 30, 30, 41, 41,
	// 							 30, 30, 41, 41,
  //                30, 30, 41, 41};

  // white 
	uint32_t duty_cycles [20] = {30, 41, 41, 41,
                               30, 41, 41, 30,
                               30, 41, 30, 30,
                               30, 30, 30, 30};






    for (int j = 0; j < 20; j++){
      for(int i = j*8; i < (j*8+8); i ++){
        // led_pattern[i] = (j+1) * 6; // 6, 12, 18, 24, 30, 36, 42, 48
        led_pattern[i] = duty_cycles[j];
        // led_pattern[i]=50;
        // 5%, 11.84%, 17.84%, 23.6% 
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
  /* USER CODE BEGIN 2 */
  // HAL_TIM_PWM_Start(&htim16, TIM_CHANNEL_1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  // HAL_TIM_PWM_Start_DMA(&htim16, TIM_CHANNEL_1, led_pattern, 32);

    generate_led_pattern();
    // uint16_t len = sizeof(led_pattern)/sizeof(led_pattern[0]);

    while(1){
    //HAL_Delay(100);
    // HAL_TIM_PWM_Stop(&htim16, TIM_CHANNEL_1);
    //HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);
    //HAL_Delay(100);
    //HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_SET);
    // HAL_TIM_PWM_Start(&htim16, TIM_CHANNEL_1);
    //HAL_TIM_PWM_Stop_DMA(&htim16, TIM_CHANNEL_1);
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

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
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
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
