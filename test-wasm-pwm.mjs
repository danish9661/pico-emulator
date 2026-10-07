#!/usr/bin/env node
// WASM PWM enable test: the global EN register aliases the per-slice
// CSR_EN bits (single physical bit, RP2040 datasheet). Arduino
// analogWrite only ever sets CSR_EN, so the tap must report enabled=1
// from CSR state alone. Covers direct-register + guest-driven paths.
import PicoemuModule from './web/picoemu.wasm.js';
import fs from 'fs';

const mod = await PicoemuModule({ print: () => {}, printErr: () => {} });
let failures = 0;
const ok = (c, m) => { console.log((c ? 'ok' : 'FAIL') + ': ' + m); if (!c) failures++; };
const W = (a, v) => mod._picoemu_mem_write32(a, v);
const R = (a) => mod._picoemu_mem_read32(a) >>> 0;
function tap(slice) {
  const p = mod._malloc(16);
  const r = mod._picoemu_pwm_read(slice, p, p + 4, p + 8, p + 12);
  const v = new Uint32Array(mod.HEAPU8.buffer, p, 4);
  mod._free(p);
  return { r, freq: v[0], da: v[1], db: v[2], en: v[3] };
}

const PWM = 0x40050000, CSR0 = 0x00, EN = 0xA0, SET = 0x2000;

// Direct: CSR-only enable (Arduino analogWrite path) reports enabled.
mod._picoemu_init(0);
mod._picoemu_set_clock(125);
W(PWM + 0x14 + 0x10, 999);   // slice1 TOP
W(PWM + 0x14 + 0x0c, 750);     // slice1 CC (B)
W(PWM + 0x14, 1);              // slice1 CSR_EN, no EN-reg write
ok((R(PWM + EN) & 2) !== 0, 'EN bit mirrors CSR_EN');
{
  const t = tap(1);
  ok(t.r === 0 && t.en === 1, `tap enabled=1 from CSR (en=${t.en})`);
  ok(t.db === 7500, `duty 75% (db=${t.db})`);
}
// Direct: global-EN write drives CSR_EN back.
W(PWM + EN, 0x00);
ok((R(PWM + CSR0) & 1) === 0, 'CSR_EN clears via EN write');
W(PWM + SET + EN, 0x01);
ok((R(PWM + CSR0) & 1) !== 0, 'CSR_EN sets via EN SET-alias');

// Guest-driven: Arduino analogWrite(16,192) enables slice 0.
{
  const uf2 = new Uint8Array(fs.readFileSync('/tmp/opencode/pwmt/build/pwmt.ino.uf2'));
  const ptr = mod._malloc(uf2.length);
  mod.HEAPU8.set(uf2, ptr);
  mod._picoemu_load_uf2(ptr, uf2.length);
  mod._free(ptr);
}
mod._picoemu_reset();
mod._picoemu_step(3000000);
{
  const t = tap(0);
  ok(t.en === 1, `guest analogWrite: enabled=1 (en=${t.en})`);
  const duty = Math.max(t.da, t.db);
  ok(duty >= 7400 && duty <= 7600, `guest analogWrite: duty ~7500 (da=${t.da} db=${t.db})`);
}

console.log(failures ? 'RESULT: FAIL' : 'RESULT: PASS');
process.exit(failures ? 1 : 0);
