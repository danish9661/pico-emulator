#!/usr/bin/env node
// WASM M33 MicroPython boot test (full live path): boot the genuine
// RP2350-ARM MicroPython UF2 (M33, USB-CDC REPL) and assert the banner
// + >>> prompt bytes arrive via the serial monitor (USB CDC routes
// through putchar in WASM builds). Regression lock for the RP2350
// IO_QSPI routing + STATUS fix: without it the image spins forever
// in a QSPI-timing wait and prints nothing.
import PicoemuModule from './web/picoemu.wasm.js';
import fs from 'fs';

const mod = await PicoemuModule({ print: () => {}, printErr: () => {} });
let failures = 0;
const ok = (c, m) => { console.log((c ? 'ok' : 'FAIL') + ': ' + m); if (!c) failures++; };
function drain(cap = 8000) {
  let out = '', ch, n = 0;
  while ((ch = mod._picoemu_read_uart(0)) !== -1 && n++ < cap) out += String.fromCharCode(ch);
  return out;
}

mod._picoemu_init(2); // M33
mod._picoemu_set_clock(125);
{
  const uf2 = new Uint8Array(fs.readFileSync('./web/micropython_rp2350.uf2'));
  const ptr = mod._malloc(uf2.length);
  mod.HEAPU8.set(uf2, ptr);
  const b = mod._picoemu_load_uf2(ptr, uf2.length);
  mod._free(ptr);
  ok(b > 0, `MP UF2 loads (${b} blocks)`);
}
mod._picoemu_reset();
let total = 0, banner = '';
for (let i = 0; i < 200 && !banner.includes('>>>'); i++) {
  mod._picoemu_step(4000000);
  total += 4000000;
  banner += drain();
}
ok(banner.includes('MicroPython'), `MP banner at ~${total} steps`);
ok(banner.includes('RP2350'), 'banner names RP2350');
ok(banner.includes('>>>'), 'REPL >>> prompt bytes');
// USB-CDC input path on M33 MP: inject print(6*7), expect 42.
{
  const line = 'print(6*7)\r';
  for (const c of line) mod._picoemu_write_uart(c.charCodeAt(0));
  let out = '';
  for (let i = 0; i < 40 && !out.includes('42'); i++) {
    mod._picoemu_step(1000000);
    out += drain();
  }
  ok(out.includes('42'), `REPL eval 6*7==42 (${JSON.stringify(out.slice(-30))})`);
}
console.log(failures ? 'RESULT: FAIL' : 'RESULT: PASS');
process.exit(failures ? 1 : 0);
