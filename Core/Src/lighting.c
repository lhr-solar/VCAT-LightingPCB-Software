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
volatile uint32_t        last_brake_tick = 0;
volatile uint32_t        last_headlight_tick = 0;
volatile uint32_t        last_left_tick  = 0;
volatile uint32_t        last_right_tick = 0;
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
    uint32_t c = pack_rgbw(HEADLIGHT_R, 0, 0, HEADLIGHT_W);
    for (int led = 0; led < TOTAL_LEDS; led++) {
        set_led(led, (led < FIRST_ACTIVE) ? 0 : c);
    }
}

/* Boards that use the split turn indicator (two halves growing out from the
 * centre with a dark gap), overlaid on a base layer, instead of the simple
 * full-strip sweep:
 *   - rear : turn overlaid on the brake light,
 *   - front: turn overlaid on the headlight.
 * Everything else (side panels, canopy) uses the original full-strip sweep. */
#if (BOARD_ID == BOARD_REAR) || (BOARD_ID == BOARD_FRONT)
  #define SPLIT_TURN_INDICATOR 1
#else
  #define SPLIT_TURN_INDICATOR 0
#endif

/* Boards that have a brake light. The front overlays turn on the headlight and
 * the side panels (left/right) are turn-indicator-only, so neither compiles the
 * brake engine. */
#if (BOARD_ID == BOARD_REAR) || (BOARD_ID == BOARD_CANOPY)
  #define USE_BRAKE 1
#else
  #define USE_BRAKE 0
#endif

#if !SPLIT_TURN_INDICATOR
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
    #define TURN_HOLD_FRAMES    (80 / MAIN_LOOP_PERIOD_MS)

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
#endif /* !SPLIT_TURN_INDICATOR : full-strip sweep turn indicator */

/* ============================================================================
 *  BRAKE LIGHT
 *  Red bar that grows outward to both ends while the brake is applied, holds
 *  solid for as long as commands keep arriving, then contracts back once the
 *  brake request drops or CAN commands stop. On the rear board the bar lights
 *  the exact same region as the turn indicators (see the shared indicator
 *  geometry below): it grows from two inner edges out to the ends, leaving a
 *  dark centre. Other boards grow from the exact centre to both ends. Lifecycle is driven by render_frame
 *  via a single "active" request flag, mirroring the turn indicator's
 *  in/hold/out behaviour.
 * ============================================================================ */

/* Duration of each one-LED-per-side expansion step, in main-loop frames. */
#define BRAKE_FRAMES_PER_STEP   (10 / MAIN_LOOP_PERIOD_MS)

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

/* Physical-centre slot. */
#define BRAKE_CENTER_POS   ((BRAKE_SPAN - 1) / 2)

/* ----- Shared indicator geometry --------------------------------------------
 * The rear turn indicators and the rear brake bar light the exact same region,
 * so both derive from one source of truth: TURN_SEGMENTS_PER_SIDE lit segments
 * per side, anchored at each side's far end and growing inward toward a dark
 * centre. The dark middle is whatever's left over (BRAKE_SPAN - 2*N slots), so
 * with equal sides the pattern stays centred. */
#ifndef TURN_SEGMENTS_PER_SIDE
  #if BOARD_ID == BOARD_FRONT
    /* Front bar is physically shorter (BRAKE_SPAN 16 vs the rear's 22), so its
     * indicators use fewer segments per side to keep a sensible dark centre. */
    #define TURN_SEGMENTS_PER_SIDE 6
  #else
    #define TURN_SEGMENTS_PER_SIDE 7
  #endif
#endif

/* Low half lit slots [0 .. TURN_BASE_LOW]; high half lit slots
 * [TURN_HIGH_INNER .. BRAKE_SPAN-1]. The strip's first LED (index 0, blanked by
 * FIRST_ACTIVE) sits at the low half's far end, so that half is physically one
 * segment short; the high half drops its innermost slot (TURN_HIGH_INNER) to
 * match, so both sides show the same number of lit segments. */
#define TURN_BASE_LOW    (TURN_SEGMENTS_PER_SIDE - 1)
#define TURN_BASE_HIGH   (BRAKE_SPAN - TURN_SEGMENTS_PER_SIDE)
#define TURN_HIGH_INNER  (TURN_BASE_HIGH + 1)

/* Slots each turn half animates over (kept equal so both sweep in lockstep). */
#define TURN_LOW_MAX     (TURN_SEGMENTS_PER_SIDE)
#define TURN_HIGH_MAX    (TURN_SEGMENTS_PER_SIDE)

/* Brake bar inner edges. On the rear board they match the turn indicators
 * exactly (same lit region); every other board has no rear-style indicator, so
 * the brake keeps its original grow-from-the-exact-centre-to-both-ends look. */
#if BOARD_ID == BOARD_REAR
  #define BRAKE_BASE_LOW   TURN_BASE_LOW
  #define BRAKE_BASE_HIGH  TURN_HIGH_INNER
#else
  #define BRAKE_BASE_LOW   BRAKE_CENTER_POS
  #define BRAKE_BASE_HIGH  BRAKE_CENTER_POS
#endif

/* Radius needed for the farther side to reach its end. */
#define BRAKE_MAX_RADIUS \
    (((BRAKE_SPAN - 1 - BRAKE_BASE_HIGH) > BRAKE_BASE_LOW) \
        ? (BRAKE_SPAN - 1 - BRAKE_BASE_HIGH) \
        : BRAKE_BASE_LOW)

/* Only boards with a brake light (rear, canopy) compile the brake engine. */
#if USE_BRAKE
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

    /* Animations off: solid red while applied (centre gap stays dark), off the
     * instant it drops. */
    if (ANIMATION_MODE == ANIM_OFF) {
        if (!active) { brake_state = BR_IDLE; return 0; }
        for (int led = 0; led < TOTAL_LEDS; led++) {
            int p = brake_phys_pos(led);
            int lit = (led >= FIRST_ACTIVE) &&
                      (p <= BRAKE_BASE_LOW || p >= BRAKE_BASE_HIGH);
            set_led(led, lit ? color : 0);
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

    int lo = BRAKE_BASE_LOW  - brake_radius;   /* low front grows down toward slot 0   */
    int hi = BRAKE_BASE_HIGH + brake_radius;   /* high front grows up toward the far end */

    for (int led = 0; led < TOTAL_LEDS; led++) {
        uint32_t led_color = 0;
        int p = brake_phys_pos(led);
        if (led < FIRST_ACTIVE) {
            led_color = 0;
        } else if (brake_state == BR_HOLD) {
            /* Solid, but the centre gap stays dark. */
            if (p <= BRAKE_BASE_LOW || p >= BRAKE_BASE_HIGH) led_color = color;
        } else {
            /* Filling / emptying: two fronts expanding outward from the gap. */
            if (p <= BRAKE_BASE_LOW  && p >= lo) led_color = color;
            if (p >= BRAKE_BASE_HIGH && p <= hi) led_color = color;
        }
        set_led(led, led_color);
    }
    return 1;
}
#endif /* USE_BRAKE : brake engine */

#if SPLIT_TURN_INDICATOR
/* ============================================================================
 *  SPLIT TURN INDICATOR (rear + front)
 *  The indicators fill amber from the physical centre outward, like the brake
 *  bar, but each side only lights its own half and keeps
 *  looping the blink animation (fill out -> hold -> empty out -> hold) while
 *  active, finishing the current cycle out when commands stop: 
 *    - left  indicator  -> the "low"  half (centre -> physical slot 0),
 *    - right indicator  -> the "high" half (centre -> far end),
 *    - both (hazards)   -> both halves.
 *  Each side lights TURN_SEGMENTS_PER_SIDE segments anchored at its far end;
 *  the slots left over in the middle stay dark.
 *  Reuses the brake's folded-strip mapping (brake_phys_pos / BRAKE_CENTER_POS).
 * ============================================================================ */

/* If the rear bar's left/right halves come out swapped, set this to 1. */
#ifndef REAR_TURN_SWAP_SIDES
#define REAR_TURN_SWAP_SIDES 0
#endif

/* Turn geometry (TURN_SEGMENTS_PER_SIDE, TURN_BASE_LOW/HIGH, TURN_HIGH_INNER,
 * TURN_LOW_MAX/HIGH_MAX) is defined once in the shared indicator-geometry block
 * above so the brake bar and these indicators stay the exact same size. */

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
static int turn_side_advance(turn_side_t *s, int active, int span, int end_full) {
    #define TURN_FRAMES_PER_STEP (20 / MAIN_LOOP_PERIOD_MS)
    #define TURN_HOLD_FRAMES_R   (80 / MAIN_LOOP_PERIOD_MS)
    s->frame++;
    switch (s->state) {
        case TR_IDLE:
            if (active) {
                if (end_full) {
                    /* Braking: the region is already fully red (brake base), so
                     * start in the closing phase from full - blink by emptying
                     * out first instead of filling from black (no start blip). */
                    s->state = TR_EMPTYING; s->fill_pos = span; s->empty_pos = 0; s->frame = 0;
                } else {
                    s->state = TR_FILLING;  s->fill_pos = 0;    s->empty_pos = 0; s->frame = 0;
                }
            } else {
                return 0;
            }
            break;
        case TR_FILLING:
            /* Always finish filling, even after the request drops, so the last
             * blink ends fully lit. */
            if (s->frame >= TURN_FRAMES_PER_STEP) {
                s->frame = 0;
                if (++s->fill_pos >= span) { s->fill_pos = span; s->state = TR_HOLD_FULL; }
            }
            break;
        case TR_HOLD_FULL:
            /* end_full (braking): once the request is gone, stop here fully lit
             * and release the region so the identical red brake base shows
             * through - no fade-out, no blip. */
            if (!active && end_full) { s->state = TR_IDLE; return 0; }
            if (s->frame >= TURN_HOLD_FRAMES_R) { s->frame = 0; s->empty_pos = 0; s->state = TR_EMPTYING; }
            break;
        case TR_EMPTYING:
            /* Caught mid-empty with the request gone while braking: reverse and
             * fill back up so we still end lit. */
            if (!active && end_full) { s->state = TR_FILLING; s->fill_pos = 0; s->frame = 0; break; }
            if (s->frame >= TURN_FRAMES_PER_STEP) {
                s->frame = 0;
                if (++s->empty_pos >= span) { s->empty_pos = span; s->state = TR_HOLD_EMPTY; }
            }
            break;
        case TR_HOLD_EMPTY:
            /* Request gone during the dark part while braking: fill back on
             * immediately instead of waiting out the hold or going idle. */
            if (!active && end_full) {
                s->frame = 0; s->fill_pos = 0; s->empty_pos = 0; s->state = TR_FILLING;
            } else if (s->frame >= TURN_HOLD_FRAMES_R) {
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
 * @brief Overlay the rear turn indicators (amber) on top of whatever is already
 *        in the frame (e.g. the brake layer). Only paints its lit slots, so the
 *        underlying pattern shows through everywhere else and between blinks.
 *        Returns 1 if either half owns the frame this tick, 0 once both halves
 *        are fully idle.
 */
static int turn_render(int left_active, int right_active, int end_full) {
#if BOARD_ID == BOARD_REAR
    /* Rear turn is red (not amber): it "owns" its whole side region and blinks
     * red<->black there, blanking the brake underneath so you get a clean red
     * blink with black gaps instead of amber-over-red. */
    const uint32_t color = pack_rgbw(255, 0, 0, 0);
#else
    /* Front: amber overlay painted on lit slots only (headlight shows between). */
    const uint32_t color = pack_rgbw(255, 64, 0, 0);
#endif

    int low_active  = REAR_TURN_SWAP_SIDES ? right_active : left_active;
    int high_active = REAR_TURN_SWAP_SIDES ? left_active  : right_active;

    /* Animations off: light the requested half/halves solid (centre stays
     * dark), nothing otherwise. */
    if (ANIMATION_MODE == ANIM_OFF) {
        if (!low_active && !high_active) { turn_low.state = TR_IDLE; turn_high.state = TR_IDLE; return 0; }
        for (int led = 0; led < TOTAL_LEDS; led++) {
            if (led < FIRST_ACTIVE) continue;
            int p = brake_phys_pos(led);
            int lit = (low_active  && p <= TURN_BASE_LOW) ||
                      (high_active && p >= TURN_HIGH_INNER);
            if (lit) set_led(led, color);   /* overlay: paint lit slots only */
        }
        return 1;
    }

    int owns_lo = turn_side_advance(&turn_low,  low_active,  TURN_LOW_MAX,  end_full);
    int owns_hi = turn_side_advance(&turn_high, high_active, TURN_HIGH_MAX, end_full);
    if (!owns_lo && !owns_hi) return 0;

    for (int led = 0; led < TOTAL_LEDS; led++) {
        if (led < FIRST_ACTIVE) continue;
        int p = brake_phys_pos(led);
#if BOARD_ID == BOARD_REAR
        /* While a side is animating it takes over its whole region: red where
         * the sweep is lit, black otherwise (overrides the brake base). Idle
         * sides are left untouched so the brake red still shows there. */
        if      (owns_lo && p <= TURN_BASE_LOW)
            set_led(led, turn_side_lit(&turn_low,  TURN_BASE_LOW - p) ? color : 0);
        else if (owns_hi && p >= TURN_HIGH_INNER)
            set_led(led, turn_side_lit(&turn_high, p - TURN_BASE_HIGH) ? color : 0);
#else
        int lit = (owns_lo && p <= TURN_BASE_LOW  && turn_side_lit(&turn_low,  TURN_BASE_LOW - p)) ||
                  (owns_hi && p >= TURN_HIGH_INNER && turn_side_lit(&turn_high, p - TURN_BASE_HIGH));
        if (lit) set_led(led, color);   /* overlay on top of the base layer */
#endif
    }
    return 1;
}

#if BOARD_ID == BOARD_FRONT
/**
 * @brief Front headlight base layer. Regs don't allow the middle section lit,
 *        so instead of the full strip this lights only the two end segments -
 *        the exact same slots the turn indicators occupy - leaving the centre
 *        gap dark, whether or not an indicator is active.
 *
 *        `turn_active` dims the white channel (to HEADLIGHT_TURN_DIM_W) while a
 *        turn indicator is blinking so the amber overlay stands out.
 */
static void pattern_headlight_front(int turn_active) {
    uint8_t w = turn_active ? HEADLIGHT_TURN_DIM_W : HEADLIGHT_W;
    const uint32_t color = pack_rgbw(HEADLIGHT_R, 0, 0, w);
    for (int led = 0; led < TOTAL_LEDS; led++) {
        int p = brake_phys_pos(led);
        int lit = (led >= FIRST_ACTIVE) &&
                  (p <= TURN_BASE_LOW || p >= TURN_HIGH_INNER);
        set_led(led, lit ? color : 0);
    }
}
#endif
#endif /* SPLIT_TURN_INDICATOR */

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

    /* The BPS strobe is a separate external light - just drive its GPIO high
     * while commanded. It is independent of the RGB strip, so the strobe and the
     * turn/hazard/brake patterns can all be active at the same time. */
    HAL_GPIO_WritePin(BPS_STROBE_GPIO_Port, BPS_STROBE_Pin,
                      (!watchdog && cmd.bps_strobe) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* A request is only "held" while commands are actually arriving. When they
     * stop (watchdog) the request drops, which lets each animated effect play
     * its out-animation to completion rather than snapping off. */
#if USE_BRAKE
    int brake_held = (last_brake_tick != 0) &&
                     ((HAL_GetTick() - last_brake_tick) <= BRAKE_RELEASE_DEBOUNCE_MS);
    int brake_req  = (!watchdog) && (cmd.brake || brake_held);
#elif BOARD_ID == BOARD_FRONT
    /* Headlight base layer is debounced like the brake so interleaved
     * headlight/turn frames don't blank it for a tick. */
    int headlight_held = (last_headlight_tick != 0) &&
                         ((HAL_GetTick() - last_headlight_tick) <= HEADLIGHT_RELEASE_DEBOUNCE_MS);
    int headlight_req  = (!watchdog) && (cmd.headlights || headlight_held);
#endif
#if SPLIT_TURN_INDICATOR
    /* Debounce the raw bits so a single interleaved turn==0 frame doesn't drop
     * the request and let the state machine idle between blinks. */
    int left_held  = (last_left_tick  != 0) &&
                     ((HAL_GetTick() - last_left_tick)  <= TURN_RELEASE_DEBOUNCE_MS);
    int right_held = (last_right_tick != 0) &&
                     ((HAL_GetTick() - last_right_tick) <= TURN_RELEASE_DEBOUNCE_MS);
    int left_active  = (!watchdog) && RESPONDS_TO_LEFT  && (cmd.left_indicator  || left_held);
    int right_active = (!watchdog) && RESPONDS_TO_RIGHT && (cmd.right_indicator || right_held);
#else
    int turn_req  = (!watchdog) &&
        ((RESPONDS_TO_LEFT  && cmd.left_indicator) ||
         (RESPONDS_TO_RIGHT && cmd.right_indicator));
#endif

    /* Priority / layering: strobe > headlight > custom > off for steady states.
     * brake_step()/turn handlers are advanced every tick so their out-animations
     * keep running even after the request drops. */
#if BOARD_ID == BOARD_REAR
    /* Rear: brake (red) is the base layer and the turn indicators (amber)
     * overlay on top, so braking and turning can show simultaneously - the turn
     * blinks amber over its segments while the red brake shows between blinks
     * and on the non-turning side. Both state machines advance every tick.
     * brake_step() paints the whole strip when it owns the frame; if it doesn't,
     * clear to black first so the turn overlay sits on nothing. */
    int brake_owns = brake_step(brake_req);
    if (!brake_owns) pattern_off();
    /* While braking, end the turn's final blink fully lit so it merges into the
     * red brake base instead of fading out and blipping back to red. */
    int turn_owns  = turn_render(left_active, right_active, brake_req);
    if (brake_owns || turn_owns) { commit_frame(255); return; }
#elif BOARD_ID == BOARD_FRONT
    /* Front: the steady pattern (headlight / strobe / custom) is the base layer
     * and the turn indicators (amber) overlay on top, so headlights and turn
     * signals show simultaneously - the turn blinks amber over its segments
     * while the headlight shows between blinks and on the non-turning side. */
    /* Dim the headlight while the turn indicator is actually animating. Keyed
     * off the split-turn state machines (non-idle) rather than the raw command
     * bits, so it stays stable for the whole blink lifecycle and doesn't flip
     * dim<->full when the sender interleaves headlight-only and headlight+turn
     * frames. In ANIM_OFF there's no state machine, so fall back to the request. */
    int turn_running = (turn_low.state != TR_IDLE) || (turn_high.state != TR_IDLE) ||
                       ((ANIMATION_MODE == ANIM_OFF) && (left_active || right_active));
    /* Linger the dim briefly after the animation stops so the base layer doesn't
     * blip back to full right as the amber clears its last frame. */
    static uint32_t last_turn_dim_tick = 0;
    if (turn_running) last_turn_dim_tick = HAL_GetTick();
    int turn_dim = turn_running ||
                   ((last_turn_dim_tick != 0) &&
                    ((HAL_GetTick() - last_turn_dim_tick) <= HEADLIGHT_TURN_DIM_LINGER_MS));
    if (!watchdog) {
        if      (headlight_req)         pattern_headlight_front(turn_dim);
        else if (cmd.custom_mode != 0)  pattern_custom_mode(cmd.custom_mode);
        else                            pattern_off();
    } else {
        pattern_off();
    }
    turn_render(left_active, right_active, 0);   /* overlay amber on top (normal empty-out) */
    commit_frame(255);
    return;
#elif (BOARD_ID == BOARD_LEFT) || (BOARD_ID == BOARD_RIGHT)
    /* Side panels: turn indicator only on the strip. The BPS strobe is a
     * separate external light driven by its own GPIO above, not the strip.
     * turn_step() paints the whole strip while active and follows its
     * out-animation; when idle it draws nothing, so blank the strip. */
    if (!turn_step(turn_req)) pattern_off();
    commit_frame(255);
    return;
#else
    /* Canopy: simple priority, brake > turn, whichever owns wins. */
    if (brake_step(brake_req)) { commit_frame(255); return; }
    if (turn_step(turn_req))   { commit_frame(255); return; }
#endif

    /* Steady patterns have no out-animation: shown only while receiving. */
    if (!watchdog) {
        if      (cmd.headlights)        pattern_headlight();
        else if (cmd.custom_mode != 0)  pattern_custom_mode(cmd.custom_mode);
        else                            pattern_off();
    } else {
        pattern_off();
    }
    commit_frame(255);
}
