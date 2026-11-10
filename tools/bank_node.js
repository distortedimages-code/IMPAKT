// IMPAKT — CHIMERA's bank into a WebAssembly engine instance (node tools: preset_levels, bench, the tours), the way the
// page's worklet does it (core/sk_wasm.cpp sk_bank_*): the index into the stage, then each sample's bytes, decoded at once
const fs = require('fs'), path = require('path');
const DIR = path.join(__dirname, '..', 'bank');
let files = null, idx = null, jobs = null;
function read() {
  if (files) return;
  idx = fs.readFileSync(path.join(DIR, 'chimera.idx'));
  files = [];
  for (let k = 0; fs.existsSync(path.join(DIR, 'chimera.' + k + '.bin')); k++) files.push(fs.readFileSync(path.join(DIR, 'chimera.' + k + '.bin')));
  // (where each sample is: the index's records, as core/sk_bank.cpp reads them)
  const d = new DataView(idx.buffer, idx.byteOffset, idx.byteLength), n = d.getUint32(8, true), nc = d.getUint32(12, true);
  let p = 16; for (let c = 0; c < nc; c++) p += 1 + idx[p];
  jobs = [];
  for (let i = 0; i < n; i++) {
    const nl = idx[p + 2] | (idx[p + 3] << 8);
    jobs.push([i, d.getUint32(p + 20, true), d.getUint32(p + 24, true), d.getUint32(p + 16, true)]);
    p += 80 + nl + 64;
  }
}
// returns how many samples the engine took
function loadBank(x) {
  read();
  const stage = () => new Uint8Array(x.memory.buffer, x.sk_bank_stage(), x.sk_bank_cap());
  stage().set(idx);
  if (x.sk_bank_index(idx.length) < 0) return 0;
  let ok = 0;
  for (const [i, file, off, len] of jobs) {
    stage().set(files[file].subarray(off, off + len));
    x.sk_bank_feed(i, len);
    while (!x.sk_bank_step(1 << 24)) {}
    ok++;
  }
  return ok;
}
module.exports = { loadBank };
