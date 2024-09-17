import re
import matplotlib.pyplot as plt
from datetime import datetime
import pandas as pd

# Define the path to your log file
file_path = 'Observer_rotation_1m.log'  

# Read the log content from the text file
with open(file_path, 'r') as file:
    log_content = file.readlines()

# Define the device names to extract data for
device_names = ['Iris6', 'Iris7', 'Iris9']

# Dictionary to hold the timestamp and RSSI values for each device
device_data = {device: {'timestamp': [], 'rssi': []} for device in device_names}

# Parsing the log content
for line in log_content:
    for device in device_names:
        if device in line:
            match = re.search(r'\[(\d+:\d+:\d+)\].*RSSI (-\d+)', line)
            if match:
                timestamp = datetime.strptime(match.group(1), '%H:%M:%S')
                rssi = int(match.group(2))
                device_data[device]['timestamp'].append(timestamp)
                device_data[device]['rssi'].append(rssi)

# Now resample the data to 1-hour intervals and convert x-axis to minutes
plt.figure(figsize=(14, 8))

for device in device_names:
    df = pd.DataFrame({
        'timestamp': device_data[device]['timestamp'],
        'rssi': device_data[device]['rssi']
    })
    df.set_index('timestamp', inplace=True)
    # Resample to 60 minutes and take the first value of each interval
    df_resampled = df.resample('60T').first()

    # Convert timestamps to minutes (relative to the first timestamp)
    df_resampled['minutes'] = (df_resampled.index - df_resampled.index[0]).total_seconds() / 60

    # Plot the resampled data with minutes on x-axis
    plt.plot(df_resampled['minutes'], df_resampled['rssi'], label=device, marker='o')

# Title and labels
plt.title('RSSI over Time for Iris6, Iris7, and Iris9 (60 Minute Intervals)', fontsize=18)
plt.xlabel('Time (Minutes)', fontsize=14)
plt.ylabel('RSSI (dBm)', fontsize=14)
plt.grid(True)
plt.xticks(fontsize=12)
plt.yticks(fontsize=12)
plt.legend(fontsize=14)
plt.tight_layout()

# Display the plot
plt.show()
