# pico-emu — hook spec (for emulator agent)

Package: `pico-emu` first-party source (`/home/danish1075/Documents/rp2350/Pico-emu`, commit `a47d098`). Our runner: `OpenHW-studio-frontend/src/worker/runners/pico-runner.ts`.
Status (2026-10): jsmirror is upstreamed in-tree (`src/sdd_jsmirror.c`, `src/sdd.c`, `include/sdd.h`, `build_wasm.sh`) and the simulator vendors the first-party build — the old `vendor/pico-emu-fork/fork.patch` replay is retired (kept for history). All P0/P1 engine items in this spec are now DONE in-tree (scan-ACK bridge, RV TIMER bridge + MIE-free wake, 32-bit sleep hints, SPI/ADC/PWM taps, M33 SMULBB HardFault fix + full DSP family, reset-clears-rings, `_picoemu_cycle_count` sim-time stamp, RP2350 IO_QSPI routing + STATUS model, RV MEICONTEXT reset + live NOIRQ/IRQ tracking, PIO FIFO taps, FC/FD loud-decline): 507 native tests green, firmware sweep 71/71, WASM rebuild + `test-wasm.js` + all four `.mjs` harnesses green. Two deliberate silicon-fidelity exceptions to strict bit-identical-no-sdd_add (both empirically clean across suite+sweep+wasm): DW TX_ABRT on empty addresses (was silent success — required for correct SDK/Arduino NACK codes on the DW path) and open-drain release reading HIGH (was stale latch — required for scan: clock-stretch SCL polling, STOP visibility, ACK sampling). ARM MP sleep live-proven native (`time.sleep(1)` returns, REPL continues); RV sleep unit-proven; M33 RP2350-MP boots to `>>>` live (native + `test-wasm-mp2350.mjs`, incl. USB-CDC `print(6*7)` → `42`); RV32_TICK image prints live.

## What to add

1. **jsmirror SDD slave** (I2C observation channel, default OFF — active only after `sdd_add`):
   - `_picoemu_sdd_add("jsmirror:i2c=0,addr=0x3C,...")` → registers ACK-sink slaves, returns ≥0 on success.
   - `_picoemu_jsmirror_pending()` → count of queued ring entries.
   - `_picoemu_jsmirror_pop(ptr, max)` → drains entries as `uint16_t`:
     - `0x200` = STOP, `0x100|addr` = START+address, `0x00–0xFF` = data byte.
2. **Address-probe ACK for scans**: `machine.I2C.scan()` (MicroPython) and `Wire` address probe must see ACK at registered addresses — same path as Arduino DW writes, no firmware-specific casing.

## What to expose (stable JS ABI)

`_picoemu_sdd_add`, `_picoemu_jsmirror_pending`, `_picoemu_jsmirror_pop`,
plus existing `_picoemu_write_uart` (REPL injection) and UART-out poll — all already used by our runner, keep signatures frozen.

## MicroPython findings (FIXED in-engine 2026-10)

7. **Scan probes never ACKed registered slaves — FIXED.** Root cause
   confirmed: `machine.I2C.scan()` uses soft-I2C (GPIO bit-bang), never
   the DW controller. New GPIO-level slave bridge (`src/i2c_bitbang.c`,
   hooked in the SIO write path for ARM + RV32): START/STOP framing,
   address+data decode, ACK driven into the SDA latch for any attached
   slave, bytes routed through the same device callbacks as the DW
   path (jsmirror ring observes bit-banged traffic too). Companion
   fixes: open-drain release restores pull-up HIGH, and the DW path
   now models TX_ABRT (ADDR_NOACK/TXDATA_NOACK + auto-STOP +
   RX-flush, clear-on-read) instead of hard-returning 0 — Arduino
   `endTransmission`/scan gets real NACK codes (Arduino probes
   zero-length writes via GPIO `_probe`, also covered by the bridge).
8. **`time.sleep()` never woke — FIXED for RV32.** Root cause: TIMER
   alarms only signalled the ARM NVIC; the RV CLINT never saw them, so
   a WFI hart slept forever (plus the wake was gated on MIE).
   `rv_clint_timer_fired()` bridges fresh TIMER0/1 edges (wired into
   both RV step loops, `main.c` + `picoemu_wasm.c`), and the WFI wake
   no longer needs MIE (wake-without-deliver, matching the ARM check).
   Also: 32-bit WFI/WFE/SEV hints now really sleep/wake (were NOPs).
   Live proof (ARM M0+ MP, native): stock `micropython_rp2040.uf2`
   REPL `time.sleep(1)` returns and the interpreter continues
   (`print(time.ticks_ms()-t)` → 395) — the full TIMER-alarm →
   NVIC → WFE-wake → deliver chain works; no avoidance needed on
   this path. RV sleep is unit-proven (`test_rv_*wfi*`/`test_rv_timer*`).

9. **M33 MicroPython RP2350 silent hang — FIXED.** The real
   `micropython-rp2350-arm` image spun forever in a QSPI-timing wait
   (`ldr r2,[r1,#0x18]; lsls #22; bpl`, SRAM copy of flash
   `0x1004ED1C`) polling IO_QSPI SD1 STATUS OUTTOPAD (bit 9). On
   RP2350 that block lives at `0x40030000`, but the bus routed it to
   the RP2040 BUSCTRL stub (hard zero) — and IO_QSPI STATUS itself
   was a hard-zero stub. Fix (`src/membus.c`): arch-aware routing
   (RP2350 `0x40030000` → IO_QSPI model, real RP2350 BUSCTRL
   `0x40068000` → busctrl model) + STATUS computed from pad control
   (OUTTOPAD/OETOPAD/INFROMPAD; flash bus idles high). Live proof:
   stock image boots to `MicroPython v1.28.0 ... Pico2 with RP2350`
   + `>>>` over USB-CDC. Regression lock: `test_ioqspi_rp2350_status`.

10. **RV32 silent hang (ebreak park) — FIXED.** The RV32_TICK image
    died in SDK startup on `csrr a5,0xBE5; slli; bltz; ebreak`: CSR
    `0xBE5` is Hazard3 **MEICONTEXT**, whose reset value is `0x8000`
    (NOIRQ, bit 15); we returned 0 (and our header even had the
    address wrong as `0xBE6`). Fix (`rv_cpu.h`, `rv_cpu.c`):
    correct address + reset `0x8000` (writes store through).
    Live proof: the image prints `RV32_BOOT` + repeating `RV32_TICK`
    over UART0. Regression locks: `test_rv32_tick_serial`
    (hand-encoded RV32 guest prints `RV32_TICK` in-process) +
    `web/rv32_tick.uf2` sweep guest (`test-firmware/gen_rv32.py`).
## M33 HardFault in I2C burst path (ROOT-CAUSED + FIXED 2026-10, was BLOCKING Pico 2)

M33 Arduino faulted deterministically inside Adafruit `display()` I2C
writes; M0 identical-shape passed (native + browser).
`[CPU] HardFault: PC=0x3E380000 LR=0x10007D0F SP=0x20081FC0`
(same PC/LR/SP across sketches; ~447K steps; reboot loop in browser).

Root cause (proven with `-trace` + ELF disassembly, NOT a fetch/ICache
misfire — the old "phantom branch" theory is dead): GCC emits `smulbb`
for the framebuffer math in `Adafruit_SSD1306::display()`, and the
Thumb-2 decoder had no SMULBB/SMLABB family. The pattern fell through
to `t32_ldst_single`, which misread `FB16 F603` as LDRB with Rt=15:
PC = a data byte (0x10). Execution wandered the ROM magic as code,
hit `BX lr` with a stale LR, landed mid-`i2c_write_blocking_internal`,
and faulted later at `blx r6` with a garbage r6. M0+ is immune because
M0 code never emits DSP multiplies.

Fix (in-tree): the full DSP multiply family in `t32_misc`
(`src/thumb32.c`) — SMULBB/BT/TB/TT + SMLABB/..., SMLAD/X + SMLSD/X
+ SMUAD/X + SMUSD/X (the X halve-swap forms were missing too),
SMULW + SMLAW, SMMUL/R + SMMLA/R, SMMLS/R, USAD8/USADA8 (Q on
accumulate overflow, `Rd==15` declined). Every encoding verified
against capstone as an independent decoder and every semantic against
unicorn as a differential oracle (vectors in `test_m33_dsp_*`); the
old SMUSD-shape guess (010X) was wrong per both oracles and is NOT
matched. Residual FA/FB/FE/FF (DSP/multiply/vector space, never a
load — loads are F8/F9) now returns unhandled (loud HardFault)
instead of misdecoding. Also permanent diagnostics: HardFault line
now logs the vector handler word + faulting halfwords + IT state
(`H=/W=/IT=`), a callee-saved (r4-r11) entry/return integrity check
(`CALLEE-CLOBBER` — silent in the full suite), and 32-bit insns are
now in `-trace`.
Repro (no browser needed): compile `m33oled.ino` (Adafruit SSD1306
`display()` loop, Arduino rp2040:rp2040:rpipico2) and run
`./build/picoemu m33oled.ino.uf2 -arch m33
-sdd jsmirror:i2c=0,addr=0x3c -timeout 60`: before → instant
`PC=0x3E380000` fault, after → clean frames (4-minute burn-in, zero
faults). Native needs the
CMakeLists `sdd_jsmirror.c` entry (already added).
Note: native needs no SDK — Arduino CLI + rp2040 core 6.0.0 suffice.

## Further gaps (same agent, P1 — full protocol audit 2026-10)

Our runner (`pico-runner.ts`) wires **no** SPI / ADC / PWM today, and the
stock glue exports no tap for them (verified: `_picoemu_*` surface is
## Further gaps (same agent, P1 — full protocol audit 2026-10)

Our runner (`pico-runner.ts`) wires **no** SPI / ADC / PWM today, and the
stock glue exports no tap for them (verified: `_picoemu_*` surface is
GPIO get/oe/out/raw + `set_gpio`, UART read/write, SD-image load,
USB/WiFi/BT/Eth/W5500, GDB — no `spi/i2c/adc/pwm` beyond this spec's SDD):

3. **SPI slave tap — DONE.** `spimirror:spi=0` SDD (`src/sdd_spimirror.c`):
   MOSI observe + MISO inject queue + CS framing into a shared ring
   (`0x100|spi` assert, `0x200|spi` deassert, bytes raw; 0xFF idle).
   ABI: `_picoemu_spimirror_pending/_drops/_pop/_inject` (in EXPORTS).
   Note: single PL022 device slot per bus — replaces sdcard/emmc there.
4. **ADC inject — DONE.** `_picoemu_adc_set/_get` (raw 12-bit,
   `raw = mv*4095/3300`; ch 0-3 = A0-A3, 4 = temp) in EXPORTS.
5. **PWM/timer-out observe — DONE.** `_picoemu_pwm_read(slice, &hz,
   &dutyA_10000, &dutyB_10000, &enabled)` in EXPORTS (any out-param
   may be NULL; freq from live sysclk).
6. PIO programs, I2S audio-out: P2 (no cells written yet; do not block on them).

## Future-component guarantee (signal-level contract)

Components are unbounded (registry already holds I2C sensors, SPI TFT/ePaper,
TM1637 bit-bang, NeoPixel timing, HC-SR04 echo, IR 38 kHz, step/dir drivers,
I2S audio) — so the promise is per **primitive**, not per part. After §1–5,
ANY future component built on these works with zero further engine changes
(only generic runner-bridge mapping, which already exists):

- GPIO out edge + level, GPIO in inject, per-pin analog in, I2C byte both
  directions + ACK/NACK + scan, SPI byte both directions + CS, UART both
  directions, PWM-out freq+duty.
- P2 engine ask (not blocking): per-step cycle/instruction counters so the
  runner can stamp edges in *sim* time — today OneWire/timing uses a
  wall-clock approximation (`arm-runner-base.ts` OneWire decode), which is
  exact only when wall pace == sim pace.

## Constraints

- Additive-only; when `sdd_add` is never called, emulation must be bit-identical to stock 1.0.3.
- Pin the Emscripten version + build script in the repo so the `.wasm` rebuild is reproducible.

## Cross-cutting constraints (sync/SAB — engine side of the contract)

Engines never touch SharedArrayBuffer: each engine runs as a plain
synchronous step function inside its board Worker, and only our
`arm-runner-base.ts` touches SAB (pin bitfields, barrier cells, STEP
collect). Two things the engine must still guarantee so multi-board
lockstep holds:

1. **Reset clears everything, synchronously** — cores, peripherals, pending
   events, and any `stopped`/fault flags. A sticky flag desyncs one board's
   lane forever while the others advance.
2. **Expose a cycle/instruction counter per step** — feeds sim-time edge
   stamps (replacing the runner's wall-clock approximation) and the
   skew/pace accounting. One counter serves both.

## Acceptance (our e2e, already written)

- `tests/e2e/pico-i2c.spec.js`: OLED `vramFill` 3.5–3.7 + LCD2004 `Hello` — green today on the fork, must stay green on first-party build.
- `tests/e2e/pico-mp-i2c.spec.js`: `MP_SCAN: [60]` + `vramFill > 0` via MicroPython `machine.I2C` — engine side PROVEN native 2026-10: stock `micropython_rp2040.uf2` + `-sdd jsmirror:i2c=0,addr=0x3c`, REPL `machine.I2C(0).scan()` returns `[60]` (default pins scl=5/sda=4); all other addresses NACK cleanly with no 50ms clock-stretch stalls (open-drain release restore). `vramFill` needs the runner's display model on top.
