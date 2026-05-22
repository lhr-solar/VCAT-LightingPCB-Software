/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main_can.c
  * @brief          : Main with CAN integration matching LightingCAN.dbc
  *
  * Parses Lighting_Command (ID 0x660) and broadcasts Lighting_*_Status
  * (ID 0x670 + BOARD_ID_OFFSET) at 10 Hz.
  *
  * NOTE: This is an alternate main. To use it, exclude Core/Src/main.c from the
  * build (or rename it) and include this file instead.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stm32l4xx_hal_can.h>

/* ============================================================================
 *  BOARD CONFIGURATION
 *  Pick which physical board this firmware is for. Affects status TX ID and
 *  which indicator side (left/right) the board responds to.
 * ============================================================================ */
#define BOARD_FRONT     0
#define BOARD_LEFT      1
#define BOARD_REAR      2
#define BOARD_RIGHT     3
#define BOARD_CANOPY    4

#ifndef BOARD_ID
  #define BOARD_ID      BOARD_REAR        /* <-- change per build target */
#endif

/* CAN IDs */
#define CAN_ID_LIGHTING_COMMAND     0x660
#define CAN_ID_STATUS_FRONT         0x670
#define CAN_ID_STATUS_LEFT          0x671
#define CAN_ID_STATUS_REAR          0x672
#define CAN_ID_STATUS_RIGHT         0x673
#define CAN_ID_STATUS_CANOPY        0x674

/* Per-board derived constants */
#if   BOARD_ID == BOARD_FRONT
    #define MY_STATUS_ID            CAN_ID_STATUS_FRONT
    #define RESPONDS_TO_LEFT        1
    #define RESPONDS_TO_RIGHT       1
    #define HEADLIGHT_R             255
    #define HEADLIGHT_G             255
    #define HEADLIGHT_B             255
    #define HEADLIGHT_W             255
#elif BOARD_ID == BOARD_LEFT
    #define MY_STATUS_ID            CAN_ID_STATUS_LEFT
    #define RESPONDS_TO_LEFT        1
    #define RESPONDS_TO_RIGHT       0
    #define HEADLIGHT_R             0
    #define HEADLIGHT_G             0
    #define HEADLIGHT_B             0
    #define HEADLIGHT_W             0       /* side panel - no headlight by default */
#elif BOARD_ID == BOARD_REAR
    #define MY_STATUS_ID            CAN_ID_STATUS_REAR
    #define RESPONDS_TO_LEFT        1
    #define RESPONDS_TO_RIGHT       1
    #define HEADLIGHT_R             255     /* rear "headlight" = tail light, red */
    #define HEADLIGHT_G             0
    #define HEADLIGHT_B             0
    #define HEADLIGHT_W             0
#elif BOARD_ID == BOARD_RIGHT
    #define MY_STATUS_ID            CAN_ID_STATUS_RIGHT
    #define RESPONDS_TO_LEFT        0
    #define RESPONDS_TO_RIGHT       1
    #define HEADLIGHT_R             0
    #define HEADLIGHT_G             0
    #define HEADLIGHT_B             0
    #define HEADLIGHT_W             0
#elif BOARD_ID == BOARD_CANOPY
    #define MY_STATUS_ID            CAN_ID_STATUS_CANOPY
    #define RESPONDS_TO_LEFT        1
    #define RESPONDS_TO_RIGHT       1
    #define HEADLIGHT_R             0
    #define HEADLIGHT_G             0
    #define HEADLIGHT_B             0
    #define HEADLIGHT_W             255
#else
    #error "BOARD_ID must be one of BOARD_FRONT/LEFT/REAR/RIGHT/CANOPY"
#endif

/* Watchdog: turn LEDs off if no command arrives within this many ms */
#define COMMAND_WATCHDOG_MS         500
#define STATUS_TX_PERIOD_MS         100      /* 10 Hz status */
#define MAIN_LOOP_PERIOD_MS         10

/* Fault codes (matches DBC VAL_TABLE_ Lighting_Board_Fault) */
#define FAULT_OK                    0
#define FAULT_ADDR_LED_UNDER        1
#define FAULT_LED0_UNDER            2
#define FAULT_LED1_UNDER            3
#define FAULT_ADDR_LED_OVER         4
#define FAULT_LED0_OVER             5
#define FAULT_LED1_OVER             6
#define FAULT_LIGHT_CMD_WATCHDOG    7
#define FAULT_WATCHDOG              8

/* WS2812 / WS2814 frame */
#define MATTHEW_NUM_QUAD_CHIPS      8
#define NUM_STEPS                   (32 * 4 * MATTHEW_NUM_QUAD_CHIPS)
#define LOW                         30
#define HI                          41
#define TOTAL_LEDS                  (4 * MATTHEW_NUM_QUAD_CHIPS)
#define FIRST_ACTIVE                1

/* ============================================================================
 *  Peripheral handles (defined in HAL init code below)
 * ============================================================================ */
CAN_HandleTypeDef hcan1;
TIM_HandleTypeDef htim16;
DMA_HandleTypeDef hdma_tim16_ch1_up;
UART_HandleTypeDef huart1;

/* Forward declarations for HAL init from STM32CubeMX */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM16_Init(void);
static void MX_CAN1_Init(void);
static void MX_USART1_UART_Init(void);

/* ============================================================================
 *  Application state - set by CAN RX, read by main loop
 * ============================================================================ */
typedef struct {
    uint8_t headlights;
    uint8_t left_indicator;
    uint8_t right_indicator;
    uint8_t blink_sync;
    uint8_t brake;
    uint8_t bps_strobe;
    uint8_t custom_mode;            /* 0-3 */
} LightingCommand;

static volatile LightingCommand cmd        = {0};
static volatile uint32_t last_cmd_tick     = 0;
static volatile uint8_t  board_fault       = FAULT_OK;

/* WS2814 DMA buffer (one PWM duty value per bit) */
uint32_t led_pattern[NUM_STEPS];

/* ============================================================================
 *  LED PATTERN HELPERS
 * ============================================================================ */

/**
 * @brief Write a packed RGBW colour into the DMA buffer for a given LED index.
 *        Bit layout: bits 31-24 = W, 23-16 = R, 15-8 = G, 7-0 = B.
 */
static inline void write_led(int led, uint32_t color) {
    uint32_t bit_index = 0;
    for (int i = 31; i >= 0; i--) {
        uint32_t bit = color & (1u << i);
        led_pattern[bit_index + (led * 32)] = (bit == 0) ? LOW : HI;
        bit_index++;
    }
}

static inline uint32_t pack_rgbw(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    return ((uint32_t)w << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

/* ============================================================================
 *  PATTERN FUNCTIONS
 *  Each function fills `led_pattern` for one frame. Designed to be called once
 *  per main-loop tick (every MAIN_LOOP_PERIOD_MS).
 * ============================================================================ */

/**
 * @brief All LEDs off (with first segment skipped as always).
 */
static void pattern_off(void) {
    for (int led = 0; led < TOTAL_LEDS; led++) write_led(led, 0);
}

/**
 * @brief Solid headlight colour for this board.
 */
static void pattern_headlight(void) {
    uint32_t c = pack_rgbw(HEADLIGHT_R, HEADLIGHT_G, HEADLIGHT_B, HEADLIGHT_W);
    for (int led = 0; led < TOTAL_LEDS; led++) {
        write_led(led, (led < FIRST_ACTIVE) ? 0 : c);
    }
}

/**
 * @brief Turn indicator (amber, fill in -> hold -> fill out -> hold).
 */
static void pattern_turn_ind(void) {
    #define ACTIVE_LEDS         (TOTAL_LEDS - FIRST_ACTIVE)
    #define TURN_FRAMES_PER_LED (20 / MAIN_LOOP_PERIOD_MS)
    #define TURN_HOLD_FRAMES    (60 / MAIN_LOOP_PERIOD_MS)

    enum { T_FILLING, T_HOLD_FULL, T_EMPTYING, T_HOLD_EMPTY };

    static int fill_pos    = 0;
    static int empty_pos   = 0;
    static int state       = T_FILLING;
    static int frame_count = 0;

    /* Amber: R=255, G=100, B=0, W=0 */
    const uint32_t color = pack_rgbw(255, 100, 0, 0);

    frame_count++;
    switch (state) {
        case T_FILLING:
            if (frame_count >= TURN_FRAMES_PER_LED) {
                frame_count = 0;
                if (++fill_pos >= ACTIVE_LEDS) { fill_pos = ACTIVE_LEDS; state = T_HOLD_FULL; }
            }
            break;
        case T_HOLD_FULL:
            if (frame_count >= TURN_HOLD_FRAMES) { frame_count = 0; state = T_EMPTYING; }
            break;
        case T_EMPTYING:
            if (frame_count >= TURN_FRAMES_PER_LED) {
                frame_count = 0;
                if (++empty_pos >= ACTIVE_LEDS) { empty_pos = ACTIVE_LEDS; state = T_HOLD_EMPTY; }
            }
            break;
        case T_HOLD_EMPTY:
            if (frame_count >= TURN_HOLD_FRAMES) {
                frame_count = 0;
                fill_pos = 0; empty_pos = 0;
                state = T_FILLING;
            }
            break;
    }

    for (int led = 0; led < TOTAL_LEDS; led++) {
        uint32_t led_color = 0;
        int active_idx = led - FIRST_ACTIVE;
        if      (led < FIRST_ACTIVE)    led_color = 0;
        else if (state == T_FILLING)    led_color = (active_idx < fill_pos)  ? color : 0;
        else if (state == T_HOLD_FULL)  led_color = color;
        else if (state == T_EMPTYING)   led_color = (active_idx < empty_pos) ? 0 : color;
        else                            led_color = 0;
        write_led(led, led_color);
    }
}

/**
 * @brief Brake light (red, fills in / held while pressed / fills out on release).
 *        Re-call every frame; uses internal state machine driven by `cmd.brake`.
 */
static void pattern_brake(void) {
    #define BRAKE_FRAMES_PER_LED (20 / MAIN_LOOP_PERIOD_MS)

    enum { B_IDLE, B_FILLING, B_HELD, B_EMPTYING };

    static int fill_pos    = 0;
    static int empty_pos   = 0;
    static int state       = B_IDLE;
    static int frame_count = 0;

    const uint32_t color = pack_rgbw(255, 0, 0, 0);

    frame_count++;
    switch (state) {
        case B_IDLE:
            if (cmd.brake) { state = B_FILLING; frame_count = 0; fill_pos = 0; }
            break;
        case B_FILLING:
            if (frame_count >= BRAKE_FRAMES_PER_LED) {
                frame_count = 0;
                if (++fill_pos >= ACTIVE_LEDS) { fill_pos = ACTIVE_LEDS; state = B_HELD; }
            }
            if (!cmd.brake) {
                empty_pos = ACTIVE_LEDS - fill_pos;
                state = B_EMPTYING;
                frame_count = 0;
            }
            break;
        case B_HELD:
            if (!cmd.brake) { state = B_EMPTYING; frame_count = 0; empty_pos = 0; }
            break;
        case B_EMPTYING:
            if (frame_count >= BRAKE_FRAMES_PER_LED) {
                frame_count = 0;
                if (++empty_pos >= ACTIVE_LEDS) {
                    empty_pos = ACTIVE_LEDS;
                    state = B_IDLE;
                    fill_pos = 0; empty_pos = 0;
                }
            }
            if (cmd.brake) {
                fill_pos = ACTIVE_LEDS - empty_pos;
                state = B_FILLING;
                frame_count = 0;
            }
            break;
    }

    for (int led = 0; led < TOTAL_LEDS; led++) {
        uint32_t led_color = 0;
        int active_idx = led - FIRST_ACTIVE;
        if      (led < FIRST_ACTIVE)  led_color = 0;
        else if (state == B_IDLE)     led_color = 0;
        else if (state == B_FILLING)  led_color = (active_idx < fill_pos)  ? color : 0;
        else if (state == B_HELD)     led_color = color;
        else if (state == B_EMPTYING) led_color = (active_idx < empty_pos) ? 0 : color;
        write_led(led, led_color);
    }
}

/**
 * @brief BPS strobe at 90 pulses/min (1.5 Hz), short white flash.
 */
static void pattern_bps_strobe(void) {
    #define STROBE_PERIOD_FRAMES    (667 / MAIN_LOOP_PERIOD_MS)
    #define STROBE_ON_FRAMES        (60  / MAIN_LOOP_PERIOD_MS)

    static int frame_count = 0;
    const uint32_t color = pack_rgbw(0, 0, 0, 255);

    if (++frame_count >= STROBE_PERIOD_FRAMES) frame_count = 0;
    int on = (frame_count < STROBE_ON_FRAMES);

    for (int led = 0; led < TOTAL_LEDS; led++) {
        write_led(led, (led < FIRST_ACTIVE) ? 0 : (on ? color : 0));
    }
}

/**
 * @brief Custom modes (1=rgb rainbow, 2=burnt orange, 3=palette fade, 0=off).
 *        TODO: port the existing matthews_pattner / smooth_palette logic here.
 *        For now just shows headlight colour for any non-zero mode.
 */
static void pattern_custom_mode(uint8_t mode) {
    if (mode == 0) { pattern_off(); return; }
    /* TODO: integrate smooth_rainbow_int / smooth_palette / burnt orange */
    pattern_headlight();
}

/* ============================================================================
 *  PRIORITY DISPATCH
 *  Decides which pattern to draw based on the current command flags and which
 *  side this board is on.
 * ============================================================================ */
static void render_frame(void) {
    /* Watchdog: if no command in COMMAND_WATCHDOG_MS, blank everything */
    if ((HAL_GetTick() - last_cmd_tick) > COMMAND_WATCHDOG_MS) {
        board_fault = FAULT_LIGHT_CMD_WATCHDOG;
        pattern_off();
        return;
    }
    board_fault = FAULT_OK;

    /* Is a turn indicator active for this board's side? */
    uint8_t turn_active =
        (RESPONDS_TO_LEFT  && cmd.left_indicator) ||
        (RESPONDS_TO_RIGHT && cmd.right_indicator);

    /* Priority: brake > turn > strobe > headlight > custom > off */
    if      (cmd.brake)             pattern_brake();
    else if (turn_active)           pattern_turn_ind();
    else if (cmd.bps_strobe)        pattern_bps_strobe();
    else if (cmd.headlights)        pattern_headlight();
    else if (cmd.custom_mode != 0)  pattern_custom_mode(cmd.custom_mode);
    else                            pattern_off();
    
}

/* ============================================================================
 *  CAN RX - parses Lighting_Command (0x660, 1 byte) per DBC bit layout
 * ============================================================================ */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    if (hcan->Instance != CAN1) return;

    CAN_RxHeaderTypeDef rx;
    uint8_t data[8];
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx, data) != HAL_OK) return;

    if (rx.IDE == CAN_ID_STD && rx.StdId == CAN_ID_LIGHTING_COMMAND && rx.DLC >= 1) {
        uint8_t b = data[0];
        cmd.headlights       = (b >> 0) & 0x01;
        cmd.left_indicator   = (b >> 1) & 0x01;
        cmd.right_indicator  = (b >> 2) & 0x01;
        cmd.blink_sync       = (b >> 3) & 0x01;
        cmd.brake            = (b >> 4) & 0x01;
        cmd.bps_strobe       = (b >> 5) & 0x01;
        cmd.custom_mode      = (b >> 6) & 0x03;
        last_cmd_tick = HAL_GetTick();

        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_11); /* heartbeat */
    }
}

/* ============================================================================
 *  CAN TX - Lighting_*_Status (8 bytes per DBC)
 *
 *  Bit layout (little-endian, Intel):
 *   [0  ..  7] : board_fault           (8 bits)
 *   [8]        : headlight             (1 bit)
 *   [9]        : left_indicator        (1 bit)
 *   [10]       : right_indicator       (1 bit)
 *   [11]       : bps_strobe            (1 bit)
 *   [12]       : brakelight            (1 bit)
 *   [13 .. 14] : custom_mode           (2 bits)
 *   [15]       : reserved              (1 bit)
 *   [16 .. 27] : addr_led_current_mA   (12 bits, scale 0.001 A)
 *   [28 .. 31] : reserved              (4 bits)
 *   [32 .. 47] : led0_current_mA       (16 bits, scale 0.001 A)
 *   [48 .. 63] : led1_current_mA       (16 bits, scale 0.001 A)
 * ============================================================================ */
static void send_status(void) {
    CAN_TxHeaderTypeDef tx_header = {0};
    tx_header.StdId = MY_STATUS_ID;
    tx_header.IDE   = CAN_ID_STD;
    tx_header.RTR   = CAN_RTR_DATA;
    tx_header.DLC   = 8;
    tx_header.TransmitGlobalTime = DISABLE;

    uint8_t  data[8] = {0};

    /* TODO: replace with real ADC readings for the LED current sensors */
    uint16_t addr_led_current_mA = 0;   /* 12-bit, max 4095 */
    uint16_t led0_current_mA     = 0;   /* 16-bit */
    uint16_t led1_current_mA     = 0;   /* 16-bit */

    data[0] = board_fault;
    data[1] = (cmd.headlights      << 0)
            | (cmd.left_indicator  << 1)
            | (cmd.right_indicator << 2)
            | (cmd.bps_strobe      << 3)
            | (cmd.brake           << 4)
            | ((cmd.custom_mode & 0x03) << 5);
    /* bit 16 = byte 2 bit 0; pack 12-bit current across bytes 2 and bottom nibble of 3 */
    data[2] = addr_led_current_mA & 0xFF;
    data[3] = (addr_led_current_mA >> 8) & 0x0F;    /* low nibble of byte 3 */
    /* bytes 4-5: led0, bytes 6-7: led1 (little-endian) */
    data[4] = led0_current_mA & 0xFF;
    data[5] = (led0_current_mA >> 8) & 0xFF;
    data[6] = led1_current_mA & 0xFF;
    data[7] = (led1_current_mA >> 8) & 0xFF;

    uint32_t mailbox;
    HAL_CAN_AddTxMessage(&hcan1, &tx_header, data, &mailbox);
}

/* ============================================================================
 *  PWM/DMA callback
 * ============================================================================ */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM16) {
        HAL_TIM_PWM_Stop_DMA(&htim16, TIM_CHANNEL_1);
    }
}

/* ============================================================================
 *  MAIN
 * ============================================================================ */
int main(void) {
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_TIM16_Init();
    MX_CAN1_Init();
    MX_USART1_UART_Init();

    /* CAN filter - accept only Lighting_Command (0x660) */
    CAN_FilterTypeDef f = {0};
    /* For a 32-bit ID-mask filter on standard ID, the StdId goes in bits 15:5
     * of the high register (left-shifted by 5). */
    f.FilterIdHigh         = (CAN_ID_LIGHTING_COMMAND << 5);
    f.FilterIdLow          = 0x0000;
    f.FilterMaskIdHigh     = (0x7FF << 5);
    f.FilterMaskIdLow      = 0x0000;
    f.FilterFIFOAssignment = CAN_RX_FIFO0;
    f.FilterBank           = 0;
    f.FilterMode           = CAN_FILTERMODE_IDMASK;
    f.FilterScale          = CAN_FILTERSCALE_32BIT;
    f.FilterActivation     = ENABLE;
    f.SlaveStartFilterBank = 0;
    HAL_CAN_ConfigFilter(&hcan1, &f);

    HAL_CAN_Start(&hcan1);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
    HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);

    last_cmd_tick = HAL_GetTick();
    uint32_t last_status_tick = HAL_GetTick();

    while (1) {
        uint32_t now = HAL_GetTick();

        /* Render a frame */
        render_frame();
        __HAL_TIM_SET_COUNTER(&htim16, 0);
        HAL_TIM_PWM_Start_DMA(&htim16, TIM_CHANNEL_1, led_pattern, NUM_STEPS);

        /* Send status at STATUS_TX_PERIOD_MS cadence */
        if ((now - last_status_tick) >= STATUS_TX_PERIOD_MS) {
            last_status_tick = now;
            send_status();
        }

        HAL_Delay(MAIN_LOOP_PERIOD_MS);
    }
}

/* ============================================================================
 *  HAL INIT (copied from main.c - keep in sync if .ioc changes regenerate)
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
void assert_failed(uint8_t *file, uint32_t line) {}
#endif
