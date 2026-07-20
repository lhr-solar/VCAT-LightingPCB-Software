/**
 ******************************************************************************
 * @file    lighting_config.h
 * @brief   Compile-time config: board select, channel functions, timing.
 *
 * Lights are plain on/off fixtures on two active-high FET GPIO channels per
 * board (CH1 = PA11, CH2 = PA12). Set BOARD_ID, and each board maps its two
 * channels to a function below. Pure macros - safe to include anywhere.
 ******************************************************************************
 */
#ifndef LIGHTING_CONFIG_H
#define LIGHTING_CONFIG_H

/* ============================================================================
 *  CHANNEL FUNCTION CODES  (assigned to CH1_FUNC / CH2_FUNC per board)
 *    FN_NONE            : always off
 *    FN_HEADLIGHT       : on while headlight commanded
 *    FN_TURN_LEFT/RIGHT : blinks while that indicator is on
 *    FN_BRAKE           : on while brake commanded
 *    FN_STROBE          : BPS strobe (held BPS_STROBE_HOLD_MS after last frame)
 *    FN_BRAKE_TURN_LEFT/RIGHT : red light, solid for brake; the turn blink wins
 *                               when that side is requested
 * ============================================================================ */
#define FN_NONE                 0
#define FN_HEADLIGHT            1
#define FN_TURN_LEFT            2
#define FN_TURN_RIGHT          3
#define FN_BRAKE               4
#define FN_STROBE              5
#define FN_BRAKE_TURN_LEFT     6
#define FN_BRAKE_TURN_RIGHT    7

/* ============================================================================
 *  BOARD SELECT  (change per build target)
 * ============================================================================ */
#define BOARD_FRONT     0
#define BOARD_LEFT      1
#define BOARD_RIGHT     2
#define BOARD_REAR      3
#define BOARD_CANOPY    4

#ifndef BOARD_ID
  #define BOARD_ID      BOARD_REAR
#endif

/* CAN IDs */
#define CAN_ID_LIGHTING_COMMAND     0x660
#define CAN_ID_STATUS_FRONT         0x670
#define CAN_ID_STATUS_LEFT          0x671
#define CAN_ID_STATUS_REAR          0x672
#define CAN_ID_STATUS_RIGHT         0x673
#define CAN_ID_STATUS_CANOPY        0x674

/* Per-board channel + status assignment.
 *
 *  Board     CH1 (PA11)          CH2 (PA12)          Status
 *  FRONT     left turn           right turn          0x670
 *  LEFT      -                   -                   0x671 (CAN passthrough)
 *  RIGHT     headlight           headlight           0x673
 *  REAR      brake + left turn   brake + right turn  0x672
 *  CANOPY    strobe              brake               0x674
 */
#if   BOARD_ID == BOARD_FRONT
    #define MY_STATUS_ID    CAN_ID_STATUS_FRONT
    #define CH1_FUNC        FN_TURN_LEFT
    #define CH2_FUNC        FN_TURN_RIGHT
#elif BOARD_ID == BOARD_LEFT
    #define MY_STATUS_ID    CAN_ID_STATUS_LEFT
    #define CH1_FUNC        FN_NONE
    #define CH2_FUNC        FN_NONE
#elif BOARD_ID == BOARD_RIGHT
    #define MY_STATUS_ID    CAN_ID_STATUS_RIGHT
    #define CH1_FUNC        FN_HEADLIGHT
    #define CH2_FUNC        FN_HEADLIGHT
#elif BOARD_ID == BOARD_REAR
    #define MY_STATUS_ID    CAN_ID_STATUS_REAR
    #define CH1_FUNC        FN_BRAKE_TURN_LEFT
    #define CH2_FUNC        FN_BRAKE_TURN_RIGHT
#elif BOARD_ID == BOARD_CANOPY
    #define MY_STATUS_ID    CAN_ID_STATUS_CANOPY
    #define CH1_FUNC        FN_STROBE
    #define CH2_FUNC        FN_BRAKE
#else
    #error "BOARD_ID must be BOARD_FRONT/LEFT/RIGHT/REAR/CANOPY"
#endif

/* ============================================================================
 *  TIMING
 * ============================================================================ */
#define COMMAND_WATCHDOG_MS         500      /* no command within → all off */
#define STATUS_TX_PERIOD_MS         100      /* 10 Hz status                */
#define MAIN_LOOP_PERIOD_MS         1
#define BPS_STROBE_HOLD_MS          1000     /* strobe on 1 s after last msg */
#define TURN_FLASH_PPM              67       /* turn blink rate (pulses/min) */

/* Hold a request active this long after its bit drops (bridges senders that
 * interleave brake/turn frames so outputs don't flicker). */
#define BRAKE_RELEASE_DEBOUNCE_MS       150
#define HEADLIGHT_RELEASE_DEBOUNCE_MS   150
#define TURN_RELEASE_DEBOUNCE_MS        150

/* ============================================================================
 *  FAULT CODES  (matches DBC VAL_TABLE_ Lighting_Board_Fault)
 * ============================================================================ */
#define FAULT_OK                    0
#define FAULT_ADDR_LED_UNDER        1
#define FAULT_LED0_UNDER            2
#define FAULT_LED1_UNDER            3
#define FAULT_ADDR_LED_OVER         4
#define FAULT_LED0_OVER             5
#define FAULT_LED1_OVER             6
#define FAULT_LIGHT_CMD_WATCHDOG    7
#define FAULT_WATCHDOG              8

#endif /* LIGHTING_CONFIG_H */
