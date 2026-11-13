/* SPDX-License-Identifier: Apache-2.0 */
// KG100S telemetry schema 1, application port 2, 10-byte big-endian payload.
function decodeUplink(input) {
  var b = input.bytes;
  if (input.fPort !== 2) {
    return {errors: ["KG100S telemetry uses FPort 2"]};
  }
  if (b.length !== 10) {
    return {errors: ["KG100S telemetry requires exactly 10 bytes"]};
  }
  if (b[0] !== 1) {
    return {errors: ["Unsupported KG100S telemetry schema"]};
  }
  var rawTemp = (b[3] << 8) | b[4];
  var temp = rawTemp >= 32768 ? rawTemp - 65536 : rawTemp;
  var missing = temp === -32768;
  return {
    data: {
      schema_version: 1,
      sequence: (b[1] << 8) | b[2],
      temperature_c: missing ? null : temp / 100,
      temperature_f: missing ? null : Math.round((temp / 100 * 9 / 5 + 32) * 100) / 100,
      temperature_source: "mcu_die",
      button_0_pressed: (b[5] & 1) !== 0,
      button_1_pressed: (b[5] & 2) !== 0,
      button_0_presses: (b[6] << 8) | b[7],
      button_1_presses: (b[8] << 8) | b[9]
    },
    warnings: missing ? ["MCU temperature was unavailable"] : [],
    errors: []
  };
}
