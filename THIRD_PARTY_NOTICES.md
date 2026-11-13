# Third-party software notices

The project application, tools, tests, patches and contributed media use
**Apache-2.0**, except where a file explicitly states otherwise. Third-party
components retain their original licenses. The firmware is a combined work;
Apache-2.0 does not replace the licenses of its dependencies.

| Component | Revision / version | License and accompanying notices |
| --- | --- | --- |
| Zephyr RTOS, board support and drivers | `41d10b59730406bbab4ca9f1fbbe9e96aafb4966` (v4.5.0) | Apache-2.0 (`LICENSE`); BSD exceptions in `licenses/Zephyr-*-BSD.txt` |
| Arm CMSIS | `512cc7e895e8491696b61f7ba8066b4a182569b8` | Apache-2.0; preserve original Arm notices when redistributing sources |
| Arm CMSIS 6 | `1c1840af7a7e757d6e2fec3ddb0e5ce0dfcc93c8` | Apache-2.0; preserve original Arm notices when redistributing sources |
| Silicon Labs HAL | `3402d10d7bbe0f323e018a4142b7ab7d45f9308b` | Compiled HAL/device files use Zlib; see `licenses/Silicon-Labs-Zlib.txt` |
| LoRaMac-node stack and SX126x core | `874544f2a8a0212a0cc12e5eea51a024fd3d93d1` | Revised BSD, `licenses/LoRaMac-node-BSD.txt` |
| LoRaMac-node software AES | Brian Gladman, 1998–2008 | Separate permissive conditions, `licenses/AES-Brian-Gladman.txt` |
| LoRaMac-node software CMAC | Lander Casado / Philippas Tsigas, 2009 | Separate permissive conditions, `licenses/CMAC-Casado-Tsigas.txt` |
| Picolibc / Newlib runtime | Included with Zephyr SDK 1.0.1 | Per-file permissive licenses; complete supplied notices in `licenses/Picolibc-COPYING.txt` and `licenses/Newlib-COPYING.txt` |
| GNU compiler runtime `libgcc` | GCC 14.3.0, Zephyr SDK 1.0.1 | GPL-3.0-or-later WITH GCC-exception-3.1; both texts in `licenses/` |

The GCC Runtime Library Exception permits distributing eligible compiled
combinations under terms consistent with the independent modules. This project
uses the unmodified GNU toolchain shipped with Zephyr SDK 1.0.1. It does not
redistribute the compiler itself. Picolibc's consolidated notice catalog also
lists build scripts and other architectures that are not incorporated in this
ARM firmware; their presence in that catalog does not mean they are linked.

The dependency source trees are fetched by `west`; they are not vendored here.
Keep their source-file copyright/license headers when redistributing them.
`west.yml` pins Zephyr, which pins the selected modules. SEGGER is selected by
the manifest for Zephyr integration, but RTT/SEGGER code is not linked into the
shipped firmware (UART shell/logging is used).

The linked-image review used compiler inputs, Ninja header dependencies and the
linker map. It found Apache-2.0, Zlib, BSD and the crypto notices above, plus the
Picolibc and GCC runtime archives. No AWS Sidewalk stack, proprietary radio
binary, Bluetooth controller or RAIL binary is linked. No SDK installer,
factory firmware, USERDATA or NVS dump is distributed.

See [RF-switch provenance](docs/RF_SWITCH_PROVENANCE.md) for the replacement of
the earlier prototype's vendor-referenced RF-switch helper.
