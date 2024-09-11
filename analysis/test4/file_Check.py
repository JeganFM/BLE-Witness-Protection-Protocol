import matplotlib.pyplot as plt
import pandas as pd
from datetime import datetime
import numpy as np

# Function to parse the log and extract time, RSSI values, and names
def parse_log(file):
    times = []
    rssi_values = []
    names = []  # Add names list to store the name rotations

    with open(file, 'r') as f:
        for line in f:
            line = line.strip()

            # Focus on SYNC signal lines to capture RSSI, time, and name rotation
            if "SYNC signal detected" in line:
                parts = line.split()
                time_str = parts[0].strip('[]')
                time = datetime.strptime(time_str, '%H:%M:%S')

                # Only take the first 10 minutes of data
                if time.minute >= 10:
                    break

                rssi = int(parts[8].strip(','))
                name_rotation = "Rotation_" + parts[10]  # Assuming part 10 holds the rotation count

                # Store the time, RSSI, and name
                times.append(time)
                rssi_values.append(rssi)
                names.append(name_rotation)  # Append the name rotation

    return times, rssi_values, names

# Path to your log file
log_file = r"SyncI6_Test_4B.log"

# Parse the log
times, rssi_values, names = parse_log(log_file)

# Convert the data to a Pandas DataFrame
df = pd.DataFrame({
    'Time': times,
    'RSSI': rssi_values,
    'Name': names  # Include names in the DataFrame
})

# Set the figure size
plt.figure(figsize=(18, 9))

# Define colors for each segment (alternating colors)
colors = plt.cm.viridis(np.linspace(0, 1, len(df)-1))

# Plot each segment individually with a different color
for i in range(len(df) - 1):
    plt.plot(df['Time'].iloc[i:i+2], df['RSSI'].iloc[i:i+2], '-o', color=colors[i], markerfacecolor='white')

# Add name labels to each point
for i in range(len(df)):
    plt.text(df['Time'].iloc[i], df['RSSI'].iloc[i], df['Name'].iloc[i], fontsize=10, ha='right')

# Add labels and title
plt.xlabel('Time', fontsize=14)
plt.ylabel('RSSI (dBm)', fontsize=14)
plt.title('Synchronized Identity Rotation of Iris6 with Name Rotations', fontsize=16)

# Enable grid and adjust the layout
plt.grid(True)
plt.tight_layout()

# Display the plot
plt.show()
