/**
 ******************************************************************************
 * @file    lighting.c
 * @brief   LED pattern engine - helpers, patterns and the per-frame renderer.
 *
 * Parses nothing itself: it reads the shared `cmd` state (filled by can_app.c)
 * and paints led_pattern accordingly. All geometry derives from
 * MATTHEW_NUM_QUAD_CHIPS so it scales with the number of quad chips.
 ******************************************************************************
 */
#include "lighting.h"

/* ============================================================================
 *  Shared application state
 * ============================================================================ */
volatile LightingCommand cmd           = {0};
volatile uint32_t        last_cmd_tick = 0;
volatile uint8_t         board_fault   = FAULT_OK;

/* WS2814 DMA buffer (one PWM duty value per bit). */
uint32_t led_pattern[NUM_STEPS];

/* Logical colour for each LED this frame. Pattern functions write here via
 * set_led(); commit_frame() then pushes the frame to led_pattern. */
static uint32_t frame_colors[TOTAL_LEDS];

/* ============================================================================
 *  LED PATTERN HELPERS
 * ============================================================================ */

/**
 * @brief Write a packed RGBW colour into the DMA buffer for a given LED index.
 *        Bit layout: bits 31-24 = W, 23-16 = R, 15-8 = G, 7-0 = B.
 */
static inline void write_led(int led, uint32_t color) {
    uint32_t bit_index = 0;
    for (int i = 31; i >= 0; i--) {
        uint32_t bit = color & (1u << i);
        led_pattern[bit_index + (led * 32)] = (bit == 0) ? LOW : HI;
        bit_index++;
    }
}

static inline uint32_t pack_rgbw(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    return ((uint32_t)w << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

/**
 * @brief Store a packed RGBW colour for an LED in the logical frame buffer.
 *        Pattern functions use this instead of writing led_pattern directly so
 *        the frame can be brightness-scaled before being committed.
 */
static inline void set_led(int led, uint32_t color) {
    if (led >= 0 && led < TOTAL_LEDS) frame_colors[led] = color;
}

/**
 * @brief Push the logical frame buffer into the DMA bit-pattern, scaling every
 *        channel by `brightness` (0-255). brightness == 255 is unscaled.
 */
static void commit_frame(uint8_t brightness) {
    for (int led = 0; led < TOTAL_LEDS; led++) {
        uint32_t c = frame_colors[led];
        if (brightness != 255) {
            uint8_t w = (uint8_t)(((c >> 24) & 0xFF) * brightness / 255);
            uint8_t r = (uint8_t)(((c >> 16) & 0xFF) * brightness / 255);
            uint8_t g = (uint8_t)(((c >>  8) & 0xFF) * brightness / 255);
            uint8_t b = (uint8_t)(((c >>  0) & 0xFF) * brightness / 255);
            c = pack_rgbw(r, g, b, w);
        }
        write_led(led, c);
    }
}

/* ============================================================================
 *  PATTERN FUNCTIONS
 *  Each fills frame_colors for one frame, called once per main-loop tick.
 * ============================================================================ */

/**
 * @brief All LEDs off (with first segment skipped as always).
 */
static void pattern_off(void) {
    for (int led = 0; led < TOTAL_LEDS; led++) set_led(led, 0);
}

/**
 * @brief Solid headlight colour for this board.
 */
static void pattern_headlight(void) {
    uint32_t c = pack_rgbw(HEADLIGHT_R, HEADLIGHT_G, HEADLIGHT_B, HEADLIGHT_W);
    for (int led = 0; led < TOTAL_LEDS; led++) {
        set_led(led, (led < FIRST_ACTIVE) ? 0 : c);
    }
}

#if BOARD_ID != BOARD_REAR
/**
 * @brief Turn indicator (amber). Lifecycle driven by render_frame via a single
 *        "active" request flag:
 *          - while active: loop fill in -> hold -> empty out -> hold (blink),
 *          - when the request drops (commands stop): finish the current sweep
 *            out to empty, then go idle. This "follows the animation out"
 *            instead of snapping off mid-blink.
 *        Starting from idle always begins a fresh fill-in.
 *
 *        Returns 1 if the indicator owns the frame this tick (something was
 *        drawn), 0 once it has gone fully idle.
 */
enum { TN_IDLE, TN_FILLING, TN_HOLD_FULL, TN_EMPTYING, TN_HOLD_EMPTY };

static int turn_state     = TN_IDLE;
static int turn_fill_pos  = 0;
static int turn_empty_pos = 0;
static int turn_frame      = 0;

static int turn_step(int active) {
    #define ACTIVE_LEDS         (TOTAL_LEDS - FIRST_ACTIVE)
    #define TURN_FRAMES_PER_LED (20 / MAIN_LOOP_PERIOD_MS)
    #define TURN_HOLD_FRAMES    (60 / MAIN_LOOP_PERIOD_MS)

    /* Amber: R=255, G=100, B=0, W=0 */
    const uint32_t color = pack_rgbw(255, 100, 0, 0);

    /* Animations off: solid on while requested, off the instant it drops. */
    if (ANIMATION_MODE == ANIM_OFF) {
        if (!active) { turn_state = TN_IDLE; return 0; }
        for (int led = 0; led < TOTAL_LEDS; led++) {
            set_led(led, (led < FIRST_ACTIVE) ? 0 : color);
        }
        return 1;
    }

    turn_frame++;
    switch (turn_state) {
        case TN_IDLE:
            if (active) {
                turn_state = TN_FILLING;
                turn_fill_pos = 0; turn_empty_pos = 0; turn_frame = 0;
            } else {
                return 0;
            }
            break;
        case TN_FILLING:
            if (turn_frame >= TURN_FRAMES_PER_LED) {
                turn_frame = 0;
                if (++turn_fill_pos >= ACTIVE_LEDS) { turn_fill_pos = ACTIVE_LEDS; turn_state = TN_HOLD_FULL; }
            }
            break;
        case TN_HOLD_FULL:
            if (turn_frame >= TURN_HOLD_FRAMES) { turn_frame = 0; turn_empty_pos = 0; turn_state = TN_EMPTYING; }
            break;
        case TN_EMPTYING:
            if (turn_frame >= TURN_FRAMES_PER_LED) {
                turn_frame = 0;
                if (++turn_empty_pos >= ACTIVE_LEDS) { turn_empty_pos = ACTIVE_LEDS; turn_state = TN_HOLD_EMPTY; }
            }
            break;
        case TN_HOLD_EMPTY:
            if (turn_frame >= TURN_HOLD_FRAMES) {
                turn_frame = 0;
                if (active) {
                    /* Still requested: start another blink cycle. */
                    turn_fill_pos = 0; turn_empty_pos = 0; turn_state = TN_FILLING;
                } else {
                    /* Request gone and we've finished emptying: stop. */
                    turn_state = TN_IDLE;
                    return 0;
                }
            }
            break;
    }

    for (int led = 0; led < TOTAL_LEDS; led++) {
        uint32_t led_color = 0;
        int active_idx = led - FIRST_ACTIVE;
        if      (led < FIRST_ACTIVE)         led_color = 0;
        else if (turn_state == TN_FILLING)   led_color = (active_idx < turn_fill_pos)  ? color : 0;
        else if (turn_state == TN_HOLD_FULL) led_color = color;
        else if (turn_state == TN_EMPTYING)  led_color = (active_idx < turn_empty_pos) ? 0 : color;
        else                                 led_color = 0;   /* TN_HOLD_EMPTY */
        set_led(led, led_color);
    }
    return 1;
}
#endif /* BOARD_ID != BOARD_REAR : sweep turn indicator */

/* ============================================================================
 *  BRAKE LIGHT
 *  Red bar that grows outward from the centre of the strip to both ends while
 *  the brake is applied, holds solid for as long as commands keep arriving, then
 *  contracts back to the centre (out-fill) once the brake request drops or CAN
 *  commands stop. Lifecycle is driven by render_frame via a single "active"
 *  request flag, mirroring the turn indicator's in/hold/out behaviour.
 * ============================================================================ */

/* Duration of each one-LED-per-side expansion step, in main-loop frames. */
#define BRAKE_FRAMES_PER_STEP   (20 / MAIN_LOOP_PERIOD_MS)

/* The brake bar grows from the PHYSICAL centre of the strip out to both ends.
 * Many light bars are a single strip folded back on itself (so the data/return
 * wire ends up at the same side), which means array index TOTAL_LEDS/2 actually
 * sits at the far physical end, not the middle. brake_phys_pos() maps an array
 * index to its physical slot so the expansion always looks centred.
 *   BRAKE_STRIP_FOLDED = 1 : strip doubles back at its midpoint (default)
 *   BRAKE_STRIP_FOLDED = 0 : single straight linear run
 */
#ifndef BRAKE_STRIP_FOLDED
#define BRAKE_STRIP_FOLDED 1
#endif

#if BRAKE_STRIP_FOLDED
  /* Outbound run [0 .. N/2-1] then return run folds back [N/2 .. N-1]; both
   * runs share the same physical slots 0 .. N/2-1. */
  #define BRAKE_SPAN   (TOTAL_LEDS / 2)
  static inline int brake_phys_pos(int led) {
      return (led < (TOTAL_LEDS / 2)) ? led : (TOTAL_LEDS - 1 - led);
  }
#else
  #define BRAKE_SPAN   TOTAL_LEDS
  static inline int brake_phys_pos(int led) { return led; }
#endif

/* Physical-centre slot and the radius needed to reach the farther end. */
#define BRAKE_CENTER_POS   ((BRAKE_SPAN - 1) / 2)
#define BRAKE_MAX_RADIUS \
    (((BRAKE_SPAN - 1 - BRAKE_CENTER_POS) > BRAKE_CENTER_POS) \
        ? (BRAKE_SPAN - 1 - BRAKE_CENTER_POS) \
        : BRAKE_CENTER_POS)

enum { BR_IDLE, BR_FILLING, BR_HOLD, BR_EMPTYING };

static int brake_state  = BR_IDLE;
static int brake_radius = 0;   /* current half-width of the lit bar */
static int brake_frame  = 0;   /* frame counter within the current step */

/**
 * @brief Advance/draw one brake frame.
 *          - while active: expand red from the centre out, then hold solid,
 *          - when the request drops: contract back to the centre, then idle.
 *        Returns 1 if the brake owns the frame this tick, 0 once fully idle.
 */
static int brake_step(int active) {
    const uint32_t color = pack_rgbw(255, 0, 0, 0);

    /* Animations off: solid red while applied, off the instant it drops. */
    if (ANIMATION_MODE == ANIM_OFF) {
        if (!active) { brake_state = BR_IDLE; return 0; }
        for (int led = 0; led < TOTAL_LEDS; led++) {
            set_led(led, (led < FIRST_ACTIVE) ? 0 : color);
        }
        return 1;
    }

    brake_frame++;
    switch (brake_state) {
        case BR_IDLE:
            if (active) {
                brake_state = BR_FILLING;
                brake_radius = 0; brake_frame = 0;
            } else {
                return 0;
            }
            break;
        case BR_FILLING:
            if (!active) {
                brake_state = BR_EMPTYING; brake_frame = 0;   /* released mid-fill: head back out */
            } else if (brake_frame >= BRAKE_FRAMES_PER_STEP) {
                brake_frame = 0;
                if (++brake_radius >= BRAKE_MAX_RADIUS) { brake_radius = BRAKE_MAX_RADIUS; brake_state = BR_HOLD; }
            }
            break;
        case BR_HOLD:
            if (!active) { brake_state = BR_EMPTYING; brake_frame = 0; }
            break;
        case BR_EMPTYING:
            if (active) {
                brake_state = BR_FILLING; brake_frame = 0;    /* re-applied: fill back in */
            } else if (brake_frame >= BRAKE_FRAMES_PER_STEP) {
                brake_frame = 0;
                if (--brake_radius <= 0) { brake_radius = 0; brake_state = BR_IDLE; return 0; }
            }
            break;
    }

    int lo = BRAKE_CENTER_POS - brake_radius;
    int hi = BRAKE_CENTER_POS + brake_radius;

    for (int led = 0; led < TOTAL_LEDS; led++) {
        uint32_t led_color;
        int p = brake_phys_pos(led);
        if      (led < FIRST_ACTIVE)     led_color = 0;
        else if (brake_state == BR_HOLD) led_color = color;
        else if (p >= lo && p <= hi)     led_color = color;   /* filling / emptying */
        else                             led_color = 0;
        set_led(led, led_color);
    }
    return 1;
}

#if BOARD_ID == BOARD_REAR
/* ============================================================================
 *  REAR TURN INDICATOR
 *  On the rear bar the indicators fill amber from the physical centre outward,
 *  like the brake light, but each side only lights its own half and keeps
 *  looping the blink animation (fill out -> hold -> empty out -> hold) while
 *  active, finishing the current cycle out when commands stop:
 *    - left  indicator  -> the "low"  half (centre -> physical slot 0),
 *    - right indicator  -> the "high" half (centre -> far end),
 *    - both (hazards)   -> both halves.
 *  A configurable gap (TURN_CENTER_GAP) is left dark around the exact centre.
 *  Reuses the brake's folded-strip mapping (brake_phys_pos / BRAKE_CENTER_POS).
 * ============================================================================ */

/* If the rear bar's left/right halves come out swapped, set this to 1. */
#ifndef REAR_TURN_SWAP_SIDES
#define REAR_TURN_SWAP_SIDES 0
#endif

/* Empty gap (in physical slots) left dark on each side of the exact centre, so
 * the two halves are visually spaced apart instead of meeting in the middle. */
#ifndef TURN_CENTER_GAP
#define TURN_CENTER_GAP 2
#endif

/* Innermost lit slot of each half (just outside the centre gap). */
#define TURN_BASE_LOW   (BRAKE_CENTER_POS - TURN_CENTER_GAP)
#define TURN_BASE_HIGH  (BRAKE_CENTER_POS + TURN_CENTER_GAP)

/* Number of slots each half spans from its inner base out to its far end. */
#define TURN_LOW_MAX    (TURN_BASE_LOW + 1)
#define TURN_HIGH_MAX   (BRAKE_SPAN - TURN_BASE_HIGH)

enum { TR_IDLE, TR_FILLING, TR_HOLD_FULL, TR_EMPTYING, TR_HOLD_EMPTY };

typedef struct { int state; int fill_pos; int empty_pos; int frame; } turn_side_t;

static turn_side_t turn_low  = { TR_IDLE, 0, 0, 0 };
static turn_side_t turn_high = { TR_IDLE, 0, 0, 0 };

/**
 * @brief Advance one half's blink lifecycle: fill out -> hold -> empty out ->
 *        hold -> (loop while active). When the request drops it finishes the
 *        current cycle out to empty, then goes idle. Returns 1 while non-idle.
 *        `span` is the number of slots this half covers from its inner base.
 */
static int turn_side_advance(turn_side_t *s, int active, int span) {
    #define TURN_FRAMES_PER_STEP (40 / MAIN_LOOP_PERIOD_MS)
    #define TURN_HOLD_FRAMES_R   (80 / MAIN_LOOP_PERIOD_MS)
    s->frame++;
    switch (s->state) {
        case TR_IDLE:
            if (active) { s->state = TR_FILLING; s->fill_pos = 0; s->empty_pos = 0; s->frame = 0; }
            else        return 0;
            break;
        case TR_FILLING:
            if (s->frame >= TURN_FRAMES_PER_STEP) {
                s->frame = 0;
                if (++s->fill_pos >= span) { s->fill_pos = span; s->state = TR_HOLD_FULL; }
            }
            break;
        case TR_HOLD_FULL:
            if (s->frame >= TURN_HOLD_FRAMES_R) { s->frame = 0; s->empty_pos = 0; s->state = TR_EMPTYING; }
            break;
        case TR_EMPTYING:
            if (s->frame >= TURN_FRAMES_PER_STEP) {
                s->frame = 0;
                if (++s->empty_pos >= span) { s->empty_pos = span; s->state = TR_HOLD_EMPTY; }
            }
            break;
        case TR_HOLD_EMPTY:
            if (s->frame >= TURN_HOLD_FRAMES_R) {
                s->frame = 0;
                if (active) { s->fill_pos = 0; s->empty_pos = 0; s->state = TR_FILLING; }
                else        { s->state = TR_IDLE; return 0; }
            }
            break;
    }
    return 1;
}

/**
 * @brief Is the slot at distance `d` from the half's inner base lit this frame?
 *        Mirrors the sweep indicator: fill grows from the centre out, empty
 *        recedes from the centre out.
 */
static int turn_side_lit(const turn_side_t *s, int d) {
    switch (s->state) {
        case TR_FILLING:   return d < s->fill_pos;
        case TR_HOLD_FULL: return 1;
        case TR_EMPTYING:  return d >= s->empty_pos;
        default:           return 0;   /* TR_HOLD_EMPTY / TR_IDLE */
    }
}

/**
 * @brief Render the rear turn indicators. Returns 1 if either half owns the
 *        frame this tick, 0 once both halves are fully idle.
 */
static int turn_render(int left_active, int right_active) {
    const uint32_t color = pack_rgbw(255, 100, 0, 0);

    int low_active  = REAR_TURN_SWAP_SIDES ? right_active : left_active;
    int high_active = REAR_TURN_SWAP_SIDES ? left_active  : right_active;

    /* Animations off: light the requested half/halves solid (with the centre
     * gap still dark), off otherwise. */
    if (ANIMATION_MODE == ANIM_OFF) {
        if (!low_active && !high_active) { turn_low.state = TR_IDLE; turn_high.state = TR_IDLE; return 0; }
        for (int led = 0; led < TOTAL_LEDS; led++) {
            if (led < FIRST_ACTIVE) { set_led(led, 0); continue; }
            int p = brake_phys_pos(led);
            int lit = (low_active  && p <= TURN_BASE_LOW) ||
                      (high_active && p >= TURN_BASE_HIGH);
            set_led(led, lit ? color : 0);
        }
        return 1;
    }

    int owns_lo = turn_side_advance(&turn_low,  low_active,  TURN_LOW_MAX);
    int owns_hi = turn_side_advance(&turn_high, high_active, TURN_HIGH_MAX);
    if (!owns_lo && !owns_hi) return 0;

    for (int led = 0; led < TOTAL_LEDS; led++) {
        uint32_t c = 0;
        if (led >= FIRST_ACTIVE) {
            int p = brake_phys_pos(led);
            if (owns_lo && p <= TURN_BASE_LOW  && turn_side_lit(&turn_low,  TURN_BASE_LOW - p)) c = color;
            if (owns_hi && p >= TURN_BASE_HIGH && turn_side_lit(&turn_high, p - TURN_BASE_HIGH)) c = color;
        }
        set_led(led, c);
    }
    return 1;
}
#endif /* BOARD_ID == BOARD_REAR */

/**
 * @brief BPS strobe at 90 pulses/min (1.5 Hz), short white flash.
 */
static void pattern_bps_strobe(void) {
    #define STROBE_PERIOD_FRAMES    (667 / MAIN_LOOP_PERIOD_MS)
    #define STROBE_ON_FRAMES        (60  / MAIN_LOOP_PERIOD_MS)

    static int frame_count = 0;
    const uint32_t color = pack_rgbw(0, 0, 0, 255);

    if (++frame_count >= STROBE_PERIOD_FRAMES) frame_count = 0;
    int on = (frame_count < STROBE_ON_FRAMES);

    for (int led = 0; led < TOTAL_LEDS; led++) {
        set_led(led, (led < FIRST_ACTIVE) ? 0 : (on ? color : 0));
    }
}

/**
 * @brief Custom modes (1=rgb rainbow, 2=burnt orange, 3=palette fade, 0=off).
 *        TODO: port the existing matthews_pattner / smooth_palette logic here.
 *        For now just shows headlight colour for any non-zero mode.
 */
static void pattern_custom_mode(uint8_t mode) {
    if (mode == 0) { pattern_off(); return; }
    /* TODO: integrate smooth_rainbow_int / smooth_palette / burnt orange */
    pattern_headlight();
}

/* ============================================================================
 *  PRIORITY DISPATCH
 *  Decides which pattern to draw based on the current command flags and which
 *  side this board is on.
 * ============================================================================ */
void render_frame(void) {
    int watchdog = (HAL_GetTick() - last_cmd_tick) > COMMAND_WATCHDOG_MS;
    board_fault  = watchdog ? FAULT_LIGHT_CMD_WATCHDOG : FAULT_OK;

    /* A request is only "held" while commands are actually arriving. When they
     * stop (watchdog) the request drops, which lets each animated effect play
     * its out-animation to completion rather than snapping off. */
    int brake_req = (!watchdog) && cmd.brake;
#if BOARD_ID == BOARD_REAR
    int left_active  = (!watchdog) && RESPONDS_TO_LEFT  && cmd.left_indicator;
    int right_active = (!watchdog) && RESPONDS_TO_RIGHT && cmd.right_indicator;
#else
    int turn_req  = (!watchdog) &&
        ((RESPONDS_TO_LEFT  && cmd.left_indicator) ||
         (RESPONDS_TO_RIGHT && cmd.right_indicator));
#endif

    /* Priority: brake > turn > strobe > headlight > custom > off.
     * brake_step()/turn handlers are advanced every tick so their out-animations
     * keep running even after the request drops; whichever owns the strip
     * (returns non-zero) wins, highest priority first. */
    if (brake_step(brake_req)) { commit_frame(255); return; }
#if BOARD_ID == BOARD_REAR
    if (turn_render(left_active, right_active)) { commit_frame(255); return; }
#else
    if (turn_step(turn_req))   { commit_frame(255); return; }
#endif

    /* Steady patterns have no out-animation: shown only while receiving. */
    if (!watchdog) {
        if      (cmd.bps_strobe)        pattern_bps_strobe();
        else if (cmd.headlights)        pattern_headlight();
        else if (cmd.custom_mode != 0)  pattern_custom_mode(cmd.custom_mode);
        else                            pattern_off();
    } else {
        pattern_off();
    }
    commit_frame(255);
}
