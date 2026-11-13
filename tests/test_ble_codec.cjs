// SPDX-License-Identifier: Apache-2.0
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const os = require('node:os');
const {spawnSync} = require('node:child_process');
const ctx = {};
vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../decoder.js'), 'utf8'), ctx);
const decode = bytes => JSON.parse(JSON.stringify(ctx.decodeUplink({bytes, fPort: 3})));

// Missing connection event support, wrong endianness or reversed address fails.
assert.deepEqual(decode([1, 1, 0x12, 0x34, 1, 0xC0, 0xDE, 0, 0x11, 0x22, 0x33]).data, {
  schema_version: 1,
  event: 'connected',
  sequence: 4660,
  ble_address: 'C0:DE:00:11:22:33',
  ble_address_type: 'random',
});
assert.equal(decode([1, 1, 0, 1, 0, 1, 2, 3, 4, 5, 6]).data.ble_address_type, 'public');
for (const frame of [[], [1, 1], [2, 1, 0, 1, 0, 1, 2, 3, 4, 5, 6],
  [1, 9, 0, 1, 0, 1, 2, 3, 4, 5, 6], [1, 1, 0, 1, 9, 1, 2, 3, 4, 5, 6]]) {
  assert.ok(decode(frame).errors.length > 0);
}
const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'opensidewalk-ble-'));
try {
  const exe = path.join(temp, 'ble-event');
  const built = spawnSync('clang', ['-std=c17', '-Wall', '-Wextra', '-Werror', '-Isrc',
    'tests/test_ble_event.c', 'src/ble_event.c', '-o', exe],
    {cwd: path.join(__dirname, '..'), encoding: 'utf8'});
  assert.equal(built.status, 0, built.stdout + built.stderr);
  const run = spawnSync(exe, [], {encoding: 'utf8'});
  assert.equal(run.status, 0, run.stdout + run.stderr);
  const hex = run.stdout.split('\n')[0];
  assert.equal(hex, '0101123401C0DE00112233');
  assert.equal(decode(Array.from(Buffer.from(hex, 'hex'))).data.ble_address, 'C0:DE:00:11:22:33');
  console.log(run.stdout.split('\n')[1]);
} finally {
  fs.rmSync(temp, {recursive: true, force: true});
}
console.log('PASS: TTN connection event decoding, address order/type and malformed frames.');
