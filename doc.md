# VCAT Lighting PCB – Firmware Guide

Firmware for the **STM32L431CBTx** lighting boards on the solar car. Every board
runs the same source; you pick which board it is by changing one `#define`. A
central Controls ECU broadcasts a 1-byte CAN command; each board reads it and
switches its lights.

Lights are plain **on/off fixtures** driven by two FET GPIO channels per board —
no addressable LEDs, no colour, no animation.

## Layout

```
Core/
  Inc/
    lighting_config.h   ← board select + all config (start here)
    lighting.h          ← command struct, shared state, render_frame()
    can_app.h           ← CAN init / TX
    bsp.h               ← peripheral handles, bsp_init()
    main.h              ← MCU includes, light channel pins
  Src/
    main.c              ← entry point + main loop
    lighting.c          ← reads the command, drives the two GPIO channels
    can_app.c           ← CAN RX parse, status TX
    bsp.c               ← clock + peripheral init (CAN, UART, GPIO)
LightingCAN.dbc         ← CAN database
```

## How it works

```
Controls ECU ── CAN 0x660 (1 byte) ──► can_app.c parses into `cmd`
                                              │
                              main loop calls render_frame() every 1 ms
                                              │
                              lighting.c reads `cmd`, drives CH1/CH2 GPIO
```

Main loop (`main.c`):

```c
while (1) {
    render_frame();                 // evaluate command, set the two channels
    if (status timer elapsed) can_status_send();
    HAL_Delay(MAIN_LOOP_PERIOD_MS); // ~1 ms
}
```

## Output channels

Each board has two active-high FET outputs, `CH1` and `CH2` (pins in `main.h`).
What each drives is set per board in `lighting_config.h` via `CH1_FUNC`/`CH2_FUNC`:

| Function | Behaviour |
|----------|-----------|
| `FN_NONE` | always off |
| `FN_HEADLIGHT` | on while headlight commanded |
| `FN_TURN_LEFT` / `FN_TURN_RIGHT` | blinks while that indicator is on |
| `FN_BRAKE` | on while brake commanded |
| `FN_STROBE` | BPS strobe (held on `BPS_STROBE_HOLD_MS` after last frame) |
| `FN_BRAKE_TURN_LEFT` / `_RIGHT` | red light: solid for brake, but the turn blink wins when that side is on |

## Board config (`lighting_config.h`)

Change `BOARD_ID` before flashing each board:

```c
#define BOARD_ID   BOARD_REAR   // FRONT_LEFT / FRONT_RIGHT / REAR / CANOPY / UNUSED
```

| Board | CH1 (PA11) | CH2 (PA12) | Status ID |
|-------|-----------|-----------|-----------|
| `BOARD_FRONT_LEFT` | left turn | — | 0x671 |
| `BOARD_FRONT_RIGHT` | right turn | headlight | 0x670 |
| `BOARD_REAR` | brake + left turn | brake + right turn | 0x672 |
| `BOARD_CANOPY` | strobe | brake | 0x674 |
| `BOARD_UNUSED` | — | — | 0x673 |

`BOARD_UNUSED` drives nothing; it just sits on the bus for CAN passthrough.

Front turn indicators are wired to the side turn lights on the harness side, so
the firmware only deals with front left / front right.

### Timing

```c
COMMAND_WATCHDOG_MS   500   // no command within this → all lights off
STATUS_TX_PERIOD_MS   100   // 10 Hz status TX
MAIN_LOOP_PERIOD_MS   1     // loop period
BPS_STROBE_HOLD_MS    1000  // strobe stays on 1 s after last strobe frame
TURN_FLASH_PPM        67    // turn blink rate (pulses/min, 50% duty)
```

`*_RELEASE_DEBOUNCE_MS` hold a request briefly after its bit drops, bridging
senders that interleave brake/turn frames so outputs don't flicker.

## CAN protocol (`LightingCAN.dbc`)

**Command** — `0x660`, 1 byte, from Controls ECU:

| Bit | Signal |
|-----|--------|
| 0 | headlights |
| 1 | left indicator |
| 2 | right indicator |
| 3 | blink sync (reserved) |
| 4 | brake |
| 5 | BPS strobe |
| 6–7 | custom mode (ignored) |

**Status** — each board TXs at 10 Hz on `0x670`–`0x674` (see table above). Byte 0
is the fault code; byte 1 echoes the active command bits. Current-sense fields
are stubbed at 0 (see `can_app.c`).

### Fault codes

| Code | Meaning |
|------|---------|
| 0 | OK |
| 7 | command watchdog tripped |
| 8 | MCU watchdog |

(Codes 1–6 are LED current faults from the old strip design, currently unused.)

## Hardware (`bsp.c`)

- **CAN1** — 250 kbps, RX filter matches only `0x660`, RX/TX on PB8/PB9.
- **Clock** — MSI → PLL → 80 MHz SYSCLK.
- **Light outputs** — CH1 = PA11, CH2 = PA12, push-pull, active-high.

| Pin | Function |
|-----|----------|
| PA11 | light channel 1 |
| PA12 | light channel 2 |
| PB11 | board heartbeat (1 Hz) |
| PA8 | CAN RX heartbeat |
| PA15 | CAN TX heartbeat |

## Adding a board

1. Add `#define BOARD_XXX N` in `lighting_config.h`.
2. Add an `#elif BOARD_ID == BOARD_XXX` block setting `MY_STATUS_ID`, `CH1_FUNC`,
   `CH2_FUNC`.
3. Add the node to `LightingCAN.dbc`.

## Common issues

| Symptom | Likely cause |
|---------|--------------|
| All lights off, no PB11 heartbeat | CAN filter mismatch or extended-ID sender |
| All lights off, heartbeat toggling | Watchdog firing — check sender rate vs `COMMAND_WATCHDOG_MS` |
| Wrong channel lit | `CH1_FUNC`/`CH2_FUNC` or pins swapped |
| Turn won't blink | check `TURN_FLASH_PPM`, confirm the indicator bit is set |
