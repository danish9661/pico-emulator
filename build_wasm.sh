#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

source "$SCRIPT_DIR/../emsdk/emsdk_env.sh" 2>/dev/null || {
  export EMSDK="$SCRIPT_DIR/../emsdk"
  export PATH="$EMSDK:$EMSDK/upstream/emscripten:$PATH"
}

echo "=== Pico-emu WASM Build ==="
echo "Emscripten: $(emcc --version | head -1)"

# Pinned toolchain (pico-emu.md contract): the .wasm rebuild is only
# reproducible with this exact Emscripten. Mismatched toolchains have
# silently changed codegen before; fail loudly instead.
PINNED_EMSDK="$(cat "$SCRIPT_DIR/emsdk-version" 2>/dev/null | tr -d ' \t\r\n')"
if [ -z "$PINNED_EMSDK" ]; then
  echo "error: emsdk-version file missing" >&2
  exit 1
fi
if ! emcc --version 2>/dev/null | head -1 | grep -q "$PINNED_EMSDK"; then
  echo "error: emsdk $PINNED_EMSDK required (see emsdk-version), got: $(emcc --version 2>/dev/null | head -1)" >&2
  echo "  fix: emsdk install $PINNED_EMSDK && emsdk activate $PINNED_EMSDK" >&2
  exit 1
fi

mkdir -p web

COMMON_FLAGS="-O3 -msimd128 -Wall -Wno-macro-redefined -Wno-logical-not-parentheses -Wno-format"
INCLUDES="-Iinclude/ -Iinclude/rp2350_rv -Iinclude/rp2350_arm"

SOURCES=(
  src/picoemu_wasm.c src/fuse_mount_wasm.c src/wasm_net.c
  src/cpu.c src/instructions.c src/thumb32.c src/membus.c
  src/uf2.c src/elf.c src/gpio.c src/timer.c src/uart.c
  src/spi.c src/i2c.c src/pwm.c src/adc.c src/dma.c
  src/pio.c src/nvic.c src/clocks.c src/usb.c src/rtc.c
  src/rom.c src/gdb.c src/storage.c src/sdcard.c src/emmc.c
  src/fatfs.c src/w5500.c src/w6300.c src/bme280.c src/cyw43.c
  src/devtools.c src/vnet.c src/sdd.c src/sdd_thermo.c src/sdd_eeprom.c
  src/sdd_jsmirror.c src/i2c_bitbang.c src/sdd_spimirror.c
  src/rp2350_rv/rv_cpu.c src/rp2350_rv/rv_clint.c
  src/rp2350_rv/rv_membus.c src/rp2350_rv/rv_bootrom.c
  src/rp2350_rv/rp2350_periph.c src/rp2350_rv/picobin.c
  src/rp2350_arm/m33_cpu.c
)

EXPORTS='[
  "_picoemu_init","_picoemu_reset",
  "_picoemu_load_uf2","_picoemu_load_elf",
  "_picoemu_step","_picoemu_set_clock",
  "_picoemu_read_uart","_picoemu_read_uart_bulk","_picoemu_write_uart",
  "_picoemu_write_uart_port","_picoemu_write_usb",
  "_picoemu_uart_active","_picoemu_usb_active",
  "_picoemu_get_gpio","_picoemu_get_gpio_raw","_picoemu_get_gpio_out","_picoemu_get_gpio_oe","_picoemu_set_gpio",
  "_picoemu_mem_read32","_picoemu_mem_write32",
  "_picoemu_is_halted","_picoemu_get_core_state",
  "_picoemu_usb_state32",
  "_picoemu_get_flash_ptr","_picoemu_get_sram_ptr",
  "_picoemu_set_cores","_picoemu_get_cores","_picoemu_set_quantum",
  "_picoemu_set_jit","_picoemu_set_debug","_picoemu_set_semihosting",
  "_picoemu_flash_save","_picoemu_flash_load","_picoemu_flash_write",
  "_picoemu_sdcard_load","_picoemu_emmc_load",
  "_picoemu_net_enable","_picoemu_net_enable6300","_picoemu_sdd_add",  "_picoemu_eth_push_rx",
  "_picoemu_jsmirror_pending","_picoemu_jsmirror_drops","_picoemu_jsmirror_pop",
  "_picoemu_spimirror_pending","_picoemu_spimirror_drops","_picoemu_spimirror_pop",
  "_picoemu_spimirror_inject",
  "_picoemu_adc_set","_picoemu_adc_get","_picoemu_pwm_read","_picoemu_cycle_count",
  "_picoemu_pio_tx_push","_picoemu_pio_rx_pop","_picoemu_pio_state",
  "_picoemu_eth_pop_tx","_picoemu_eth_set_uplink","_picoemu_wifi_enable","_picoemu_board_eth",
  "_picoemu_bt_hci_enable","_picoemu_bt_hci_pop_tx","_picoemu_bt_hci_push_rx",
  "_picoemu_w5500_push_rx","_picoemu_w5500_push_status","_picoemu_ws_send_w5500",
  "_picoemu_w5500_pop_tx","_picoemu_w5500_tx_len","_picoemu_w5500_gw_enable",
  "_picoemu_w6300_push_rx","_picoemu_w6300_push_status",
  "_picoemu_w6300_pop_tx","_picoemu_w6300_tx_len","_picoemu_w6300_gw_enable",
  "_picoemu_board_eth6300",
  "_picoemu_coverage_start","_picoemu_coverage_dump",
  "_picoemu_trace_start","_picoemu_trace_stop",
  "_picoemu_hotspots_start","_picoemu_hotspots_report",
  "_picoemu_profile_start","_picoemu_profile_dump",
  "_picoemu_callgraph_start","_picoemu_callgraph_dump",
  "_picoemu_gpiotrace_start","_picoemu_gpiotrace_stop",
  "_picoemu_irqlat_start","_picoemu_irqlat_report",
  "_picoemu_stackcheck_start","_picoemu_stackcheck_report",
  "_picoemu_symbols_load","_picoemu_watch_add","_picoemu_fault_add",
  "_picoemu_script_load","_picoemu_expect_start","_picoemu_expect_check",
  "_picoemu_heatmap_start","_picoemu_heatmap_dump","_picoemu_set_buslog",
  "_picoemu_gdb_enable","_picoemu_gdb_is_hit","_picoemu_gdb_hit_core",
  "_picoemu_gdb_break","_picoemu_gdb_poll",
  "_picoemu_gdb_push_rx","_picoemu_gdb_pop_tx","_picoemu_gdb_tx_len",
  "_picoemu_gdb_start","_picoemu_gdb_stop","_picoemu_gdb_notify_stop",
  "_picoemu_net_set_connected","_picoemu_net_push_rx","_picoemu_net_pop_rx",
  "_picoemu_wire_set_connected","_picoemu_wire_push_rx",
  "_picoemu_tap_push_rx",
  "_picoemu_get_gpio_out","_picoemu_get_gpio_oe",
  "_picoemu_w5500_dev_push_rx",
  "_picoemu_w6300_dev_push_rx",
  "_fuse_mount_start","_fuse_mount_stop","_fuse_mount_active",
  "_flash_persist_set_path","_flash_persist_open","_flash_persist_save_all",
  "_free","_malloc"
]'

echo "Compiling ${#SOURCES[@]} source files..."

emcc \
  "${SOURCES[@]}" \
  $INCLUDES $COMMON_FLAGS \
  -s WASM=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s MODULARIZE=1 \
  -s EXPORT_NAME="PicoemuModule" \
  -s EXPORTED_FUNCTIONS="$EXPORTS" \
  -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","getValue","setValue","HEAPU8","HEAPU32","HEAP8"]' \
  -s INITIAL_MEMORY=67108864 \
  -s MAXIMUM_MEMORY=268435456 \
  -s STACK_SIZE=1048576 \
  -s NO_EXIT_RUNTIME=1 \
  -s ENVIRONMENT='web,node' \
  -s EXPORT_ES6=1 \
  -o web/picoemu.wasm.js

echo ""
echo "Build complete!"
echo "  WASM: $(du -h web/picoemu.wasm.wasm | cut -f1)"
echo "  JS:   $(du -h web/picoemu.wasm.js | cut -f1)"
echo ""
echo "To test: python3 -m http.server 8080 --directory web"
echo "  Then open http://localhost:8080"