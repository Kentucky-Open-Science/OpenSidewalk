// SPDX-License-Identifier: Apache-2.0
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const os = require('node:os');
const {spawnSync} = require('node:child_process');

const root = path.resolve(__dirname, '..');
const decoderPath = path.join(root, 'decoder.js');
assert.ok(fs.existsSync(decoderPath), 'The device telemetry decoder has not been implemented');
assert.ok(fs.existsSync(path.join(root, 'src/telemetry_packet.c')),
          'The firmware telemetry encoder has not been implemented');
const context = {};
vm.runInNewContext(fs.readFileSync(decoderPath, 'utf8'), context);
const decode = (bytes, fPort = 2) => JSON.parse(JSON.stringify(context.decodeUplink({bytes, fPort})));

// A swapped byte order, unsigned temperature, or wrong button-bit mapping must fail these.
assert.deepEqual(decode([1, 0, 1, 9, 230, 2, 0, 0, 0, 7]).data, {
  schema_version: 1,
  sequence: 1,
  temperature_c: 25.34,
  temperature_f: 77.61,
  temperature_source: 'mcu_die',
  button_0_pressed: false,
  button_1_pressed: true,
  button_0_presses: 0,
  button_1_presses: 7,
});

const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'kg100s-codec-'));
try {
  const binary = path.join(temp, 'fixture');
  const built = spawnSync('clang', ['-std=c17', '-Wall', '-Wextra', '-Werror', '-Isrc',
    'tests/telemetry_fixture.c', 'src/telemetry_packet.c', '-o', binary], {cwd: root, encoding: 'utf8'});
  assert.equal(built.status, 0, built.stdout + built.stderr);
  const encoded = spawnSync(binary, [], {encoding: 'utf8'});
  assert.equal(encoded.status, 0, encoded.stderr);
  assert.equal(encoded.stdout.trim(), '011234FB2E030102ABCD');
  const negative = decode(Array.from(Buffer.from(encoded.stdout.trim(), 'hex'))).data;
  assert.equal(negative.temperature_c, -12.34);
  assert.equal(negative.temperature_f, 9.79);
  assert.equal(negative.sequence, 4660);
  assert.equal(negative.button_0_pressed, true);
  assert.equal(negative.button_1_pressed, true);
  assert.equal(negative.button_0_presses, 258);
  assert.equal(negative.button_1_presses, 43981);
} finally {
  fs.rmSync(temp, {recursive: true, force: true});
}

const missing = decode([1, 0, 2, 128, 0, 0, 0, 0, 0, 0]);
assert.equal(missing.data.temperature_c, null);
assert.equal(missing.data.temperature_f, null);
assert.ok(missing.warnings.length > 0);
assert.ok(decode([0x4b, 0x47, 0x31, 0x30, 0x30, 0x53]).errors.length > 0);
assert.ok(decode([2, 0, 0, 0, 0, 0, 0, 0, 0, 0]).errors.length > 0);
assert.ok(decode([1, 0, 0, 0, 0, 0, 0, 0, 0, 0], 3).errors.length > 0);
console.log('PASS: C encoder/TTN decoder interoperate; signed temperature, button states/counts, missing temperature, and invalid frames.');
