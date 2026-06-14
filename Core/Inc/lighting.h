/**
 ******************************************************************************
 * @file    lighting.h
 * @brief   LED pattern engine: command state, frame buffer and per-frame render.
 ******************************************************************************
 */
#ifndef LIGHTING_H
#define LIGHTING_H

#include "main.h"
#include "lighting_config.h"

/* Parsed Lighting_Command (CAN 0x660). Written by the CAN RX module, read by
 * the render loop. */
typedef struct {
    uint8_t headlights;
    uint8_t left_indicator;
    uint8_t right_indicator;
    uint8_t blink_sync;
    uint8_t brake;
    uint8_t bps_strobe;
    uint8_t custom_mode;            /* 0-3 */
} LightingCommand;

/* Shared application state (defined in lighting.c). */
extern volatile LightingCommand cmd;
extern volatile uint32_t        last_cmd_tick;   /* HAL_GetTick() of last command */
extern volatile uint8_t         board_fault;     /* FAULT_* code reported on CAN  */

/* WS2814 DMA buffer (one PWM duty value per colour bit). */
extern uint32_t led_pattern[NUM_STEPS];

/**
 * @brief Render one animation frame into led_pattern based on the current
 *        command, board side and the command watchdog. Call once per
 *        MAIN_LOOP_PERIOD_MS tick.
 */
void render_frame(void);

#endif /* LIGHTING_H */
