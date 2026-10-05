# Firmware

All firmware is C on **Zephyr RTOS / nRF Connect SDK v2.7.0**, targeting the **nRF52840 DK**. Each folder is a self-contained Zephyr application:

```
<folder>/
├── CMakeLists.txt
├── prj.conf          # Zephyr/Kconfig options (Bluetooth roles, logging, etc.)
├── src/main.c        # application code
└── images/*.hex      # compiled firmware images from the build_* folders (flash directly)
```

Build with `west build -b nrf52840dk/nrf52840 <folder>`, or flash a prebuilt image from `images/`.

The folders reflect real iteration: several tests have multiple versions (`listener-v1` → `v3`) because the code was refined as the experiments progressed. The final version of each test is the highest-numbered or `-final` folder. Original working-folder names are listed so the history can be traced.

## Tests

| Folder | Original folder | What it does |
|--------|-----------------|--------------|
| `test1-no-rotation` | `Final_Test_1` | **Baseline.** Advertises a fixed device name forever (non-connectable). No rotation. |
| `test2-fixed-interval-rotation` | `Final_Test_2` | Rotates the advertised name every 60 s on a fixed timer, with timestamps in the log. |
| `test3-random-interval-rotation` | `Final_Test_3` | Rotates the name at a random interval between 1 and 5 minutes. |
| `test4-synchronized-rotation/broadcaster-iris3` | `Yo_broadcast` | **Iris3 broadcaster.** Advertises a `SYNC` manufacturer-data packet with an incrementing counter every 60 s. |
| `test4-synchronized-rotation/listener-v1` | `Fianl_Test_4` (sic) | First sync-based listener: rotates through a pre-generated list of names when a `SYNC` is heard. |
| `test4-synchronized-rotation/listener-v2` | `Final_Test_4_2` | Listener with counter-based name rotation (`Iris9_<n>`). |
| `test4-synchronized-rotation/listener-v3` | `Final_Test_4_3` | **Final Test 4 listener.** Adds `SYNC` debouncing and detects the other listeners' rotated names, logging RSSI. |
| `test4-synchronized-rotation/drafts` | `Untitled-1.cpp` | Listener draft with name normalisation helpers. |
| `test5-hybrid-rotation/broadcaster-iris3-keyed` | `Yo_Broadcaster_2` | Key-management broadcaster: generates a random 16-byte (IRK-style) key, stores it with a version number in non-volatile storage (NVS), and rotates every 60 s. |
| `test5-hybrid-rotation/broadcaster-iris3-encrypted` | `Yo_Broadcast_3` | **Final Test 5 broadcaster.** `SYNC` packet whose counter is obfuscated with a demonstration XOR key; logs the counter before and after encryption. |
| `test5-hybrid-rotation/listener-aes-gcm-experiment` | `Final_Test_4_4` | Experiment: AES-GCM key handling (TinyCrypt) with the key stored via Zephyr settings/flash. |
| `test5-hybrid-rotation/listener-final` | `Final_Test_4_5` | **Final Test 5 listener.** Hybrid: rotates on sync, rotates autonomously on a timer, and observes peers to adjust its own sequence. |
| `test5-hybrid-rotation/listener-iris7` | `Iris7` | Test 5 listener variant with shorter sync/rotation intervals. |
| `test5-hybrid-rotation/aes-otp-broadcaster-draft` | `message.txt` | Draft of an AES / one-time-key broadcaster. |

## Early prototypes (`00-early-prototypes`)

The work that led up to the numbered tests, in date order:

| Folder | Date | Description |
|--------|------|-------------|
| `broadcaster-first-draft` | 19 Aug 2024 | First broadcaster `main.c` and extended `prj.conf`. |
| `observer` | 23 Aug 2024 | Observer adapted from the nRF Connect SDK observer sample; prints timestamped device name, RSSI, and advertising type. This became the Iris5 passive observer. |
| `sync-on-command` | 31 Aug 2024 | First broadcaster/listener pair: the listener adjusts its timer when it receives a `SYNC` advertisement. Includes captured serial logs. |
| `rotation-test-1` | 4 Sep 2024 | First identity-rotation firmware (rotates the device name) with a flashable image. |
| `sync-on-time` | 10–11 Sep 2024 | Time-based sync prototype: a periodic `SYNC` broadcaster plus a listener. |

## Notes

- The Iris numbers (3, 5, 6, 7, 9) refer to the individual Raspberry Pi + nRF52840 DK nodes. The same listener firmware was flashed to Iris6, Iris7 and Iris9, which is why some builds are named `build_I6`, `build_I7`, `build_I9`.
- Several tests use device-name prefixes (`Iris6_`, `Iris7_`, `Iris9_`) to identify which node produced an identity; the prefix changes between versions as nodes were reassigned during development.
- Only the application sources and final `.hex` images are kept. Zephyr build directories (object files, generated headers, `.elf`) were left out because they can be regenerated from source.
