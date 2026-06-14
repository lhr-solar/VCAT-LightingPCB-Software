/**
 ******************************************************************************
 * @file    lighting_config.h
 * @brief   Compile-time configuration for the lighting firmware.
 *
 * Board selection and all per-board constants, CAN IDs, fault codes, LED frame
 * geometry, loop timing and the animation mode switch live here so every module
 * shares one source of truth. Pure macros - safe to include anywhere.
 ******************************************************************************
 */
#ifndef LIGHTING_CONFIG_H
#define LIGHTING_CONFIG_H

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
    #define HEADLIGHT_R             0
    #define HEADLIGHT_G             0
    #define HEADLIGHT_B             0
    #define HEADLIGHT_W             255
    #define MATTHEW_NUM_QUAD_CHIPS      8
#elif BOARD_ID == BOARD_LEFT
    #define MY_STATUS_ID            CAN_ID_STATUS_LEFT
    #define RESPONDS_TO_LEFT        1
    #define RESPONDS_TO_RIGHT       0
    #define HEADLIGHT_R             0
    #define HEADLIGHT_G             0
    #define HEADLIGHT_B             0
    #define HEADLIGHT_W             0       /* side panel - no headlight by default */
    #define MATTHEW_NUM_QUAD_CHIPS      8
#elif BOARD_ID == BOARD_REAR
    #define MY_STATUS_ID            CAN_ID_STATUS_REAR
    #define RESPONDS_TO_LEFT        1
    #define RESPONDS_TO_RIGHT       1
    #define HEADLIGHT_R             64     /* rear "headlight" = tail light, red */
    #define HEADLIGHT_G             0
    #define HEADLIGHT_B             0
    #define HEADLIGHT_W             0
    #define MATTHEW_NUM_QUAD_CHIPS      11
#elif BOARD_ID == BOARD_RIGHT
    #define MY_STATUS_ID            CAN_ID_STATUS_RIGHT
    #define RESPONDS_TO_LEFT        0
    #define RESPONDS_TO_RIGHT       1
    #define HEADLIGHT_R             0
    #define HEADLIGHT_G             0
    #define HEADLIGHT_B             0
    #define HEADLIGHT_W             0
    #define MATTHEW_NUM_QUAD_CHIPS      8
#elif BOARD_ID == BOARD_CANOPY
    #define MY_STATUS_ID            CAN_ID_STATUS_CANOPY
    #define RESPONDS_TO_LEFT        1
    #define RESPONDS_TO_RIGHT       1
    #define HEADLIGHT_R             0
    #define HEADLIGHT_G             0
    #define HEADLIGHT_B             0
    #define HEADLIGHT_W             255
    #define MATTHEW_NUM_QUAD_CHIPS      8
#else
    #error "BOARD_ID must be one of BOARD_FRONT/LEFT/REAR/RIGHT/CANOPY"
#endif

/* Loop / comms timing */
#define COMMAND_WATCHDOG_MS         500      /* blank LEDs if no command within  */
#define STATUS_TX_PERIOD_MS         100      /* 10 Hz status                     */
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
#define NUM_STEPS                   (32 * 4 * MATTHEW_NUM_QUAD_CHIPS)
#define LOW                         30
#define HI                          41
#define TOTAL_LEDS                  (4 * MATTHEW_NUM_QUAD_CHIPS)
#define FIRST_ACTIVE                1

/* ============================================================================
 *  ANIMATION MODE
 *    ANIM_ON  : run the full fill/sweep animations.
 *    ANIM_OFF : skip animation - LEDs are solid-on while the command is active
 *               and off otherwise.
 * ============================================================================ */
#define ANIM_OFF        0
#define ANIM_ON         1

#ifndef ANIMATION_MODE
  #define ANIMATION_MODE  1
#endif

#endif /* LIGHTING_CONFIG_H */
