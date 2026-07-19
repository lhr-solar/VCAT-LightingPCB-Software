/**
 ******************************************************************************
 * @file    bsp.h
 * @brief   Board support: peripheral handles, init and the WS2814 strip output.
 ******************************************************************************
 */
#ifndef BSP_H
#define BSP_H

#include "main.h"

/* Peripheral handles (defined in bsp.c; also extern'd by the CubeMX IT/MSP
 * files, so the symbol names must stay exactly these). */
extern CAN_HandleTypeDef  hcan1;
extern UART_HandleTypeDef huart1;

/**
 * @brief Bring up the clock tree and all peripherals (GPIO, CAN1, USART1).
 *        Call once at startup before using any peripheral.
 */
void bsp_init(void);

#endif /* BSP_H */
