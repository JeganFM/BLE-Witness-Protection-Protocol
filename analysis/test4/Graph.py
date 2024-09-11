import re
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.cm import get_cmap
from datetime import datetime

# Function to parse the log files and extract Time, RSSI, and Name
def parse_log(file):
    time_pattern = re.compile(r'\[(\d{2}:\d{2}:\d{2})\]')
    rssi_pattern = re.compile(r'RSSI: (-?\d+)')
    name_pattern = re.compile(r'Rotated to device name: (\w+)')
    
    times, rssis, names = [], [], []

    with open(file, 'r') as f:
        for line in f:
            time_match = time_pattern.search(line)
            rssi_match = rssi_pattern.search(line)
            name_match = name_pattern.search(line)
            
            if time_match and rssi_match and name_match:
                times.append(datetime.strptime(time_match.group(1), '%H:%M:%S'))
                rssis.append(int(rssi_match.group(1)))
                names.append(name_match.group(1))
    
    return pd.DataFrame({'Time': times, 'RSSI': rssis, 'Name': names})

# Load logs for Iris6, Iris7, Iris9
iris6_data = parse_log('SyncI6_Test_4B.log')
iris7_data = parse_log('SyncI7_Test_4B.log')
iris9_data = parse_log('SyncI9_Test_4B.log')

# Combine all logs into one DataFrame for consistent plotting
data = pd.concat([iris6_data, iris7_data, iris9_data])

# Create a colormap for the rotations
cmap = get_cmap('tab10')  # You can change this to any other colormap like 'viridis', 'plasma', etc.

# Plot settings
fig, ax = plt.subplots()

# Get unique rotations for color mapping
unique_rotations_iris6 = iris6_data['Name'].unique()
unique_rotations_iris7 = iris7_data['Name'].unique()
unique_rotations_iris9 = iris9_data['Name'].unique()

# Function to assign colors based on rotation index
def get_color(rotation_index):
    return cmap(rotation_index % 10)  # Modulo to cycle through the colormap if needed

# Plot data points for each Iris with different markers and colors per rotation
for i, rotation in enumerate(unique_rotations_iris6):
    subset = iris6_data[iris6_data['Name'] == rotation]
    ax.scatter(subset['Time'], subset['RSSI'], color=get_color(i), label=f'Iris6 {rotation}', marker='o')

for i, rotation in enumerate(unique_rotations_iris7):
    subset = iris7_data[iris7_data['Name'] == rotation]
    ax.scatter(subset['Time'], subset['RSSI'], color=get_color(i), label=f'Iris7 {rotation}', marker='s')

for i, rotation in enumerate(unique_rotations_iris9):
    subset = iris9_data[iris9_data['Name'] == rotation]
    ax.scatter(subset['Time'], subset['RSSI'], color=get_color(i), label=f'Iris9 {rotation}', marker='^')

# Add vertical lines for each sync signal (from Iris6 sync times)
sync_times = pd.to_datetime(iris6_data['Time'])  # Assuming the sync happens at the times in Iris6 log
for sync_time in sync_times:
    plt.axvline(sync_time, color='gray', linestyle='--')

# Labels and legend
plt.xlabel('Time')
plt.ylabel('RSSI (dBm)')
plt.legend()
plt.title('Synchronized Identity Rotation of Iris Devices with Different Colors for Rotations')

plt.show()
