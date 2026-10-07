#!/usr/bin/env node
// WASM jsmirror regression test: attach jsmirror over the exact runner
// sequence, drive DW writes via mem_write32, assert ring contents.
// Catches attach/ring/codegen regressions that native tests can't
// (native and wasm are different toolchains).
import PicoemuModule from './web/picoemu.wasm.js';

const mod = await PicoemuModule({ print: () => {}, printErr: () => {} });
let failures = 0;
const ok = (c, m) => { console.log((c ? 'ok' : 'FAIL') + ': ' + m); if (!c) failures++; };

const I2C0 = 0x40044000, TAR = 0x004, DATA_CMD = 0x010, TX_ABRT = 0x080;
const STOP = 1 << 9;

function sddAdd(arg) {
  const bytes = new TextEncoder().encode(arg + '\0');
  const ptr = mod._malloc(bytes.length);
  mod.HEAPU8.set(bytes, ptr);
  const r = mod._picoemu_sdd_add(ptr);
  mod._free(ptr);
  return r;
}
function drain() {
  const out = [];
  const max = 64, ptr = mod._malloc(max * 2);
  const n = mod._picoemu_jsmirror_pop(ptr, max);
  const view = new Uint16Array(mod.HEAPU8.buffer, ptr, n);
  for (let i = 0; i < n; i++) out.push(view[i] & 0xffff);
  mod._free(ptr);
  return out;
}

// Boot M0+ like the runner (init, no firmware needed for DW regs).
mod._picoemu_init(0);
mod._picoemu_set_clock(125);

// 1) Runner's exact attach string.
const r = sddAdd('jsmirror:i2c=0,addr=0x3c');
ok(r >= 0, `sdd_add jsmirror (r=${r})`);

// 2) Addressed DW write -> ring [data, data, STOP], no abort.
mod._picoemu_mem_write32(I2C0 + TAR, 0x3c);
mod._picoemu_mem_write32(I2C0 + DATA_CMD, 0x00);
mod._picoemu_mem_write32(I2C0 + DATA_CMD, 0xae | STOP);
ok(mod._picoemu_mem_read32(I2C0 + TX_ABRT) === 0, 'no TX abort on attached write');
ok(mod._picoemu_jsmirror_pending() === 3, `ring pending==3 (got ${mod._picoemu_jsmirror_pending()})`);
const e = drain();
ok(e.length === 3 && e[0] === 0x00 && e[1] === 0xae && e[2] === 0x200,
  `ring == [0x00,0xae,STOP] (got ${e.map(x => '0x' + x.toString(16)).join(',')})`);

// 3) Empty address -> abort, ring silent.
mod._picoemu_mem_write32(I2C0 + TAR, 0x40);
mod._picoemu_mem_write32(I2C0 + DATA_CMD, 0x00 | STOP);
ok((mod._picoemu_mem_read32(I2C0 + TX_ABRT) & 1) !== 0, 'ADDR_NOACK abort on empty address');
ok(mod._picoemu_jsmirror_pending() === 0, 'ring silent on NACK');

// 4) Repeat sdd_add (re-provision) must keep working, not orphan.
const r2 = sddAdd('jsmirror:i2c=0,addr=0x3c');
ok(r2 >= 0, `second sdd_add (r=${r2})`);
mod._picoemu_mem_write32(I2C0 + TAR, 0x3c);
mod._picoemu_mem_write32(I2C0 + DATA_CMD, 0x00 | STOP);
ok(mod._picoemu_jsmirror_pending() === 2, `ring alive after re-add (got ${mod._picoemu_jsmirror_pending()})`);
drain();

console.log(failures ? 'RESULT: FAIL' : 'RESULT: PASS');
process.exit(failures ? 1 : 0);
