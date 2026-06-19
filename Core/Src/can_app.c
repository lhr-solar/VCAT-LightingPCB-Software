/**
 ******************************************************************************
 * @file    can_app.c
 * @brief   Application CAN layer matching LightingCAN.dbc.
 *
 * Parses Lighting_Command (ID 0x660) into the shared `cmd` state and broadcasts
 * Lighting_*_Status (ID 0x670 + board offset).
 ******************************************************************************
 */
#include "can_app.h"
#include "bsp.h"               /* hcan1 */
#include "lighting.h"          /* cmd, board_fault, last_cmd_tick */
#include "lighting_config.h"   /* CAN IDs, MY_STATUS_ID */
#include <stm32l4xx_hal_can.h>

/* ============================================================================
 *  Init - RX filter + start + notifications
 * ============================================================================ */
void can_app_init(void) {
    CAN_FilterTypeDef f = {0};
    /* For a 32-bit ID-mask filter on a standard ID, the StdId goes in bits 15:5
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
        /* Track the last time we actually saw the brake asserted, so the render
         * loop can debounce brief brake-bit dropouts (e.g. CAN frames that
         * interleave brake and turn commands) instead of starting the brake's
         * release animation on a single 0. */
        if (cmd.brake) last_brake_tick = HAL_GetTick();

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
void can_status_send(void) {
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
