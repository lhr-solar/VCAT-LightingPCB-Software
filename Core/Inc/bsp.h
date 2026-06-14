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
extern TIM_HandleTypeDef  htim16;
extern DMA_HandleTypeDef  hdma_tim16_ch1_up;
extern UART_HandleTypeDef huart1;

/**
 * @brief Bring up the clock tree and all peripherals (GPIO, DMA, TIM16, CAN1,
 *        USART1). Call once at startup before using any peripheral.
 */
void bsp_init(void);

/**
 * @brief Kick a DMA transfer of `len` PWM duty words out to the WS2814 strip.
 *        Non-blocking; the transfer is stopped in the PWM pulse-finished ISR.
 */
void bsp_strip_show(const uint32_t *buf, uint16_t len);

#endif /* BSP_H */
