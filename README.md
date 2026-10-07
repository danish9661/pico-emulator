# picoemu – RP2040/RP2350 Emulator (WebAssembly)

[![npm version](https://img.shields.io/npm/v/pico-emu.svg)](https://www.npmjs.com/package/pico-emu)
[![npm downloads](https://img.shields.io/npm/dm/pico-emu.svg)](https://www.npmjs.com/package/pico-emu)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Tests](https://github.com/danish9661/pico-emulator/actions/workflows/test.yml/badge.svg)](https://github.com/danish9661/pico-emulator/actions/workflows/test.yml)
[![Pages](https://github.com/danish9661/pico-emulator/actions/workflows/pages.yml/badge.svg)](https://danish9661.github.io/pico-emulator/)

> **Credit:** Fork of [Night-Traders-Dev/Bramble](https://github.com/Night-Traders-Dev/Bramble) (MIT, © 2025 Night-Traders-Dev) — original M0+ core, peripherals and tests. Port, RP2350 cores (M33 + RV32), W6300 model, browser UI and all work since by [danish9661](https://github.com/danish9661) (© 2026, see [LICENSE](LICENSE)).

A from-scratch emulator for Raspberry Pi RP2040 and RP2350 microcontrollers, supporting both ARM Cortex-M0+ (Thumb) and RISC-V Hazard3 (RV32IMAC) cores. Loads and executes UF2 and ELF firmware with accurate memory mapping and peripheral emulation. Compiles to WebAssembly via Emscripten for browser execution at ~8-10× speed over pure-JS emulators.

**Live demo:** `https://danish9661.github.io/pico-emulator/` (`web/` deployed via GitHub Pages, `web/.nojekyll` + `/.github/workflows/pages.yml`).

**npm:** `npm i pico-emu` — WASM build, browser UI and all demo UF2 firmware ([npmjs.com/package/pico-emu](https://www.npmjs.com/package/pico-emu)).

```sh
npx pico-emu hello_world.uf2         # CLI: arch auto-detected from UF2
```

Browser UI (`web/index.html`) has RP2040 / M33 / RV32 demo dropdowns, serial
monitor and GPIO viewer. Full usage + every API export: [`docs/PICOEMU.md`](docs/PICOEMU.md).

## Current Status: v1.0.4

  507/507 tests passing, sweep 71/71. **RP2040 (M0+)**: littleOS shell, TinyUSB CDC `hello_usb`, MicroPython v1.22.1 REPL (USB CDC), all peripheral self-tests. **RP2350 ARM (M33, `-arch m33`)**: littleOS shell with Sage eval (`print(6*7)` = `42`), VFP single + deferred-compute double, TrustZone SAU/MPU, DSP scalar + MVE-Helium integer vectors, MicroPython RP2350 REPL (USB CDC), Adafruit SSD1306 display. **RP2350 RISC-V (RV32, `-arch rv32`)**: Hazard3 RV32IMAC + Zba/Zbb/Zbs/Zcb/Zcmp + Zfinx single-float, CLINT, dual-hart; littleOS shell, RV32_TICK serial. Firmware auto-detects via UF2 family ID / picobin IMAGE_DEF. **Networking**: vnet bus (TAP bridge, peer mesh), W5500/W6300 live sockets (`-net-live`, `-net-live6300`, `web/net_proxy.py`), MACRAW socket-0 single-gateway path shared by WiFi + both Ethernet chips. **Wired Ethernet**: `pico-eth`/`pico-eth2` (W5500, SPI0 CS17/RST20/INT21) + `pico-w6300`/`pico-w6300-2` (W6300 dual IPv4/IPv6 offload, SPI0 QSPI-single CS16/RST22/INT15); in-tree `eth_dhcp`/`eth_http`/`eth_dhcp6300`/`eth_http6300` guests (DORA + HTTP on all three cores) + Arduino `Wiznet5500lwIP` / `W6300lwIP` DORA prove-outs (see CHANGELOG). **Bluetooth**: HCI responder + GATT loopback (`GATT-DONE`, sweep-locked), HCI-forward to Bumble/RootCanal/physical (`web/hci_bridge.py`), ARM `ble_adv` guests reach `ARM BLE LISTEN`.

### Coverage

| Area | Status | Details |
|------|--------|---------|
| RP2040 CPU | 65+ Thumb-1 | O(1) dispatch, NZCV, BL/MSR/MRS/barriers |
| RP2350 ARM | Cortex-M33 | Thumb-2, BASEPRI, M33 CPUID, VFP single + DCP double, DSP scalar, MVE-Helium int |
| RP2350 RV | Hazard3 RV32IMAC | Zba/Zbb/Zbs/Zcb/Zcmp + Zfinx float, custom CSRs, CLINT, bootrom, icache |
| RP2350 Peripherals | Complete | TICKS, POWMAN, QMI, OTP+data, BOOTRAM, TIMER1, PIO2, HSTX, TRNG, SHA-256, 48 GPIO, SIO |
| Memory Map | RP2040 + RP2350 | RP2040: 2MB flash + 264KB SRAM + 16KB ROM. RP2350: 4MB flash + 520KB SRAM + 32KB ROM + CLINT |
| Boot | UF2 + ELF | boot2, ROM functions, RV bootrom, picobin IMAGE_DEF, family-ID arch detect |
| Exceptions | ARM + RV | Tail-chaining, late-arriving, PRIMASK/FAULTMASK; mtvec, MRET, CLINT, Hazard3 ext IRQ |
| Timing | Configurable | `-clock 125/150`, per-instruction costs, CLINT mtime, TIMER1 |
| Debugging | GDB RSP + 18 tools | Breakpoints, coverage, trace, VCD, heatmap, fault injection (`-gdb`, `-semihosting`, …) |
| Flash/Storage | Write-through | `-flash` sync, `-mount` FUSE, SD card + eMMC (SPI, file-backed) |
| WiFi/BT | CYW43 (Pico W) | gSPI-over-PIO, scan/join, TAP bridge (`-wifi`, `-tap`), HCI responder + GATT, `-bt-hci` forward |
| Virtual Network | VNet bus | TAP/NAT (`-net`), peer mesh (`-net-peer`), W5500 (`-net-live`) / W6300 (`-net-live6300`) live sockets |
| Multi-Device | Wire + SDD | UART/GPIO/Ethernet instance links (`-wire-*`), TMP102 thermometer + 24LC256 EEPROM (`-sdd`) |
| Performance | ICache + JIT | 64K decode cache default, `-jit` hot blocks; native ~86 MIPS, WASM ~22–25 MIPS |
| Dev Tools | 18 tools | Semihosting, coverage, hotspots, profile, trace, callgraph, VCD, IRQ latency, stack check, watch, expect, script, fault injection, heatmap, symbols, exit codes, timeouts |
 | Tests | 507 | CTest integrated; RV + M33 + networking + storage + W6300 dual-stack/PACKET-INFO/RA-capture |

### Peripherals

| Peripheral | Address | Emulation Level |
|------------|---------|-----------------|
| GPIO / SIO | `0x40014000` / `0xD0000000` | Full: edge/level IRQs, FIFOs, spinlocks, divider, interpolators (48 pins on RP2350) |
| UART | `0x40034000` / `0x40038000` | Full: dual PL011, 16-deep FIFOs, stdin routing |
| SPI | `0x4003C000` / `0x40040000` | Full: dual PL022, FIFOs, device callbacks (SD, W5500, CYW43) |
| I2C | `0x40044000` / `0x40048000` | Full: dual DW_apb_i2c, device callbacks (TMP102, EEPROM) |
| Timer / SysTick | `0x40054000` / `0xE000E010` | Full: 64-bit + 4 alarms, TICKINT/COUNTFLAG |
| PWM / ADC / DMA / PIO | `0x40050000` etc. | Full: PWM slices, ADC+temp, 12–16ch DMA, PIO exec (RP2350: 12-slice PWM, 9-mux ADC, 16ch DMA, PIO2) |
| NVIC / SCB | `0xE000E100` | Full: preemption, PRIMASK/FAULTMASK, M33 BASEPRI, 64-IRQ RP2350 map |
| Resets / Clocks / XOSC / PLL / WDT | `0x4000C000`… | Full: SELECTED, FC0, LOCK/STABLE, reboot |
| SIO / ROM / USB / RTC | misc | Full: ROM table + soft-float/double, USB CDC bridge, RTC calendar |
| XIP / Flash | `0x10000000`… | Full: cache control, write-through `-flash`, `-mount` FUSE |
| CYW43 / W5500 / W6300 | boards | CYW43 gSPI WiFi/BT; W5500 SPI NIC; W6300 QSPI dual-stack NIC (see Networking) |
| Stubs | misc | SYSINFO / IO_QSPI / PADS_QSPI / XIP-cache-timing (always-ready) |

### Storage Devices

| Device         | Interface          | Details                                                                |
|----------------|--------------------|------------------------------------------------------------------------|
| SD Card (SDHC) | SPI (default SPI1) | Full SPI-mode protocol, CSD v2.0, single/multi-block R/W, file-backed |
| eMMC           | SPI (default SPI0) | CMD1 init, EXT_CSD, sector addressing, file-backed                    |

Both devices attach via `spi_attach_device()` callbacks with periodic flush and flush-on-exit.

All peripherals support RP2040 atomic register aliases (SET/CLR/XOR). RP2350 adds: HSTX serializer, TRNG stream, SHA-256 (FIPS vector), SAU/MPU + faults + TT, DSP scalar + MVE-Helium integer vectors, Zfinx RV32 float.

### Known Limitations

- **Cycle timing**: Default 1 MHz (fast-forward). Use `-clock 125` for real RP2040 timing.
- **Fidelity tradeoffs**: DMA pacing, very high-speed PIO timing, and non-CDC USB device behavior are still functional models rather than fully cycle-perfect hardware.
- See [ROADMAP](docs/ROADMAP.md) for detailed status.

## Building and Running

### Prerequisites

- CMake 3.10+
- Standard C library (host)
- `arm-none-eabi-gcc` and Python 3 if you want to build the sample/test firmware

### Build the Emulator

```bash
./build.sh
```

This builds the `picoemu` executable in the project root.

You can also build explicitly with CMake:

```bash
cmake -S . -B build
cmake --build build -j
```

### WebAssembly Build (Browser)

See `docs/WASM.md` for full guide. Quick start:

```bash
./emsdk/emsdk_env.sh
./build_wasm.sh          # emcc -O3 -msimd128 web/picoemu.wasm.{js,wasm} 156K
python3 -m http.server 8080 --directory web  # http://localhost:8080
# Preset firmware: hello_world, gpio_test, timer_test, interrupt_test, name_prompt, littleos (M0), littleos_pico2 (M33), littleos_pico2_riscv (RV32) in web/
```

Browser UI `web/index.html` provides drag-drop UF2/ELF, serial monitor, GPIO 0-29 viewer, core PC/SP/halted/MIPS, clock select, and `PicoemuModule({print,printErr:console.log})` to avoid red console.

### Choose Core Mode

Pico-emu builds with dual-core support enabled by default. Select the active cores at runtime:

```bash
./picoemu firmware.uf2 -cores 1
./picoemu firmware.uf2 -cores 2
./picoemu firmware.uf2 -cores auto
```

### Build Test Firmware

In-tree guests are prebuilt (`web/*.uf2`). To rebuild from source:

```bash
cd test-firmware
chmod +x build.sh
./build.sh hello_world   # hello_world, gpio, timer, interrupt, name_prompt, ...
./build.sh all           # every guest incl. RV32 + eth_dhcp/eth_http(_6300)
```

Peripheral demos: `gpio` (LED 25), `timer` (alarm IRQs), `uart_echo`/`name_prompt` (stdin), `spi`/`i2c`/`pwm`/`adc`/`dma`/`pio`/`usb`/`rtc`/`clocks`/`psm`/`fp`/`ws2812`; networking: `eth_dhcp`/`eth_http` (+`6300` W6300 variants, all three arches); wireless: `wifi_*`, `ble_adv`, `ble_gatt`.

### Run

**UF2 Firmware:**

```bash
./picoemu web/hello_world.uf2
./picoemu web/gpio_test.uf2
./picoemu web/timer_test.uf2
./picoemu web/interrupt_test.uf2
./picoemu web/name_prompt.uf2 -stdin
printf 'Ada\n' | ./picoemu web/name_prompt.uf2 -stdin
```

**ELF Firmware** (auto-detected by extension):

```bash
./picoemu firmware.elf
```

### Run Tests

```bash
ctest --test-dir build --output-on-failure
```

### Debug Modes

Pico-emu now supports flexible debug output modes:

**Single-Core CPU Step Tracing** (verbose CPU and peripheral logging):
```bash
./picoemu -debug web/timer_test.uf2
```

**Assembly Instruction Tracing** (detailed POP/BX/branch operations):
```bash
./picoemu -asm web/timer_test.uf2
```

**Combined Debug + Assembly Tracing:**
```bash
./picoemu -debug -asm web/timer_test.uf2
```

**No Debug Output:**
```bash
./picoemu web/hello_world.uf2
```

**Dual-Core Specific:**
```bash
./picoemu firmware.uf2 -debug           # Core 0 debug output
./picoemu firmware.uf2 -debug -debug1   # Both cores debug
./picoemu firmware.uf2 -status          # Periodic status updates
./picoemu firmware.uf2 -stdin           # stdin to USB CDC when active, else UART0
./picoemu firmware.uf2 -gdb 4444        # GDB server on custom port
./picoemu firmware.uf2 -clock 125       # Real RP2040 timing (125 MHz)
./picoemu firmware.uf2 -flash fs.bin    # Persistent flash storage
./picoemu firmware.uf2 -debug-mem       # Log unmapped peripheral access
./picoemu firmware.uf2 -jit             # JIT for hot flash/ROM loops
./picoemu firmware.uf2 -cores 2 -thread-quantum 128  # Threaded timeslice
```

**RP2350 Modes** (`-arch m0+` default; `rv32` = Hazard3 RISC-V, `m33` = Cortex-M33):

```bash
./picoemu firmware_rv.uf2 -arch rv32        # explicit RV32
./picoemu pico2_rv_firmware.uf2             # auto-detected from UF2 family ID / picobin
./picoemu firmware_m33.uf2 -arch m33 -clock 150 -flash m33_flash.bin -stdin
```

**Networking (UART-to-TCP bridge):**

```bash
# Bridge UART0 to TCP port (connect with nc, minicom, etc.)
./picoemu firmware.uf2 -net-uart0 9999 -stdin
# In another terminal: nc localhost 9999

# Connect UART0 to a remote host
./picoemu firmware.uf2 -net-uart0-connect 192.168.1.10:9999
```

**Multi-Device Wiring (inter-instance communication):**

```bash
# Terminal 1: Instance A with UART0 wired via Unix socket
./picoemu fw_sensor.uf2 -wire-uart0 /tmp/uart_link.sock -stdin

# Terminal 2: Instance B with UART0 wired to the same socket
./picoemu fw_controller.uf2 -wire-uart0 /tmp/uart_link.sock -stdin

# UART TX on either side arrives as UART RX on the other
# GPIO pins can also be wired: -wire-gpio /tmp/gpio_link.sock
```

**WiFi (Pico W / CYW43):**

```bash
# Basic Pico W/CYW43 emulation
./picoemu firmware.uf2 -wifi

# Bridge emulated WLAN frames to a host TAP interface
./picoemu firmware.uf2 -wifi -tap tap0
```

**Virtual Network (Internet Bridge + Mesh):**

```bash
# Internet bridge (auto TAP + NAT, may sudo) and peer mesh
./picoemu firmware.uf2 -net -stdin
./picoemu fw1.uf2 -net-peer /tmp/vnet.sock -stdin   # Terminal 1
./picoemu fw2.uf2 -net-peer /tmp/vnet.sock -stdin   # Terminal 2

# Offload live sockets (real host TCP/UDP; W6300 incl. IPv6 loopback)
./picoemu w5500_firmware.uf2 -net -net-live -stdin
./picoemu w6300_firmware.uf2 -net -net-live6300 -stdin

# Boards (separate SPI hardware, off unless requested)
./picoemu w5500_firmware.uf2 -board pico-eth -stdin               # stub
./picoemu w5500_firmware.uf2 -board pico-eth -board-live -stdin   # live
./picoemu w5500_firmware.uf2 -board pico-eth -board-spi 1 -stdin  # SPI1
./picoemu w5500_firmware.uf2 -board pico-eth2 -stdin              # Pico2 label
./picoemu w6300_firmware.uf2 -board pico-w6300 -stdin             # stub
./picoemu w6300_firmware.uf2 -board pico-w6300 -board6300-live -stdin  # live
./picoemu w6300_firmware.uf2 -board pico-w6300-2 -stdin           # Pico2 label

# In-tree guests (prebuilt in web/; M33 = _pico2 + pico-eth2/pico-w6300-2,
# RV32 = _rv32 + -arch rv32). Terminal 1 = guest, terminal 2 = peer:
./picoemu web/eth_dhcp.uf2 -board pico-eth -net-peer /tmp/eth.sock -clock 125
python3 test-firmware/dhcp_peer_test.py /tmp/eth.sock  # ALL DHCP CHECKS PASSED
./picoemu web/eth_http.uf2 -board pico-eth -net-peer /tmp/eth.sock -clock 125
python3 test-firmware/http_peer_test.py /tmp/eth.sock  # ALL HTTP CHECKS PASSED
./picoemu web/eth_dhcp6300.uf2 -board pico-w6300 -net-peer /tmp/eth.sock -clock 125
python3 test-firmware/dhcp_peer_test.py /tmp/eth.sock  # ALL DHCP CHECKS PASSED
./picoemu web/eth_http6300.uf2 -board pico-w6300 -net-peer /tmp/eth.sock -clock 125
python3 test-firmware/http_peer_test.py /tmp/eth.sock  # eth_http6300_common table
# Same DORA via the Go gateway (openhw-studio-gateway running):
./picoemu web/eth_dhcp.uf2 -board pico-eth -net -net-peer /tmp/gw.sock
python3 web/gateway_bridge.py --sock /tmp/gw.sock --room lab  # lease .2

# Wire Ethernet frames between instances
./picoemu fw_sensor.uf2 -wire-eth /tmp/mesh.sock -stdin
./picoemu fw_ctrl.uf2 -wire-eth /tmp/mesh.sock -stdin
```

**Software-Defined Devices (SDD):**

```bash
# Attach a TMP102 thermometer on I2C0 at 0x48
./picoemu firmware.uf2 -sdd thermometer

# Custom temperature, bus, and address
./picoemu firmware.uf2 -sdd thermometer:temp=37.5,i2c=1,addr=0x49
```

**I2C EEPROM** (24LC256, 32KB, addr `0x50`):
```bash
./picoemu firmware.uf2 -sdd eeprom
./picoemu firmware.uf2 -sdd eeprom:i2c=1,addr=0x51,file=eeprom.bin

# Combine with mesh networking
./picoemu fw_sensor.uf2 -wire-eth /tmp/mesh.sock -sdd thermometer:temp=42
```

**JS mirrors** (observation channels for external simulators):
```bash
# I2C slave that ACKs 0x3C and mirrors every transaction
# (works for DW-controller AND GPIO bit-bang masters, so
# MicroPython machine.I2C.scan() sees it)
./picoemu firmware.uf2 -sdd jsmirror:i2c=0,addr=0x3c
./picoemu firmware.uf2 -sdd jsmirror:i2c=0,addr=0x3c,addr=0x27

# SPI slave: MOSI observe + MISO inject (replaces sdcard/emmc on the bus)
./picoemu firmware.uf2 -sdd spimirror:spi=0
```

**Storage Devices (SD Card / eMMC):**

```bash
# Attach a 32MB SD card image on SPI1 (default)
./picoemu firmware.uf2 -sdcard sdcard.img -sdcard-size 32

# Attach SD card on SPI0 instead
./picoemu firmware.uf2 -sdcard sdcard.img -sdcard-spi 0

# Attach a 64MB eMMC image on SPI0 (default)
./picoemu firmware.uf2 -emmc emmc.img -emmc-size 64

# Attach eMMC on SPI1 instead
./picoemu firmware.uf2 -emmc emmc.img -emmc-spi 1

# Combine with flash persistence and MicroPython
./picoemu python/micropython.uf2 -stdin -clock 125 -flash mpy.bin -sdcard sd.img
```

**MicroPython REPL** (`web/micropython_rp2040.uf2`, `web/micropython_rp2350.uf2`):

Bundled v1.22.1 boots to `>>>` over USB CDC and evaluates (`print(6*7)` → `42`), native and in-browser (`node test-wasm.js` asserts both).

```bash
./picoemu web/micropython_rp2040.uf2 -stdin -clock 125 -flash mpy.bin
```

Output:
```
MicroPython v1.22.1 on 2024-01-05; Raspberry Pi Pico with RP2040
Type "help()" for more information.
>>>
```

**GDB Remote Debugging:**
```bash
# Terminal 1: Start emulator with GDB server
./picoemu firmware.uf2 -gdb            # default :3333
./picoemu firmware.uf2 -gdb 4444       # custom port

# Terminal 2: Connect GDB
arm-none-eabi-gdb firmware.elf -ex "target remote :3333"
```

## Project Structure

```
Pico-emu/
├── src/
│   ├── main.c / picoemu_wasm.c  # Native CLI / WASM embedding entry points
│   ├── cpu.c / instructions.c / thumb32.c  # M0+ + M33 Thumb engines
│   ├── rp2350_arm/m33_cpu.c     # Cortex-M33 overlay (BASEPRI, VFP/DCP, DSP/MVE)
│   ├── rp2350_rv/               # Hazard3 RV32: rv_cpu, rv_clint, rv_membus,
│   │                            #   rv_bootrom, rp2350_periph, picobin (+Zfinx)
│   ├── membus.c / uf2.c / elf.c # Bus routing, firmware loaders
│   ├── gpio/timer/uart/spi/i2c/pwm/adc/dma/pio/nvic/clocks/usb/rtc/rom.c
│   ├── cyw43.c / w5500.c / w6300.c / bme280.c  # Device models
│   ├── vnet/tapif/netbridge/wire.c  # Virtual net bus, TAP, bridges, mesh
│   ├── sdd*.c / storage/sdcard/emmc/fatfs/fuse_mount.c
│   ├── gdb.c / devtools.c / corepool.c
│   └── wasm_net.c / fuse_mount_wasm.c  # Browser shims
├── include/ (+ rp2350_arm/ + rp2350_rv/)  # Per-module register definitions
├── tests/test_suite.c           # 507 unit tests (CTest integrated)
├── test-firmware/               # .S guests + gen_*.py + *_peer_test.py + sweep_all.sh
├── web/                         # Browser bench (index/docs/about), cli.js,
│                                #   picoemu.wasm.*, prebuilt *.uf2, net/hci bridges
├── openhw-studio-gateway/       # Go gateway: DHCP, RA/NA/echo, NAT64/DNS64
├── docs/ (GATEWAY/GPIO/NETWORKING/NVIC_audit/PICOEMU/ROADMAP/WASM/audit_report)
├── CMakeLists.txt / build.sh / build_wasm.sh / build_wasm_threads.sh
├── CHANGELOG.md / LICENSE / README.md
```

## Peripheral Notes

- **Timer** (`0x40054000`): 64-bit counter, ALARM0-3 + NVIC IRQ 0-3, W1C INTR/INTE/INTS, ARMED. Try `timer_test.uf2`.
- **GPIO** (`0x40014000`, SIO `0xD0000000`): pins + function select + SIO atomics + edge/level IRQs (48 pins on RP2350). See [docs/GPIO.md](docs/GPIO.md). Try `gpio_test.uf2` (LED 25).
- **Dual-core**: independent PCs/SPs/register sets; shared flash + per-core RAM + shared RAM; FIFOs, 32 spinlocks, SIO atomics. `-cores 1|2`, `-status`, `-thread-quantum N`.

## Technical Implementation

### Memory Map

- **Flash (XIP)**: `0x10000000`+ (2MB RP2040 / 4MB RP2350, aliases `0x11/0x12/0x13`)
- **SRAM**: `0x20000000`+ (264KB RP2040 / 520KB RP2350, mirror `0x21000000`)
- **ROM**: `0x00000000` (16KB RP2040 / 32KB RP2350, function table + soft-float/double)
- **Peripherals**: `0x40000000`+ APB, `0xD0000000` SIO, `0xE000E000` SysTick/NVIC, RP2350 CLINT
- **XIP cache**: `0x14000000` CTRL/FLUSH/STAT + 16KB XIP SRAM `0x15000000`

### Timing Model

Configurable clock (`-clock 1` fast-forward default, `-clock 125/150` real silicon):
per-instruction cycle costs feed a cycle accumulator for the timer; SysTick counts raw CPU
cycles per ARM spec. I/O split: firmware output on stdout, diagnostics on stderr.

### Instruction Dispatch

256-entry O(1) table on `instr >> 8` (32-bit BL/MSR/MRS/barriers pre-decoded, secondary
dispatchers for shared top bytes); `pc_updated` flag controls auto-advance. Full APSR
NZCV via shared add/sub flag helpers.

### UF2 Loading

Validates magic (`0x0A324655/0x9E5D5157/0x0AB16F30`), flash range, 476B payload bound,
overflow-safe targets; multi-block supported, malformed blocks rejected cleanly.

### Dual-Core Architecture

Both cores step independently with synchronized shared state; SIO FIFOs + 32 spinlocks
for messaging/locking (`-cores 1|2`, `-status`, `-thread-quantum N`).

## Performance

Pico-emu now ships with a 64K decoded instruction cache enabled by default and optional JIT basic-block compilation via `-jit`.

- **Instruction cache**: Avoids repeat decode/dispatch work for hot Thumb-1 paths.
- **JIT**: Compiles hot flash/ROM basic blocks and reports execution stats on exit.
- **Threaded execution**: `-cores 2` and `-cores auto` map emulated cores to host pthreads while preserving a shared-state lock.
- **I/O behavior**: Firmware output stays on stdout while emulator diagnostics stay on stderr, which keeps pipes and scripted runs predictable.

Measured on a 16-CPU Linux x86-64 host (`./build/picoemu_bench`, 4.2M-instruction synthetic loop, best of 3):

| Build | Throughput | Notes |
|-------|-----------|-------|
| Native, ICache only | 85.9 MIPS | default |
| Native, ICache + JIT (`-jit`) | 147.6 MIPS | 1.72x over ICache |
| WASM in Node 22 (`littleos.uf2`, real firmware + peripherals) | 22–25 MIPS | `node test-wasm.js`; JIT slower in WASM (22.1 vs 25.4, leave off) |

For context, the improved pure-JS fork [c1570/rp2040js](https://github.com/c1570/rp2040js) reports ~70M cycles/s on recent PCs. Cycles are not instructions (Thumb averages >1 cycle/instr), so the figures are not directly comparable — but Pico-emu native is in the same league or faster on CPU-bound loops, while the browser build trades raw speed for the full peripheral set (USB, VNet, SD/eMMC, GDB) that pure-JS emulators lack. Browser frame budget is `500k` instructions/frame (~29ms at 17 MIPS); full 125MHz realtime would need ~80+ MIPS, so heavy firmware runs at ~1/5 realtime in the tab.

For benchmarking details, see `tests/benchmark.c`.

## Future Work

1. **Timing fidelity**: DMA pacing, high-speed PIO timing, more USB edge cases.
2. **Device breadth**: more SPI/I2C SDD models.
3. **Networking depth**: DHCP server in vnet, mDNS relay, packet capture/replay.

## Contributing

PRs welcome: firmware coverage, device models, docs. Verify with
`ctest --test-dir build --output-on-failure`. History: [CHANGELOG.md](CHANGELOG.md);
deep dives: [docs/](docs/).

## License

## License

MIT License - See LICENSE file for details

## Contact & Support

For issues, questions, or contributions:
- Open an issue on GitHub
- Check existing documentation in [docs/](docs/)
- Review [CHANGELOG.md](CHANGELOG.md) for recent changes
- See [NVIC Audit Report](docs/NVIC_audit_report.md) for historical background on early NVIC issues
