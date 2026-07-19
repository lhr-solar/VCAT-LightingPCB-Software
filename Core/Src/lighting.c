/**
 ******************************************************************************
 * @file    lighting.c
 * @brief   GPIO light driver - reads `cmd` and switches the two FET channels.
 *
 * Each board maps CH1/CH2 to a function (see lighting_config.h). No strip,
 * colour or animation: just on/off, plus a simple turn blink.
 ******************************************************************************
 */
#include "lighting.h"

/* Shared application state (see lighting.h). */
volatile LightingCommand cmd                  = {0};
volatile uint32_t        last_cmd_tick        = 0;
volatile uint32_t        last_brake_tick      = 0;
volatile uint32_t        last_headlight_tick  = 0;
volatile uint32_t        last_left_tick       = 0;
volatile uint32_t        last_right_tick      = 0;
volatile uint32_t        last_bps_strobe_tick = 0;
volatile uint8_t         board_fault          = FAULT_OK;

/* A request is active if set now, or set within the last hold_ms (debounce). */
static int held(uint8_t bit_now, uint32_t last_tick, uint32_t hold_ms) {
    if (bit_now) return 1;
    return (last_tick != 0) && ((HAL_GetTick() - last_tick) <= hold_ms);
}

/* Free-running turn blink: on for the first half of each period. */
static int turn_blink_on(void) {
    uint32_t period = 60000u / TURN_FLASH_PPM;
    return (HAL_GetTick() % period) < (period / 2u);
}

static void write_channel_1(int on) {
    HAL_GPIO_WritePin(LIGHT_CH1_GPIO_Port, LIGHT_CH1_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
static void write_channel_2(int on) {
    HAL_GPIO_WritePin(LIGHT_CH2_GPIO_Port, LIGHT_CH2_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* Resolve a channel's on/off state from its function code. */
static int channel_state(int func,
                         int headlight_req, int brake_req,
                         int left_req, int right_req,
                         int blink_on, int strobe_on) {
    switch (func) {
        case FN_HEADLIGHT:        return headlight_req;
        case FN_TURN_LEFT:        return left_req  && blink_on;
        case FN_TURN_RIGHT:       return right_req && blink_on;
        case FN_BRAKE:            return brake_req;
        case FN_STROBE:           return strobe_on;
        /* Turn takes precedence: blinks (cutting the brake) when its side is on. */
        case FN_BRAKE_TURN_LEFT:  return left_req  ? blink_on : brake_req;
        case FN_BRAKE_TURN_RIGHT: return right_req ? blink_on : brake_req;
        case FN_NONE:
        default:                  return 0;
    }
}

void render_frame(void) {
    int watchdog = (HAL_GetTick() - last_cmd_tick) > COMMAND_WATCHDOG_MS;
    board_fault  = watchdog ? FAULT_LIGHT_CMD_WATCHDOG : FAULT_OK;

    /* Watchdog blanks all lights. Strobe is independent (separate safety fixture). */
    int headlight_req = !watchdog && held(cmd.headlights,      last_headlight_tick, HEADLIGHT_RELEASE_DEBOUNCE_MS);
    int brake_req     = !watchdog && held(cmd.brake,           last_brake_tick,     BRAKE_RELEASE_DEBOUNCE_MS);
    int left_req      = !watchdog && held(cmd.left_indicator,  last_left_tick,      TURN_RELEASE_DEBOUNCE_MS);
    int right_req     = !watchdog && held(cmd.right_indicator, last_right_tick,     TURN_RELEASE_DEBOUNCE_MS);

    int strobe_on = (last_bps_strobe_tick != 0) &&
                    ((HAL_GetTick() - last_bps_strobe_tick) <= BPS_STROBE_HOLD_MS);
    int blink_on  = turn_blink_on();

    write_channel_1(channel_state(CH1_FUNC, headlight_req, brake_req,
                                  left_req, right_req, blink_on, strobe_on));
    write_channel_2(channel_state(CH2_FUNC, headlight_req, brake_req,
                                  left_req, right_req, blink_on, strobe_on));
}
