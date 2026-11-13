# RF-switch implementation and provenance

The first private prototype consulted a Silicon Labs Sidewalk configuration
file whose header referenced Zlib and additional Silicon Labs/Amazon terms.
That vendor source and the earlier helper are **not included in this release**.
This is not a claim that the author had never viewed that earlier material.

For the public release, `src/kg100s_rf_switch.h` is a newly written,
table-driven implementation of the hardware behavior documented by **Semtech,
SX1261/2 Datasheet, revision 2.2 (December 2024), section 8.6, page 59**.
The document explicitly describes manual DIO3 control and restoring it after
sleep. We use the documented register fields, not a port of Sidewalk source.

References:

- [Semtech SX1262 product page and current datasheet](https://www.semtech.com/products/wireless-rf/lora-connect/sx1262)
- [Semtech revision 2.2 datasheet, public mirror, page 59](https://manuals.plus/m/6d783125dbef567954ce53095ad068d0950b2a8de52f95bb2cfde4146c4306c2.pdf#page=59)

The KG100S uses this output as the antenna-switch supply, with voltage selection
6 (3.0 V) and output-high bit 3. The helper updates only the documented fields
and preserves unrelated register bits. Its register access callbacks are supplied
by Zephyr's existing SX126x driver. The integration patch runs restoration before
packet TX/RX/CAD, outside the driver's nonrecursive SPI semaphore, and logs the
initial register readback. Automated tests cover register values, preservation of
unrelated bits, and restoration after simulated sleep.

The helper and integration changes are licensed Apache-2.0 by the project.
Zephyr's existing driver keeps its upstream authorship and Apache-2.0 license.
This release does not include or grant rights to Amazon Sidewalk content.
No such vendor source is shipped or linked. The referenced hardware datasheet
is linked, not republished or relicensed.
