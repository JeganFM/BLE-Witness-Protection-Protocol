# BLE Witness Protection Protocol

**Synchronized identity rotation for Bluetooth Low Energy devices: protecting against passive tracking.**

**Read the full write-up:** [Technical report](docs/TECHNICAL-REPORT.md), covering the motivation, the math, the five tests and the results.

---

## The problem

BLE devices advertise a static identifier, which makes them easy to track. Address randomisation (Resolvable Private Addresses, RPAs) was introduced to prevent this, but research has shown that devices can still be correlated through passive and active methods. Prior work synchronised MAC address rotation between a single central and a single peripheral. This project looks at the gap: **multi-peripheral piconets**, where one central device talks to several peripherals, and how their identity changes can be coordinated through centralised and decentralised strategies.

## The approach: a "witness protection" protocol

The name comes from witness protection: someone at risk is given a new name and a new place, so that anyone who knew the old identity can no longer find them. Here, a BLE device gets a new advertised name, and the devices change together so that the old and new identities can't be matched up.

Every device in the network periodically adopts a new identity (here, the BLE device name, e.g. `Iris9_1` → `Iris9_2`). The protocol studies *how and when* those changes happen, so that an outside observer can no longer link the old identity to the new one.

### Hardware and roles

Five Raspberry Pis, each controlling an **nRF52840 DK** over a SEGGER J-Link:

| Node | Role | Behaviour |
|------|------|-----------|
| **Iris3** | Broadcaster | Sends periodic `SYNC` advertisements (with a counter, later encrypted) that coordinate rotation |
| **Iris6, Iris7, Iris9** | Listeners / peers | Rotate identity on sync, and scan for each other's rotations |
| **Iris5** | Passive observer | Logs all advertising traffic as an independent attacker's view |

![Sync and rotation sequence](docs/diagrams/sync_rotation_sequence_diagram.png)

### The five tests

| # | Strategy | Firmware | Results |
|---|----------|----------|---------|
| 1 | **No rotation**: static identity (baseline) | [firmware/test1-no-rotation](firmware/test1-no-rotation) | [results/test1](results/test1) |
| 2 | **Fixed-interval rotation**: every 60 s, independently | [firmware/test2-fixed-interval-rotation](firmware/test2-fixed-interval-rotation) | [results/test2](results/test2) |
| 3 | **Random-interval rotation**: every 1 to 5 min | [firmware/test3-random-interval-rotation](firmware/test3-random-interval-rotation) | [results/test3](results/test3) |
| 4 | **Synchronized rotation**: listeners rotate on the broadcaster's `SYNC` signal, with debouncing | [firmware/test4-synchronized-rotation](firmware/test4-synchronized-rotation) | [results/test4](results/test4) |
| 5 | **Hybrid rotation**: encrypted sync signal, then autonomous and peer-observed rotation | [firmware/test5-hybrid-rotation](firmware/test5-hybrid-rotation) | [results/test5](results/test5) |

## Results

RSSI over time for each device, as recorded by the observer. **Test 1** has a static identity throughout; **Test 4** shows all three listeners changing identity together on each sync signal.

| Test 1: no rotation | Test 4: synchronized rotation |
|---|---|
| ![Test 1](results/test1/graphs/Test_1_All_Devices_No_Rotation.png) | ![Test 4](results/test4/graphs/Test4_All_Devices.png) |

| Test 2: fixed interval | Test 3: random interval |
|---|---|
| ![Test 2](results/test2/graphs/Test2_All_Devices.png) | ![Test 3](results/test3/graphs/Test3_All_Devices.png) |

The four serial terminals during a Test 4 run (Iris6, Iris9, Iris7 listeners and the Iris3 broadcaster):

![Four-device terminals](results/test4/screenshots/four_device_terminals.png)

## Repository layout

```
firmware/    C firmware (Zephyr RTOS / nRF Connect SDK) for every device and every test iteration,
             plus compiled .hex images and the early prototypes that led up to the tests
analysis/    Python scripts that parse the serial logs and produce the RSSI-over-time graphs
results/     Serial logs from the project's own nodes, parsed data, and generated graphs, organised per test
docs/        Technical report and design diagrams
```

See [firmware/README.md](firmware/README.md) for a guide to every firmware folder (which is which, and how the iterations relate) and [analysis/README.md](analysis/README.md) for the scripts.

## Toolchain

- **Boards:** Nordic nRF52840 DK (`nrf52840dk/nrf52840`)
- **SDK / RTOS:** nRF Connect SDK v2.7.0, Zephyr RTOS (v3.6.99)
- **Language:** C (firmware); Python with pandas, matplotlib, plotly and numpy (analysis)
- **Flashing / monitoring:** SEGGER J-Link via `west flash` (or nRF Connect for VS Code); serial output captured with `screen` on each Raspberry Pi

### Building and flashing a firmware folder

Each firmware folder is a standard Zephyr application (`CMakeLists.txt`, `prj.conf`, `src/main.c`) and can be built from an nRF Connect SDK v2.7.0 environment:

```bash
west build -b nrf52840dk/nrf52840 firmware/test4-synchronized-rotation/listener-v3
west flash
```

Pre-built images are in each folder's `images/` directory and can be flashed directly.

## About the dates in this repository

This project was developed between **July and December 2024**. The repository was assembled in **October 2026** from the original working folders, and no file contents were altered to do so, apart from the privacy edits described under [Data handling](#data-handling). Each commit is dated to the original last-modified time of the files it contains (UK time: BST until 27 Oct 2024, GMT after), so the history reflects when the work was actually done. Where the same file existed in several places, the earliest copy's timestamp was kept. [docs/provenance/MANIFEST.csv](docs/provenance/MANIFEST.csv) lists every file's original location, timestamp, and checksum. The README files, the data note and the technical report were written at assembly time and carry the assembly date.

## Data handling

The passive observer (Iris5) records every BLE advertisement in range, which includes devices belonging to other people. For that reason the raw observer logs and the screen recordings of the test runs are **not published** in this repository. The graphs generated from them are included, along with the logs from the project's own nodes and the parsed datasets. See [DATA.md](DATA.md) for details.

Local account names were also removed from a few file paths in the analysis scripts (now plain filenames, read from the working directory) and from the window titles in one screenshot. The affected files are marked in [docs/provenance/MANIFEST.csv](docs/provenance/MANIFEST.csv).

## Author

**JeganFM**
