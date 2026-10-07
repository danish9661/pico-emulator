# picoemu — usage, implementation & API reference

`picoemu` (npm) is the prebuilt WebAssembly distribution of the Pico-emu
RP2040/RP2350 emulator: three CPU cores (Cortex-M0+, Cortex-M33, RISC-V
Hazard3 RV32IMAC), full peripherals, a browser UI, a Node CLI, and 40+
demo UF2 images. Emulation core: MIT port of
[Night-Traders-Dev/Bramble](https://github.com/Night-Traders-Dev/Bramble).

## 1. Install & quick start

```sh
npm i pico-emu
npx pico-emu hello_world.uf2                 # arch auto-detected from UF2
npx pico-emu uart_echo_rv32.uf2              # type a line, Enter submits
```

CLI options: `--arch auto|m0|m33|rv32` (default `auto` via UF2 family ID
`0xE48BFF56/59/5A`), `--clock 125`, `--steps 2000000`, `--timeout 30`,
`--cores 2`. Terminal input → firmware UART; firmware output → stdout;
Ctrl-C quits.

## 2. Browser UI

Serve the package directory over HTTP and open `index.html` (file:// will
not load the `.wasm`). Three demo dropdowns — RP2040 / M33 / RV32 — load
presets via `loadPreset(name)`; `Run` steps the emulator in
`requestAnimationFrame` chunks; serial monitor drains UART0; GPIO viewer
polls `picoemu_get_gpio*`; `Send` writes the input box + CR(13).

## 3. Embedding in JS

```js
import createEmu from 'pico-emu';            // resolves picoemu.wasm.js
import fs from 'fs';

const mod = await createEmu({ print: () => {}, printErr: () => {} });
mod._picoemu_init(1);                       // 0=M0+ 1=RV32 2=M33
mod._picoemu_set_clock(125);
const uf2 = new Uint8Array(fs.readFileSync('hello_rv32.uf2'));
const ptr = mod._malloc(uf2.length);
mod.HEAPU8.set(uf2, ptr);
mod._picoemu_load_uf2(ptr, uf2.length);     // returns nonzero on success
mod._free(ptr);
mod._picoemu_reset();

mod._picoemu_step(200000);                  // run N instructions
let ch, out = '';
while ((ch = mod._picoemu_read_uart(0)) !== -1) out += String.fromCharCode(ch);
mod._picoemu_write_uart(65);                // one byte into the firmware
```

Step in a loop (e.g. 2.5M instructions/frame at 60 fps ≈ 150 MHz
real-time). Drain UART once per frame, not per step. `HEAPU8`/`HEAPU32`
give zero-copy memory access.

## 4. API reference (all `picoemu_*` exports)

### Lifecycle / image loading

| Export | Signature | Notes |
|---|---|---|
| `picoemu_init` | `int (int arch)` | `0`=M0+ `1`=RV32 `2`=M33. Resets everything. |
| `picoemu_reset` | `void ()` | Re-reset current firmware. |
| `picoemu_load_uf2` | `int (ptr, len)` | Parses UF2 blocks into flash. Returns nonzero OK. |
| `picoemu_load_elf` | `int (ptr, len)` | Same for ELF images. |
| `picoemu_set_clock` | `void (int mhz)` | e.g. `125`. Drives timer/CLINT rates. |

### Run control / inspection

| Export | Signature | Notes |
|---|---|---|
| `picoemu_step` | `int (int n)` | Execute up to N instructions; returns count run. Stops early on halt/GDB hit. |
| `picoemu_is_halted` | `int ()` | Nonzero when hart0/core0 halted. |
| `picoemu_get_core_state` | `void (core, *pc, *sp)` | Pass pointers to two u32 slots; reads PC/SP of core/hart 0/1. |
| `picoemu_get_flash_ptr` | `uint8_t * ()` | Direct flash bytes (read via `HEAPU8`). |
| `picoemu_get_sram_ptr` | `uint8_t * ()` | Direct SRAM bytes (RV32 bus SRAM). |
| `picoemu_set_cores` | `void (int n)` | `1`/`2`. Gates hart1/core1 stepping + launch. Default 2. |
| `picoemu_get_cores` | `int ()` | Active core count. |
| `picoemu_set_quantum` | `void (int q)` | Cooperative step quantum. |
| `picoemu_set_jit` | `void (int on)` | Hot-block JIT (default off in WASM). |
| `picoemu_set_debug` | `void (int on, int core)` | Per-core instruction tracing. |
| `picoemu_set_semihosting` | `void (int on)` | ARM semihosting SVC handling. |

### UART

| Export | Signature | Notes |
|---|---|---|
| `picoemu_read_uart` | `int ()` | Next RX byte, or `-1` if empty. Drain in a loop. |
| `picoemu_read_uart_bulk` | `int (*dest, max)` | Bulk drain into a buffer. |
| `picoemu_write_uart` | `void (int ch)` | One byte into firmware RX. CR(13) submits shell lines; USB CDC firmware (MicroPython) also takes raw CR. |

### GPIO

| Export | Signature | Notes |
|---|---|---|
| `picoemu_get_gpio` | `int (pin)` | Effective level (output if driven, else input). |
| `picoemu_get_gpio_raw` | `int (pin)` | Raw input level. |
| `picoemu_get_gpio_out` | `uint32_t ()` | Output latch bitmask. |
| `picoemu_get_gpio_oe` | `uint32_t ()` | Output-enable bitmask. |
| `picoemu_set_gpio` | `void (pin, val)` | Drive an input pin (buttons/sensors). |

### Memory

| Export | Signature | Notes |
|---|---|---|
| `picoemu_mem_read32` | `uint32_t (addr)` | Bus-accurate read (peripherals included). |
| `picoemu_mem_write32` | `void (addr, val)` | Bus-accurate write. |

Handy bases: UART0 `0x40070000` (RP2350) / `0x40034000` (RP2040),
SIO `0xD0000000`, CLINT `0xD0000100` (RV32: MTIME `+0x00`,
MTIMECMP0 `+0x08`), TIMER0 `0x400B0000`, SRAM `0x20000000` (520 KB
RP2350), flash `0x10000000`.

### Observer taps (simulator hooks)

| Export | Signature | Notes |
|---|---|---|
| `picoemu_sdd_add` | `int (argstr)` | Attach `thermometer:`, `eeprom:`, `jsmirror:i2c=0,addr=0x3c`, `spimirror:spi=0`. Resets registry per call. |
| `picoemu_jsmirror_pending/drops` | `int ()` | Queued I2C ring entries / drop counter. |
| `picoemu_jsmirror_pop` | `int (*out, max)` | Drain ring: `0x100\|addr` START, bytes, `0x200` STOP. |
| `picoemu_spimirror_pending/drops/pop` | `int…` | Same for SPI: `0x100\|spi` CS assert, `0x200\|spi` deassert, MOSI bytes. |
| `picoemu_spimirror_inject` | `int (data, len)` | Queue MISO reply bytes (0xFF when empty). |
| `picoemu_adc_set/get` | `void/int (ch, raw12)` | Per-channel 12-bit inject/readback (`raw = mv*4095/3300`). |
| `picoemu_pwm_read` | `int (slice, *hz, *dutyA, *dutyB, *en)` | Freq + duty/10000 + enable; any out-param nullable. |
| `picoemu_pio_tx_push` | `int (block, sm, word)` | Feed a PIO program (I2S-out samples, pixels); `-1` on full. |
| `picoemu_pio_rx_pop` | `int (block, sm, *word)` | Observe PIO output (I2S-in samples); `-1` on empty. |
| `picoemu_pio_state` | `int (block, sm, *pc, *tx, *rx, *stalled)` | SM readback. |
| `picoemu_cycle_count` | `double ()` | Sim-time stamp (cycle counter) for edge stamps/pace. |

### Storage / SD / flash

| Export | Notes |
|---|---|
| `picoemu_flash_save/load` | Persist flash to browser storage. |
| `picoemu_flash_write(data, len, offset)` | Patch flash bytes. |
| `picoemu_sdcard_load(data, len, spi)` / `picoemu_emmc_load` | Attach file-backed block devices. |

### Network / GDB / devtools

`picoemu_net_enable`, `picoemu_sdd_add`, `picoemu_board_eth(on, live, spi)`
(pico-eth board: WIZnet W5500-EVB-Pico on SPI0, CSn=GPIO17, RSTn=GPIO20,
INTn=GPIO21; off by default), `picoemu_board_eth6300(on, live, spi)`
(pico-w6300 board: WIZnet W6300-EVB-Pico on SPI0 QSPI-single, CSn=GPIO16,
RSTn=GPIO22, INTn=GPIO15; off by default), `picoemu_eth_push_rx`,
`picoemu_eth_pop_tx`, `picoemu_eth_set_uplink` (raw-ETH gateway path,
see `docs/GATEWAY.md`), `picoemu_w5500_push_rx/status` (targets the board
when it is on, else the legacy `-net-live` device), `picoemu_ws_send_w5500` (WebSocket
bridges, pumped each frame by the UI); `picoemu_gdb_enable/is_hit/
hit_core/break` (non-blocking RSP for the UI GDB panel);
`picoemu_coverage_*`, `picoemu_trace_*`, `picoemu_hotspots_*`,
`picoemu_profile_*`, `picoemu_callgraph_*`, `picoemu_gpiotrace_*`
(VCD), `picoemu_irqlat_*`, `picoemu_stackcheck_*`,
`picoemu_symbols_load`, `picoemu_watch_add`, `picoemu_fault_add`,
`picoemu_script_load`, `picoemu_expect_*`, `picoemu_heatmap_*`,
`picoemu_set_buslog`. These mirror the native devtools; the UI's
Devtools panel drives them.

## 5. Implementation notes

- `src/picoemu_wasm.c` replaces the CLI (`main.c`) with the exports
  above; peripherals/CPUs are shared with native (`src/`, `src/rp2350_rv/`,
  `src/rp2350_arm/`). WASM built by `build_wasm.sh` (Emscripten,
  MODULARIZE, 64–256 MB linear memory).
- RV32 demos (`test-firmware/*_rv32.S`) are generated by
   `test-firmware/gen_rv32.py` (position-independent rv32ima, auipc string
   refs) and linked by `test-firmware/rvlink.py` (handles
   R_JAL/BRANCH/HI20/LO12/PCREL) + `uf2conv.py` (family `0xE48BFF5A`).
   Rebuild: `python3 test-firmware/gen_rv32.py`, assemble with
   `clang --target=riscv32-unknown-elf -march=rv32ima -mabi=ilp32`,
   link, convert. ARM in-tree guests (`test-firmware/gen_eth_dhcp.py`,
   `gen_eth_http.py`, `gen_ble_arm.py`) assemble with clang
   (`armv6m`/`armv8-m.main`) and link via `test-firmware/build_eth.py`
   (`--gen/--gen-http/--gen-ble` + `m0/m33/rv32/http-*/ble-*` targets).
   Native sweep: `./test-firmware/sweep_all.sh build`
   (60 firmware: 18 M0+ + 12 M33 + 18 RV32 + 6 WiFi + 6 eth, all archs).
- Dual-core: ARM uses host-threaded stepping with WFI fast-forward;
  RV32 harts step cooperatively; hart 1 launches via the SIO mailbox
  (`0xD00001C0` entry / `0x1C4` SP / `0x1C8` arg / `0x1CC` launch).
  See `dualcore_rv32` demo.
- littleOS Sage eval verified on M33 and RV32 (`sage print(6*7)` →
  `42.0000`); RV32 `health` temperature reads wrong on the vintage
  `littleos_pico2_riscv.uf2` (stale firmware, not an emulation bug).
- Performance: native ~86 MIPS (ICache) / ~148 MIPS (JIT); WASM
  ~22–25 MIPS. Budget ~2.5M steps/frame for 60 fps UI.

## 6. Building from source

Native: `cmake -S . -B build && cmake --build build -j && ctest
--test-dir build` (507 tests). WASM: `./build_wasm.sh` (needs emsdk;
output to `web/picoemu.wasm.*`). Publish flow: manual
`.github/workflows/publish.yml` (branch + version + description →
npmjs `pico-emu` + GPR `@danish9661/pico-emu`).

## 7. Porting matrix (native → WASM)

Every emulation source is in the WASM build. Host-OS-bound modules are
replaced by shims with the same API:

| Native | WASM | Notes |
|---|---|---|
| all CPUs + peripherals (`cpu`, `thumb32`, `membus`, `gpio`, `timer`, `uart`, `spi`, `i2c`, `pwm`, `adc`, `dma`, `pio`, `nvic`, `clocks`, `usb`, `rtc`, `rom`, `gdb`, `storage`, `sdcard`, `emmc`, `fatfs`, `w5500`, `bme280`, `cyw43`, `devtools`, `vnet`, `sdd*`, `rv_*`, `m33_cpu`) | compiled as-is | bit-identical emulation |
| `corepool.c` (pthreads) | `wasm_net.c` cooperative pool | `picoemu_set_cores(1\|2)`; `picoemu.wasm.threads.*` (`-pthread`) for true workers via `serve_coop.py` |
| `netbridge.c` TCP | WebSocket bridge | `connectNet()` + `web/net_proxy.py` |
| `wire.c` unix sockets | `BroadcastChannel` | multi-tab mesh, same protocol |
| `tapif.c` TAP device | WebSocket proxy | fake fd + loopback, `net_proxy.py --ws` |
| `fuse_mount.c` FUSE | `fuse_mount_wasm.c` | OPFS/IDBFS persistent flash |
| `main.c` CLI | `picoemu_wasm.c` exports + `web/cli.js` (node) | full API in §4 |

WiFi (CYW43) is compiled in and hooked to PIO + polled in every loop,
but has no guest driver in-box: end-to-end WiFi needs Pico-SDK-based
firmware (provides the CYW43 stack) plus a live backend
(`-net-live` native, proxy in browser).

Wired Ethernet (W5500 + W6300) has three faces per chip: the legacy floating
`-net-live` / `-net-live6300` device (no board pins), the `pico-eth` /
`pico-w6300` board variant (`-board pico-eth|pico-w6300` native, Board row
in the browser bench, `--board pico-eth|pico-w6300` in `web/cli.js`):
W5500 on SPI0 with CSn=GPIO17, RSTn=GPIO20, INTn=GPIO21, VERSIONR `0x04`,
W1C socket IR, computed SIR; W6300 on SPI0 QSPI-single with CSn=GPIO16,
RSTn=GPIO22, INTn=GPIO15, CIDR `0x61/0x00/0x11`, CHIP/NET/PHY lock groups,
PHYSR opposite-polarity link bits, `Sn_IRCLR` W1C; the W6300 model covers
the full dual IPv4/IPv6 offload surface — socket modes TCP4/UDP4/IPRAW4/
MACRAW + TCP6/UDP6/IPRAW6 + dual-stack TCPD/UDPD, commands OPEN/LISTEN/
CONNECT/CONNECT6/DISCON/CLOSE/SEND/SEND_MAC/SEND_KEEP/RECV/SEND6, IPv6 net
registers (LLAR/GUAR/SUB6R/GA6R, SLDIP6R, UIP6R/UPORT6R), masked interrupt
chain (Sn_IMR/SIMR/SLIMR/IMR + IEN gate), RTR/RCR retry engine with
TIMEOUT, Sn_KPALVTR auto-keepalive, NETMR Wake-on-LAN magic-packet detect,
Sn_TTLR/Sn_TOSR/Sn_MSSR socket options, offload RECV commit with live
RX_RSR; live host sockets for IPv4 + IPv6 (loopback-verified);
and the **MACRAW gateway path** (socket 0 in `MR_MACRAW` joins the shared
vnet bus — same room/DHCP/NAT as CYW43 WiFi, see `docs/NETWORKING.md`).
`-board-live` dials real host sockets; the browser proxy path is shared
with the legacy device. Off by default (one flag test, zero cost).
`pico-eth2` is an alias for the RP2350-based WIZnet W5500-EVB-Pico2:
identical wiring/pins (only the SoC differs), so one model serves both.
`pico-w6300` / `pico-w6300-2` are the W6300-EVB-Pico/Pico2 pair (same
alias pattern). On M33/RV32 the RP2350 SPI bases (`0x40080000`/`0x40088000`)
route to the same SPI instances (`spi_match` is RP2350-aware, same as UART).

In-tree guests need no toolchain: `web/eth_dhcp{,_pico2,_rv32}.uf2`
(DORA → `ETH DONE`) and `web/eth_http{,_pico2,_rv32}.uf2` (DORA + ARP →
SYN → `GET /` → `200 hello-eth` → FIN → `ETH HTTP-DONE`), verified via
`test-firmware/dhcp_peer_test.py` / `http_peer_test.py` (per-arch
MAC/XID/sport, server SSEQ `0x00100000`). Sweep asserts the pre-DORA
markers (`ETH MACRAW-OK`) offline; W6300 in-tree guests
`web/eth_dhcp6300{,_pico2,_rv32}.uf2` (same DORA, QSPI-single + CIDR2 +
unlock + `Sn_MR=0x07`) and `web/eth_http6300{,_pico2,_rv32}.uf2`
sweep-locked the same way; Arduino-CLI `Wiznet5500lwIP` DHCP
(`test-firmware/arduino/ethdhcp/`) is the real-driver prove-out
(M0+/M33 in-tree DORA green via live peer; Arduino E2E post-OFFER stall is
guest-side RX pump, under test — see CHANGELOG); Arduino-CLI `W6300lwIP` DHCP
(`test-firmware/arduino/ethdhcp6300/`, vendored `ethdhcp6300_arduino{,_pico2}.uf2`)
is the W6300 real-driver prove-out — full DORA green on M0+ and M33 via the
PIO-QSPI bridge (driver DMA/PIO traffic snooped at the register layer, zero
cost when off). In-tree ARM `ble_adv{,_pico2}.uf2`
(RV32 `wifi_ble_adv_rv32.uf2` is the reference) reach `ARM BLE LISTEN`
(sweep-locked, like RV32).
