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
  #define BOARD_ID      BOARD_FRONT        /* <-- change per build target */
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
    #define HEADLIGHT_W             255
    /* While a turn indicator is blinking, the headlight underneath dims to this
     * white level (0-255) so the amber sweep stands out. */
    #define HEADLIGHT_TURN_DIM_W    64
    #define MATTHEW_NUM_QUAD_CHIPS      9
    /* Lit segments per side for the split turn indicator. The front bar is
     * physically shorter than the rear, so it uses fewer segments per side to
     * keep a sensible dark centre. */
    #define TURN_SEGMENTS_PER_SIDE      6
#elif BOARD_ID == BOARD_LEFT
    #define MY_STATUS_ID            CAN_ID_STATUS_LEFT
    #define RESPONDS_TO_LEFT        1
    #define RESPONDS_TO_RIGHT       0
    #define HEADLIGHT_R             0
    #define HEADLIGHT_W             0       /* side panel - no headlight by default */
    #define MATTHEW_NUM_QUAD_CHIPS      3
#elif BOARD_ID == BOARD_REAR
    #define MY_STATUS_ID            CAN_ID_STATUS_REAR
    #define RESPONDS_TO_LEFT        1
    #define RESPONDS_TO_RIGHT       1
    #define HEADLIGHT_R             64     /* rear "headlight" = tail light, red */
    #define HEADLIGHT_W             0
    #define MATTHEW_NUM_QUAD_CHIPS      11
    /* Lit segments per side for the split turn indicator (and the matching
     * brake-bar inner edges). */
    #define TURN_SEGMENTS_PER_SIDE      7
#elif BOARD_ID == BOARD_RIGHT
    #define MY_STATUS_ID            CAN_ID_STATUS_RIGHT
    #define RESPONDS_TO_LEFT        0
    #define RESPONDS_TO_RIGHT       1
    #define HEADLIGHT_R             0
    #define HEADLIGHT_W             0
    #define MATTHEW_NUM_QUAD_CHIPS      3
#elif BOARD_ID == BOARD_CANOPY
    #define MY_STATUS_ID            CAN_ID_STATUS_CANOPY
    #define RESPONDS_TO_LEFT        1
    #define RESPONDS_TO_RIGHT       1
    #define HEADLIGHT_R             0
    #define HEADLIGHT_W             255
    #define MATTHEW_NUM_QUAD_CHIPS      8
#else
    #error "BOARD_ID must be one of BOARD_FRONT/LEFT/REAR/RIGHT/CANOPY"
#endif

/* Loop / comms timing */
#define COMMAND_WATCHDOG_MS         500      /* blank LEDs if no command within  */
#define STATUS_TX_PERIOD_MS         100      /* 10 Hz status                     */
#define MAIN_LOOP_PERIOD_MS         1

/* Treat the brake as still held for this long after the last frame that had the
 * brake bit set. Bridges brief dropouts (e.g. a sender that interleaves
 * brake-only and turn-only CAN frames) so the brake doesn't flicker into its
 * release animation on a single brake==0 frame. Keep well under
 * COMMAND_WATCHDOG_MS; larger = more glitch tolerance but more release lag. */
#define BRAKE_RELEASE_DEBOUNCE_MS   150

/* Same debounce for the headlight base layer (front overlays the turn indicator
 * on the headlight; interleaved headlight/turn frames must not blank it). */
#define HEADLIGHT_RELEASE_DEBOUNCE_MS   150

/* Keep the front headlight dimmed for this long after the turn indicator stops
 * animating, so the base layer doesn't blip back to full brightness right as the
 * amber finishes its last frame. */
#define HEADLIGHT_TURN_DIM_LINGER_MS    100

/* Treat a turn indicator as still requested for this long after the last frame
 * that had its bit set. Bridges brief dropouts from senders that interleave
 * turn and brake/headlight frames so the split indicator doesn't spuriously go
 * idle (and flash the brake/headlight base) between blinks. */
#define TURN_RELEASE_DEBOUNCE_MS        150

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
 *  ANIMATION TIMING
 *    All in milliseconds; lighting.c converts them to main-loop frames via
 *    MAIN_LOOP_PERIOD_MS, so changing MAIN_LOOP_PERIOD_MS rescales them.
 * ============================================================================ */
#define TURN_STEP_MS            20   /* per-LED/segment step during fill & empty */
#define TURN_HOLD_MS            80   /* hold time at full / empty between phases  */
#define BRAKE_STEP_MS           10   /* per-LED step for the brake-bar expansion  */

/* ============================================================================
 *  PATTERN COLOURS  (R, G, B, W channels, each 0-255)
 * ============================================================================ */
/* Full-strip sweep turn indicator (left / right / canopy) - amber. */
#define TURN_SWEEP_R            255
#define TURN_SWEEP_G            100
#define TURN_SWEEP_B            0
#define TURN_SWEEP_W            0
/* Front split turn indicator, overlaid on the headlight - amber. */
#define TURN_FRONT_R            255
#define TURN_FRONT_G            64
#define TURN_FRONT_B            0
#define TURN_FRONT_W            0
/* Rear split turn indicator - red (matches the brake so it merges cleanly). */
#define TURN_REAR_R             255
#define TURN_REAR_G             0
#define TURN_REAR_B             0
#define TURN_REAR_W             0
/* Brake bar - red. */
#define BRAKE_R                 255
#define BRAKE_G                 0
#define BRAKE_B                 0
#define BRAKE_W                 0

/* ============================================================================
 *  INDICATOR / BRAKE GEOMETRY
 * ============================================================================ */
/* Strip physically folds back on itself at its midpoint (1) or is a single
 * straight run (0). Sets how the brake / rear-indicator centre is derived. */
#define BRAKE_STRIP_FOLDED      1
/* Set to 1 if the rear bar's left/right halves come out physically reversed. */
#define REAR_TURN_SWAP_SIDES    0
/* TURN_SEGMENTS_PER_SIDE is set per board above (front/rear); other boards fall
 * back to a default in lighting.c since they use the full-strip sweep. */

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
