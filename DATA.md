# Data

## What was captured

Each test used the project's own nRF52840 nodes (Iris3 broadcaster; Iris6, Iris7 and Iris9 listeners) plus a passive observer (Iris5). The observer logged every BLE advertisement it could hear, so its captures include devices that belong to other people, such as their random addresses, advertised names, signal strength and timing.

## What is in this repository

| Data | Included | Notes |
|------|----------|-------|
| Logs from the project's own nodes (Iris3, Iris6, Iris7, Iris9) | Yes | `results/test*/logs/`, `firmware/00-early-prototypes/` |
| Parsed datasets | Yes | `results/parsed-data/` (names, RSSI and timestamps of the project's own devices) |
| Graphs generated from the logs | Yes | `results/test*/graphs/`, `results/plotly-exports/` |
| Raw observer (Iris5) logs | **No** | They contain other people's devices |
| Screen recordings of the test runs | **No** | They show raw terminal output and cannot be edited to remove third-party or account details |

## Consequences for reproducing the results

The graphs for Tests 1 to 4 were produced from observer logs that are not published here, so the scripts in `analysis/` cannot regenerate those graphs from this repository alone. The scripts, the graphs they produced, and the logs from the project's own nodes are all included, and the method is described in the technical report (`docs/TECHNICAL-REPORT.md`).

## Other privacy edits

- Local account names were removed from file paths in three analysis scripts and from the window titles of one screenshot.
- `docs/provenance/MANIFEST.csv` lists the original and published checksum of each edited file.
