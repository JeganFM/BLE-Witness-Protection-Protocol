#import matplotlib.pyplot as plt
import pandas as pd
from datetime import datetime
import numpy as np

# Function to parse the log and extract time and RSSI values
def parse_log(file):
    times = []
    rssi_values = []
    
    with open(file, 'r') as f:
        for line in f:
            line = line.strip()

            # Focus on SYNC signal lines to capture RSSI and time
            if "SYNC signal detected" in line:
                parts = line.split()
                time_str = parts[0].strip('[]')
                time = datetime.strptime(time_str, '%H:%M:%S')

                # Only take the first 10 minutes of data
                if time.minute >= 10:
                    break

                rssi = int(parts[8].strip(','))

                # Store the time and RSSI
                times.append(time)
                rssi_values.append(rssi)

    return times, rssi_values

# Path to your log file
log_file = r"Observer_no_rotation.log"

# Parse the log
times, rssi_values = parse_log(log_file)

# Convert the data to a Pandas DataFrame
df = pd.DataFrame({
    'Time': times,
    'RSSI': rssi_values
})

# Set the figure size
plt.figure(figsize=(18, 9))

# Define colors for each segment (alternating colors)
colors = plt.cm.viridis(np.linspace(0, 1, len(df)-1))

# Plot each segment individually with a different color
for i in range(len(df) - 1):
    plt.plot(df['Time'].iloc[i:i+2], df['RSSI'].iloc[i:i+2], '-o', color=colors[i], markerfacecolor='white')

# Add labels and title
plt.xlabel('Time', fontsize=14)
plt.ylabel('RSSI (dBm)', fontsize=14)
plt.title('Synchronized Identity Rotation of Iris6 with Different Colors for Rotations', fontsize=16)

# Enable grid and adjust the layout
plt.grid(True)
plt.tight_layout()

# Display the plot
plt.show()
