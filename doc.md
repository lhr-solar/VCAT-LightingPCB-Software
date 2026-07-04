# VCAT Lighting PCB – Firmware Guide

## Overview

This firmware runs on an **STM32L431CBTx** and drives WS2812/WS2814 LED strips across five lighting zones of a solar car. Each zone gets its own flashed binary — the same source compiles five different boards by changing one `#define`. A central Controls ECU broadcasts a single 1-byte CAN command frame; each board listens, parses it, and renders the appropriate pattern independently.

---

## Project Structure

```
Core/
  Inc/
    lighting_config.h   ← ALL tunable constants live here (start here)
    lighting.h          ← LightingCommand struct, extern state, render_frame()
    can_app.h           ← CAN init/TX prototypes
    bsp.h               ← Peripheral handle externs, bsp_init/strip_show
    main.h              ← MCU includes, BPS strobe GPIO defines
    stm32l4xx_it.h      ← IRQ handler declarations
  Src/
    main.c              ← Entry point, main loop
    lighting.c          ← Pattern engine (all LED logic lives here)
    can_app.c           ← CAN RX parsing, CAN TX status
    bsp.c               ← Clock/peripheral init, DMA strip output
    stm32l4xx_it.c      ← IRQ handlers (DMA, CAN routed to HAL)
    stm32l4xx_hal_msp.c ← HAL MSP init (GPIO/DMA mux for TIM16/CAN/UART)
LightingCAN.dbc         ← CAN database (open in CANdb++, Vector, or cantools)
Controls_LightingPCB.ioc← STM32CubeMX project (regenerate HAL from here)
STM32L431CBTx_FLASH.ld  ← Linker script
```

---

## How It All Fits Together

```
Controls ECU
    │  CAN 0x660 (1 byte, 10 Hz typical)
    ▼
can_app.c  ──── parses bits ──→  volatile LightingCommand cmd  (lighting.h)
                                         │
                              main loop calls render_frame() every 1 ms
                                         │
                              lighting.c reads cmd, decides pattern,
                              writes frame_colors[], calls commit_frame()
                              which fills led_pattern[] (PWM duty words)
                                         │
                              bsp_strip_show(led_pattern, NUM_STEPS)
                              kicks TIM16 DMA → WS281x data line
                                         │
                                     LEDs update
```

### Main Loop (`main.c`)

```c
while (1) {
    render_frame();                          // paint one frame
    bsp_strip_show(led_pattern, NUM_STEPS);  // DMA it out (blocks until done)
    if (status timer elapsed) can_status_send();
    HAL_Delay(MAIN_LOOP_PERIOD_MS);          // ~1 ms tick
}
```

`last_cmd_tick` is pre-seeded at startup so the watchdog doesn't fire before the first CAN frame arrives.

---

## CAN Protocol (`LightingCAN.dbc`)

### Command frame — sent by Controls ECU

| CAN ID | Name | DLC |
|--------|------|-----|
| `0x660` (1632 dec) | `Lighting_Command` | 1 byte |

Bit layout of the single data byte:

| Bit | Signal | Values |
|-----|--------|--------|
| 0 | `Lighting_Set_Headlights` | 0=Off, 1=On |
| 1 | `Lighting_Set_Left_Indicator` | 0=Off, 1=On |
| 2 | `Lighting_Set_Right_Indicator` | 0=Off, 1=On |
| 3 | `Lighting_Blink_Sync` | (reserved / future) |
| 4 | `Lighting_Set_Brake` | 0=Off, 1=On |
| 5 | `Lighting_Set_BPS_Strobe` | 0=Off, 1=On |
| 6-7 | `Lighting_Set_Custom_Mode` | 0=Off, 1-3=Mode 1-3 |

Example bytes:
- `0x01` → headlights only
- `0x02` → left indicator only
- `0x04` → right indicator only
- `0x10` → brake only
- `0x20` → BPS strobe only
- `0x12` → brake + left indicator (simultaneous)

### Status frames — sent by each board at 10 Hz

| CAN ID | Board |
|--------|-------|
| `0x670` | Front |
| `0x671` | Left |
| `0x672` | Rear |
| `0x673` | Right |
| `0x674` | Canopy |

Each 8-byte status frame echoes back the active command bits plus a fault code and three current readings (stubs — currently zero, see TODO in `can_app.c`):

| Bits | Signal |
|------|--------|
| 0–7 | `Lighting_Board_Fault` (see fault table below) |
| 8 | headlight |
| 9 | left indicator |
| 10 | right indicator |
| 11 | BPS strobe |
| 12 | brake |
| 13–14 | custom mode |
| 16–27 | addressable LED current (mA, scale 0.001 A) |
| 32–47 | LED0 current (mA) |
| 48–63 | LED1 current (mA) |

### Fault codes

| Code | Meaning |
|------|---------|
| 0 | OK |
| 1 | Addressable LED undercurrent |
| 2 | LED0 undercurrent |
| 3 | LED1 undercurrent |
| 4 | Addressable LED overcurrent |
| 5 | LED0 overcurrent |
| 6 | LED1 overcurrent |
| 7 | Light command watchdog tripped |
| 8 | MCU watchdog |

---

## Board Configuration (`lighting_config.h`)

This is the main knobs file. **Change `BOARD_ID` before flashing each board.**

### Selecting a board

```c
#define BOARD_ID      BOARD_FRONT   // change to BOARD_LEFT/REAR/RIGHT/CANOPY
```

| Constant | Value | Notes |
|----------|-------|-------|
| `BOARD_FRONT` | 0 | White headlight lit only on the two turn-indicator end segments (dark centre), amber split turn overlaid on top, headlight dims while turning, no brake |
| `BOARD_LEFT` | 1 | Left turn only, no headlight colour, no brake |
| `BOARD_REAR` | 2 | Red tail light, **red** split turn that owns its side (blinks red↔black, blanking the brake underneath), brake engine |
| `BOARD_RIGHT` | 3 | Right turn only, no headlight colour, no brake |
| `BOARD_CANOPY` | 4 | White headlight, full-strip turn sweep, brake engine |

### Per-board LED strip size

```c
#define MATTHEW_NUM_QUAD_CHIPS   9   // Front  → 36 LEDs
                                 3   // Left   → 12 LEDs
                                11   // Rear   → 44 LEDs
                                 3   // Right  → 12 LEDs
                                 8   // Canopy → 32 LEDs
```

`TOTAL_LEDS = 4 × MATTHEW_NUM_QUAD_CHIPS`. The first LED (`FIRST_ACTIVE = 1`) is always kept dark (used for strip addressing / data injection point).

### Headlight colour

```c
#define HEADLIGHT_R   0     // red channel (0-255)
#define HEADLIGHT_W   255   // white channel (0-255)
```

Per-board defaults: Front/Canopy = pure white (`W=255`), Rear = dim red (`R=64`), Left/Right = off.

**Front headlight is segment-only.** For regulatory reasons the front board does
not light the middle of the strip. `pattern_headlight_front()` lights only the
two end segments — the exact slots the split turn indicator occupies — leaving
the centre dark whether or not an indicator is active.

```c
#define HEADLIGHT_TURN_DIM_W   64   // front headlight white level while a turn
                                    // indicator is animating (dims from HEADLIGHT_W
                                    // so the amber sweep stands out). Front board only.
```

### Timing constants

```c
#define COMMAND_WATCHDOG_MS            500   // ms with no CAN → LEDs off
#define STATUS_TX_PERIOD_MS            100   // 10 Hz status TX
#define MAIN_LOOP_PERIOD_MS            1     // render loop period (ms)
#define BRAKE_RELEASE_DEBOUNCE_MS      150   // hold brake on after bit drops
#define HEADLIGHT_RELEASE_DEBOUNCE_MS  150   // hold headlight on after bit drops
#define TURN_RELEASE_DEBOUNCE_MS       150   // hold a turn indicator "requested"
                                             // after its bit drops (bridges senders
                                             // that interleave turn/brake frames so
                                             // the split indicator doesn't blip idle)
#define HEADLIGHT_TURN_DIM_LINGER_MS   100   // keep the front headlight dimmed this
                                             // long after the turn stops animating
```

### Animation timing

All in milliseconds; `lighting.c` converts them to main-loop frames via
`MAIN_LOOP_PERIOD_MS`, so changing the loop period rescales them automatically.

```c
#define TURN_STEP_MS    20   // per-LED/segment step during fill & empty
#define TURN_HOLD_MS    80   // hold time at full / empty between phases
#define BRAKE_STEP_MS   10   // per-LED step for the brake-bar expansion
```

### Pattern colours

RGBW channels (0-255 each), consumed by the pattern functions in `lighting.c`.

```c
// Full-strip sweep turn (Left/Right/Canopy) — amber
#define TURN_SWEEP_R 255 / TURN_SWEEP_G 100 / TURN_SWEEP_B 0 / TURN_SWEEP_W 0
// Front split turn overlaid on the headlight — amber
#define TURN_FRONT_R 255 / TURN_FRONT_G 64  / TURN_FRONT_B 0 / TURN_FRONT_W 0
// Rear split turn — red (matches the brake so it merges cleanly)
#define TURN_REAR_R  255 / TURN_REAR_G  0   / TURN_REAR_B  0 / TURN_REAR_W  0
// Brake bar — red
#define BRAKE_R      255 / BRAKE_G       0   / BRAKE_B      0 / BRAKE_W      0
```

### Animation mode

```c
#define ANIMATION_MODE   1   // ANIM_ON  = sweep animations
                         0   // ANIM_OFF = snap on/off, no sweep
```

### Turn indicator geometry (rear/front split indicator)

Set per board in the `#if BOARD_ID == ...` block:

```c
#define TURN_SEGMENTS_PER_SIDE   6   // Front (shorter bar, BRAKE_SPAN=16)
                                 7   // Rear  (longer bar,  BRAKE_SPAN=22)
```

Increasing this number extends how far the indicator sweeps in from each end. The centre gap shrinks correspondingly. Boards that use the full-strip sweep (Left/Right/Canopy) don't need it — `lighting.c` provides a default.

```c
#define REAR_TURN_SWAP_SIDES   0   // set to 1 if left/right halves are
                                   // physically wired in reverse
```

### Brake strip geometry

```c
#define BRAKE_STRIP_FOLDED   1   // 1 = strip doubles back on itself (default)
                             0   // 0 = single straight run
```

When folded, the strip's physical midpoint is at array index 0/`TOTAL_LEDS-1` (the wire turnaround), so `brake_phys_pos()` remaps array indices to physical slots so the expansion looks centred.

---

## Pattern Engine (`lighting.c`)

### Pattern priority per board

Higher in the list wins. Each board has a dedicated dispatch path compiled in via `#if BOARD_ID == ...`.

**Front:**
Base layer: headlight (debounced) → custom mode → off. The headlight lights only
the two turn-indicator end segments (dark centre) and **dims to
`HEADLIGHT_TURN_DIM_W`** while a turn is animating. Amber split turn is overlaid
on top (simultaneous headlight + turn). The BPS strobe is a separate GPIO fixture
(see below), independent of the strip.

**Rear:**
Brake (red, grows from centre) as base layer. The split turn is **red** and
*owns* its side while animating: it blinks red↔black over its segments, blanking
the brake underneath, so between blinks that side goes dark while the rest of the
bar stays red. While braking, the turn's first and last blinks are forced to the
lit state so they merge seamlessly into the red brake instead of blipping (see
`end_full` below). If neither owns the frame: headlight → custom mode → off.

**Left / Right:**
Turn indicator (full sweep). While not turning: BPS strobe if requested, else off. No headlight, no brake.

**Canopy:**
Brake → turn → BPS strobe → headlight → custom mode → off.

### Pattern functions

| Function | Description |
|----------|-------------|
| `pattern_off()` | All LEDs off |
| `pattern_headlight()` | Solid full-strip colour `HEADLIGHT_R`/`HEADLIGHT_W` (rear/canopy) |
| `pattern_headlight_front(turn_active)` | Front only: lights just the two turn-segment ends (dark centre); dims the white channel to `HEADLIGHT_TURN_DIM_W` when `turn_active` |
| `pattern_custom_mode(mode)` | Modes 1-3 (currently placeholder → shows headlight colour; TODO) |

The BPS strobe is **not** an LED pattern — `render_frame()` drives it directly as
a GPIO (`BPS_STROBE_Pin`) whenever the strobe bit is set, independent of the RGB
strip, so it can be active alongside any strip pattern.

### Animated state machines

**Turn indicator — full strip sweep** (`turn_step`, used by Left/Right/Canopy):
`IDLE → FILLING → HOLD_FULL → EMPTYING → HOLD_EMPTY → (loop or IDLE)`
Amber fills LED by LED from one end, holds, empties back out. When the request drops it finishes the current cycle out before going idle (no snap-off).

**Turn indicator — split sweep** (`turn_render` + `turn_side_advance`, used by Front/Rear):
Two independent halves (`turn_low` = left, `turn_high` = right), each running the same fill/hold/empty/hold lifecycle.
- **Front:** amber, painted only on its lit slots, so the (dimmed) headlight shows between blinks and on the inactive side.
- **Rear:** red, and while a side is animating it takes over that whole side — red where lit, **black in the gaps** (overriding the brake base).

`turn_side_advance()` takes an `end_full` flag (passed as `brake_req` on the rear, `0` on the front). When set:
- the turn *starts* in the closing phase from full (so on a red brake base the first blink begins already-lit and blinks by going dark — no start blip), and
- when the request drops it always finishes in the **lit** state and releases the region to the identical red brake base (no fade-out, no end blip), reversing back to fill if it was caught mid-empty.

The raw left/right bits are debounced by `TURN_RELEASE_DEBOUNCE_MS` before feeding the state machines, so interleaved turn/brake frames don't make a side spuriously idle between blinks.

**Brake** (`brake_step`, used by Rear/Canopy):
`IDLE → FILLING → HOLD → EMPTYING → IDLE`
Red bar grows outward from the centre to both ends, holds, then contracts when released. On the rear board it uses the same physical region as the turn indicators. On canopy it grows from the exact strip centre.

### Tuning animation timing and colour

All animation timing and pattern colours are now knobs in `lighting_config.h`
(see **Animation timing** and **Pattern colours** above), not literals in
`lighting.c`. `lighting.c` only derives frame counts from the `*_MS` values, e.g.
`TURN_FRAMES_PER_STEP = TURN_STEP_MS / MAIN_LOOP_PERIOD_MS`. To retune, edit the
config — no need to touch the pattern engine.

---

## Hardware / BSP (`bsp.c`)

### WS281x strip output

- **Timer:** TIM16, Channel 1
- **DMA:** DMA1 Channel 3
- **GPIO:** PA6 (TIM16_CH1 via AF14)
- **Clock:** 80 MHz SYSCLK → TIM16 period = 51 counts (≈1.25 µs/bit, matching WS281x spec)
  - `LOW` duty = 30 counts (≈375 ns high → WS281x "0" bit)
  - `HI` duty = 41 counts (≈512 ns high → WS281x "1" bit)
- `bsp_strip_show()` blocks until the previous DMA transfer completes (via `strip_dma_done` flag), then starts the next one. The `HAL_TIM_PWM_PulseFinishedCallback` ISR stops the timer and sets the flag when done.

### CAN

- **Peripheral:** CAN1
- **Baud rate:** 80 MHz / prescaler 20 / (1 + 13 + 2) TQ = **250 kbps**
- **RX filter:** exact match on `0x660`, standard frame, 32-bit ID-mask mode, FIFO0
- **RX IRQ:** `CAN1_RX0_IRQn`, priority 0

### Debug / heartbeat GPIO

| Pin | Function |
|-----|----------|
| PB11 | Toggles on every successfully parsed CAN RX frame |
| PA12 | General purpose output (currently unused) |
| PB4 | BPS strobe external fixture GPIO (defined in `main.h`) |

### Clock tree

MSI → PLL (×40 / ÷2) → 80 MHz SYSCLK. All APB clocks at 80 MHz.

---

## Adding a New Board

1. Add a new `#define BOARD_XXX N` constant in `lighting_config.h`.
2. Add an `#elif BOARD_ID == BOARD_XXX` block in the per-board section of `lighting_config.h` defining:
   - `MY_STATUS_ID`
   - `RESPONDS_TO_LEFT` / `RESPONDS_TO_RIGHT`
   - `HEADLIGHT_R` / `HEADLIGHT_W`
   - `MATTHEW_NUM_QUAD_CHIPS`
   - `TURN_SEGMENTS_PER_SIDE` (only if the board uses the split indicator; full-strip-sweep boards fall back to the default in `lighting.c`)
   - `HEADLIGHT_TURN_DIM_W` (only if the board dims its headlight while turning, like the front)
3. Add a dispatch path in `render_frame()` in `lighting.c` (copy the closest existing board's block).
4. Add the new node to `LightingCAN.dbc` and assign a status CAN ID.

---

## Implementing Custom Modes

`pattern_custom_mode()` in `lighting.c` is currently a stub that falls back to `pattern_headlight()` for any non-zero mode. The TODO comment points to three planned modes:

```c
static void pattern_custom_mode(uint8_t mode) {
    if (mode == 0) { pattern_off(); return; }
    // TODO: mode 1 = RGB rainbow, mode 2 = burnt orange, mode 3 = palette fade
    pattern_headlight();   // placeholder
}
```

Implement by adding a `switch(mode)` and filling `frame_colors[]` via `set_led()` calls, same as the other pattern functions.

---

## Implementing LED Current Sensing

`can_status_send()` in `can_app.c` has three stub variables:

```c
uint16_t addr_led_current_mA = 0;   // 12-bit ADC reading → mA
uint16_t led0_current_mA     = 0;   // 16-bit
uint16_t led1_current_mA     = 0;   // 16-bit
```

Replace these with real ADC reads. The fault logic in `render_frame()` currently only sets `FAULT_LIGHT_CMD_WATCHDOG`; add over/undercurrent checks here using the fault codes defined in `lighting_config.h`.

---

## Common Issues

| Symptom | Likely cause |
|---------|--------------|
| All LEDs off, no heartbeat on PB11 | CAN filter mismatch or sender using extended ID |
| All LEDs off, heartbeat toggling | Watchdog firing — check `COMMAND_WATCHDOG_MS`, ensure sender rate is fast enough |
| Steady patterns don't show, animations work | DMA race in `bsp_strip_show` — ensure `strip_dma_done` flag is in place |
| Left/right turn indicator swapped | Set `REAR_TURN_SWAP_SIDES 1` in `lighting_config.h` |
| Brake or turn animation never stops | State machine stuck — check `active` flag debounce logic in `render_frame` |
| Wrong number of LEDs lit | `MATTHEW_NUM_QUAD_CHIPS` doesn't match physical strip |
