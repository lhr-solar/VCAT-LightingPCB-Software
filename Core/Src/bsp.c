/**
 ******************************************************************************
 * @file    bsp.c
 * @brief   Board support - clock/peripheral init and ISR glue.
 *
 * HAL init code mirrors STM32CubeMX output; keep in sync if the .ioc is
 * regenerated. The lights are now plain GPIO outputs (see lighting.c), so the
 * WS281x TIM16/DMA strip output has been removed.
 ******************************************************************************
 */
#include "bsp.h"

/* ============================================================================
 *  Peripheral handles
 * ============================================================================ */
CAN_HandleTypeDef  hcan1;
UART_HandleTypeDef huart1;

/* Forward declarations for the CubeMX-style init helpers. */
void        SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN1_Init(void);
static void MX_USART1_UART_Init(void);

/* ============================================================================
 *  Public API
 * ============================================================================ */
void bsp_init(void) {
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_CAN1_Init();
    MX_USART1_UART_Init();
}

/* ============================================================================
 *  HAL INIT
 * ============================================================================ */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef oscC = {0};
    RCC_ClkInitTypeDef clkC = {0};

    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) Error_Handler();

    oscC.OscillatorType        = RCC_OSCILLATORTYPE_MSI;
    oscC.MSIState              = RCC_MSI_ON;
    oscC.MSICalibrationValue   = 0;
    oscC.MSIClockRange         = RCC_MSIRANGE_6;
    oscC.PLL.PLLState          = RCC_PLL_ON;
    oscC.PLL.PLLSource         = RCC_PLLSOURCE_MSI;
    oscC.PLL.PLLM              = 1;
    oscC.PLL.PLLN              = 40;
    oscC.PLL.PLLP              = RCC_PLLP_DIV7;
    oscC.PLL.PLLQ              = RCC_PLLQ_DIV2;
    oscC.PLL.PLLR              = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&oscC) != HAL_OK) Error_Handler();

    clkC.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clkC.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clkC.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clkC.APB1CLKDivider = RCC_HCLK_DIV1;
    clkC.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clkC, FLASH_LATENCY_4) != HAL_OK) Error_Handler();
}

static void MX_CAN1_Init(void) {
    hcan1.Instance                  = CAN1;
    hcan1.Init.Prescaler            = 20;
    hcan1.Init.Mode                 = CAN_MODE_NORMAL;
    hcan1.Init.SyncJumpWidth        = CAN_SJW_1TQ;
    hcan1.Init.TimeSeg1             = CAN_BS1_13TQ;
    hcan1.Init.TimeSeg2             = CAN_BS2_2TQ;
    hcan1.Init.TimeTriggeredMode    = DISABLE;
    hcan1.Init.AutoBusOff           = ENABLE;
    hcan1.Init.AutoWakeUp           = ENABLE;
    hcan1.Init.AutoRetransmission   = ENABLE;
    hcan1.Init.ReceiveFifoLocked    = DISABLE;
    hcan1.Init.TransmitFifoPriority = ENABLE;
    if (HAL_CAN_Init(&hcan1) != HAL_OK) Error_Handler();
}

static void MX_USART1_UART_Init(void) {
    huart1.Instance                    = USART1;
    huart1.Init.BaudRate               = 115200;
    huart1.Init.WordLength             = UART_WORDLENGTH_8B;
    huart1.Init.StopBits               = UART_STOPBITS_1;
    huart1.Init.Parity                 = UART_PARITY_NONE;
    huart1.Init.Mode                   = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling           = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Default all outputs low. */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);            /* heartbeat        */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8,  GPIO_PIN_RESET);            /* CAN RX heartbeat */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);            /* CAN TX heartbeat */
    HAL_GPIO_WritePin(LIGHT_CH1_GPIO_Port, LIGHT_CH1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LIGHT_CH2_GPIO_Port, LIGHT_CH2_Pin, GPIO_PIN_RESET);

    /* Board heartbeat on PB11. */
    g.Pin   = GPIO_PIN_11;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &g);

    /* CAN RX / TX heartbeats on PA8 / PA15. */
    g.Pin = GPIO_PIN_8 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOA, &g);

    /* Light output channels CH1 (PA11) / CH2 (PA12), both on GPIOA. */
    g.Pin = LIGHT_CH1_Pin | LIGHT_CH2_Pin;
    HAL_GPIO_Init(GPIOA, &g);
}

void Error_Handler(void) {
    __disable_irq();
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { (void)file; (void)line; }
#endif
