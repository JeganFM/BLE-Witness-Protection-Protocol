# Analysis

Python scripts that turn the serial logs captured from the nRF52840 nodes into the RSSI-over-time graphs and parsed datasets used to analyse the tests.

**Requirements:** Python 3 with `pandas`, `matplotlib`, `plotly` and `numpy`.

## How they work

The logs contain timestamped lines such as sync detections, name rotations (`Rotated to device name: Iris6_2`), and observations of other devices (`Iris device found: Iris9_1, RSSI: -53`). The scripts parse those lines with regular expressions, build a DataFrame, and plot RSSI against time, colour-coded by device and rotation.

Most scripts read a log file named on a `log_file = r"..."` line near the bottom of the file (relative to the working directory). To reproduce a graph, copy or point that line at the matching log in [`../results`](../results), then run the script. The scripts are kept exactly as they were during the project, so you may need to adjust that filename. For example, `test5/Viz.py` originally pointed at a log file whose name is now `results/test5/logs/test5_run_20241130.log`.

**Note:** The raw logs from the passive observer (Iris5) are not published, because they contain other people's devices (see [DATA.md](../DATA.md)). Several scripts here, mainly the Test 1 to 4 graphers, were written to read those logs, so they will not run on this repository's data as-is. The graphs they produced are in [`../results`](../results), and the logs from the project's own nodes are included.

## Layout

| Folder | Contents |
|--------|----------|
| `shared/` | `iris.py`, the main Plotly log parser and grapher, plus `irisgraph.py` and `file_check.py` |
| `test1/` – `test4/` | Per-test versions of the grapher (`iris.py`, `Test1_Static.py` … `Test4.py`) and helpers (`parse.py`, `grapher.py`, `clean.py`, `clean_graph.py`, `one_log.py`) |
| `test5/` | Test 5 parsing and visualisation: `pars.py` (log → CSV), `Dataframe.py`, `Graph.py`, `Viz.py`, `parse_and_plot_rotations.py` |
| `early-experiments/` | July–August 2024 exploratory scripts (BLE address detection, first RSSI graphs) |

Where a script has several versions with the same name, the later versions carry their original last-modified timestamp in the file name (for example `iris-20240922-064627.py`).
