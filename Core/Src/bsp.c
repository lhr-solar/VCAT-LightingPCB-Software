/**
 ******************************************************************************
 * @file    bsp.c
 * @brief   Board support - clock/peripheral init, strip DMA output, ISR glue.
 *
 * HAL init code mirrors STM32CubeMX output; keep in sync if the .ioc is
 * regenerated.
 ******************************************************************************
 */
#include "bsp.h"

/* ============================================================================
 *  Peripheral handles
 * ============================================================================ */
CAN_HandleTypeDef  hcan1;
TIM_HandleTypeDef  htim16;
DMA_HandleTypeDef  hdma_tim16_ch1_up;
UART_HandleTypeDef huart1;

/* From stm32l4xx_hal_msp.c */
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Forward declarations for the CubeMX-style init helpers. */
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM16_Init(void);
static void MX_CAN1_Init(void);
static void MX_USART1_UART_Init(void);

/* ============================================================================
 *  Public API
 * ============================================================================ */
void bsp_init(void) {
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_TIM16_Init();
    MX_CAN1_Init();
    MX_USART1_UART_Init();
}

void bsp_strip_show(const uint32_t *buf, uint16_t len) {
    __HAL_TIM_SET_COUNTER(&htim16, 0);
    HAL_TIM_PWM_Start_DMA(&htim16, TIM_CHANNEL_1, (uint32_t *)buf, len);
}

/* ============================================================================
 *  PWM/DMA callback - stop the one-shot transfer when the frame is sent.
 * ============================================================================ */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM16) {
        HAL_TIM_PWM_Stop_DMA(&htim16, TIM_CHANNEL_1);
    }
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

static void MX_TIM16_Init(void) {
    TIM_OC_InitTypeDef oc = {0};
    TIM_BreakDeadTimeConfigTypeDef bdt = {0};

    htim16.Instance                = TIM16;
    htim16.Init.Prescaler          = 0;
    htim16.Init.CounterMode        = TIM_COUNTERMODE_UP;
    htim16.Init.Period             = 50;
    htim16.Init.ClockDivision      = TIM_CLOCKDIVISION_DIV1;
    htim16.Init.RepetitionCounter  = 0;
    htim16.Init.AutoReloadPreload  = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim16) != HAL_OK) Error_Handler();
    if (HAL_TIM_PWM_Init(&htim16) != HAL_OK)  Error_Handler();

    oc.OCMode       = TIM_OCMODE_PWM1;
    oc.Pulse        = 25;
    oc.OCPolarity   = TIM_OCPOLARITY_HIGH;
    oc.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    oc.OCFastMode   = TIM_OCFAST_DISABLE;
    oc.OCIdleState  = TIM_OCIDLESTATE_RESET;
    oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    if (HAL_TIM_PWM_ConfigChannel(&htim16, &oc, TIM_CHANNEL_1) != HAL_OK) Error_Handler();

    bdt.OffStateRunMode  = TIM_OSSR_DISABLE;
    bdt.OffStateIDLEMode = TIM_OSSI_DISABLE;
    bdt.LockLevel        = TIM_LOCKLEVEL_OFF;
    bdt.DeadTime         = 0;
    bdt.BreakState       = TIM_BREAK_DISABLE;
    bdt.BreakPolarity    = TIM_BREAKPOLARITY_HIGH;
    bdt.AutomaticOutput  = TIM_AUTOMATICOUTPUT_DISABLE;
    if (HAL_TIMEx_ConfigBreakDeadTime(&htim16, &bdt) != HAL_OK) Error_Handler();

    HAL_TIM_MspPostInit(&htim16);
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

static void MX_DMA_Init(void) {
    __HAL_RCC_DMA1_CLK_ENABLE();
    HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
}

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);

    g.Pin   = GPIO_PIN_11;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &g);

    g.Pin = GPIO_PIN_12;
    HAL_GPIO_Init(GPIOA, &g);
}

void Error_Handler(void) {
    __disable_irq();
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { (void)file; (void)line; }
#endif
