#!/usr/bin/env node
// WASM guest-driven I2C test (full live path): boot the bundled
// MicroPython, attach jsmirror, drive machine.I2C.writeto through the
// REPL exactly like the simulator runner does, assert ring contents.
// Catches codegen/stepping regressions no direct-register test can.
import PicoemuModule from './web/picoemu.wasm.js';
import fs from 'fs';

const mod = await PicoemuModule({ print: () => {}, printErr: () => {} });
let failures = 0;
const ok = (c, m) => { console.log((c ? 'ok' : 'FAIL') + ': ' + m); if (!c) failures++; };

function drainUart(cap = 8000) {
  let out = '', ch, n = 0;
  while ((ch = mod._picoemu_read_uart(0)) !== -1 && n++ < cap) out += String.fromCharCode(ch);
  return out;
}
function execLine(line) {
  for (const c of line + '\r') mod._picoemu_write_uart(c.charCodeAt(0));
  mod._picoemu_step(3000000);
  return drainUart();
}
function sddAdd(arg) {
  const bytes = new TextEncoder().encode(arg + '\0');
  const ptr = mod._malloc(bytes.length);
  mod.HEAPU8.set(bytes, ptr);
  const r = mod._picoemu_sdd_add(ptr);
  mod._free(ptr);
  return r;
}
function ringDrain() {
  const out = [], max = 64, ptr = mod._malloc(max * 2);
  const n = mod._picoemu_jsmirror_pop(ptr, max);
  const view = new Uint16Array(mod.HEAPU8.buffer, ptr, n);
  for (let i = 0; i < n; i++) out.push(view[i] & 0xffff);
  mod._free(ptr);
  return out;
}

mod._picoemu_init(0);
mod._picoemu_set_clock(125);
{
  const uf2 = new Uint8Array(fs.readFileSync('./web/micropython_rp2040.uf2'));
  const ptr = mod._malloc(uf2.length);
  mod.HEAPU8.set(uf2, ptr);
  mod._picoemu_load_uf2(ptr, uf2.length);
  mod._free(ptr);
}
// Runner-faithful order: attach BEFORE post-load reset + boot.
ok(sddAdd('jsmirror:i2c=0,addr=0x3c') >= 0, 'sdd_add jsmirror (pre-reset)');
mod._picoemu_reset();
mod._picoemu_step(3000000);
const boot = drainUart();
ok(boot.includes('>>>'), 'MP REPL banner');

let out = execLine('from machine import I2C,Pin');
ok(!out.includes('Traceback') && !out.includes('Error'), `import I2C (${JSON.stringify(out.slice(-40))})`);
out = execLine('i2c=I2C(0,scl=Pin(5),sda=Pin(4),freq=100000)');
ok(!out.includes('Traceback') && !out.includes('Error'), `I2C init (${JSON.stringify(out.slice(-40))})`);
out = execLine('i2c.writeto(60,bytes([0,174]))');
ok(!out.includes('Traceback'), `writeto no traceback (${JSON.stringify(out.slice(-60))})`);

const pend = mod._picoemu_jsmirror_pending();
ok(pend === 3, `ring pending==3 after guest writeto (got ${pend})`);
const e = ringDrain();
ok(e.length === 3 && e[0] === 0x00 && e[1] === 0xae && e[2] === 0x200,
  `ring == [0x00,0xae,STOP] (got ${e.map(x => '0x' + x.toString(16)).join(',')})`);

console.log(failures ? 'RESULT: FAIL' : 'RESULT: PASS');
process.exit(failures ? 1 : 0);
