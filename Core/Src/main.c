/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Lighting controller entry point.
  *
  * Wires together the board support (bsp), the LED pattern engine (lighting)
  * and the application CAN layer (can_app):
  *   - bsp.c / bsp.h            : clocks, peripherals, WS2814 strip DMA output
  *   - lighting.c / lighting.h  : patterns + per-frame render into led_pattern
  *   - can_app.c / can_app.h    : Lighting_Command RX, Lighting_*_Status TX
  *   - lighting_config.h        : board selection and all compile-time config
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "lighting_config.h"
#include "bsp.h"
#include "lighting.h"
#include "can_app.h"

int main(void) {
    bsp_init();
    can_app_init();

    last_cmd_tick = HAL_GetTick();
    uint32_t last_status_tick = HAL_GetTick();

    while (1) {
        uint32_t now = HAL_GetTick();

        /* Paint a frame and push it out to the strip. */
        render_frame();
        bsp_strip_show(led_pattern, NUM_STEPS);

        /* Broadcast status at the configured cadence. */
        if ((now - last_status_tick) >= STATUS_TX_PERIOD_MS) {
            last_status_tick = now;
            can_status_send();
        }

        HAL_Delay(MAIN_LOOP_PERIOD_MS);
    }
}
