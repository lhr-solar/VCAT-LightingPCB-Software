/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Lighting controller entry point.
  *
  * Wires together the board support (bsp), the GPIO light driver (lighting)
  * and the application CAN layer (can_app):
  *   - bsp.c / bsp.h            : clocks, peripherals (CAN, UART, GPIO)
  *   - lighting.c / lighting.h  : evaluates the command, drives the two channels
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

        /* Evaluate the command and drive the two GPIO light channels. */
        render_frame();

        /* Broadcast status at the configured cadence. */
        if ((now - last_status_tick) >= STATUS_TX_PERIOD_MS) {
            last_status_tick = now;
            can_status_send();
        }

        //HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_11); /* heartbeat */

        /* Board heartbeat on PB11 - toggle every 500 ms. */
        static uint32_t last_hbt_tick = 0;
        if ((now - last_hbt_tick) >= 500) {
            last_hbt_tick = now;
            HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_11);
        }

        HAL_Delay(MAIN_LOOP_PERIOD_MS);
    }
}
