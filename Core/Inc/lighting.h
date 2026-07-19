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
extern volatile uint32_t        last_brake_tick; /* HAL_GetTick() of last frame with brake set */
extern volatile uint32_t        last_headlight_tick; /* HAL_GetTick() of last frame with headlight set */
extern volatile uint32_t        last_left_tick;  /* HAL_GetTick() of last frame with left indicator set */
extern volatile uint32_t        last_right_tick; /* HAL_GetTick() of last frame with right indicator set */
extern volatile uint32_t        last_bps_strobe_tick; /* HAL_GetTick() of last frame with bps_strobe set */
extern volatile uint8_t         board_fault;     /* FAULT_* code reported on CAN  */

/**
 * @brief Evaluate the current command against this board's channel functions
 *        and drive the two output channels (on/off + turn blink). Also drives
 *        the command watchdog. Call once per MAIN_LOOP_PERIOD_MS tick.
 */
void render_frame(void);

#endif /* LIGHTING_H */
