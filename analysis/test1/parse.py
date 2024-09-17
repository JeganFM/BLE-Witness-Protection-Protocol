import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
from datetime import datetime
import re

# Function to parse the log and extract time and RSSI values for devices like "Iris9", "Iris7", and "Iris6"
def parse_log_v2(file):
    times = []
    names = []
    rssi_values = []

    with open(file, 'r') as f:
        for line in f:
            line = line.strip()
            
            # Search for device names like "Iris6", "Iris7", "Iris9"
            match = re.search(r'Name: (Iris\d+)', line)
            if match:
                # Extract the time (first element in square brackets)
                time_str = re.search(r'\[(\d{2}:\d{2}:\d{2})\]', line).group(1)
                time = datetime.strptime(time_str, '%H:%M:%S')
                
                # Extract the device name (e.g., "Iris6")
                name = match.group(1)
                
                # Extract the RSSI value (comes after "RSSI")
                rssi = int(re.search(r'RSSI (-\d+)', line).group(1))
                
                # Store the extracted information
                times.append(time)
                names.append(name)
                rssi_values.append(rssi)

    # Create a DataFrame with the parsed data
    df = pd.DataFrame({
        'TIME': times,
        'NAME': names,
        'RSSI': rssi_values
    })

    return df

# Specify the log file to process
log_file_path = 'Observer_no_rotation.log'  # Change this path to select the other file if needed

# Parse the specified log file
df = parse_log_v2(log_file_path)

# Convert TIME to a usable format (in seconds)
df['TIME_seconds'] = df['TIME'].dt.hour * 3600 + df['TIME'].dt.minute * 60 + df['TIME'].dt.second

# Extract the device prefix from the NAME
df['Device'] = df['NAME']

# Unique devices in the data
devices = df['Device'].unique()

# Define the time range for plotting in seconds
time_range = (df['TIME_seconds'].min(), df['TIME_seconds'].max())

# Set the RSSI range
rssi_range = (-100, -30)

# Plot each device separately
for device in devices:
    device_df = df[df['Device'] == device]

    # Create a plot for the current device
    fig, ax = plt.subplots(figsize=(10, 6))

    # Plot the RSSI values for the current device
    ax.plot(device_df['TIME_seconds'], device_df['RSSI'], label=f'{device}')

    # Set the x-axis limit for the recorded period and the y-axis for RSSI
    ax.set_xlim(time_range)
    ax.set_ylim(rssi_range)

    # Set major ticks every 10 minutes (600 seconds) and minor ticks every minute (60 seconds)
    ax.xaxis.set_major_locator(ticker.MultipleLocator(600))  # 10-minute intervals
    ax.xaxis.set_minor_locator(ticker.MultipleLocator(60))   # 1-minute intervals

    # Format x-axis labels to show time in HH:MM:SS format
    ax.xaxis.set_major_formatter(ticker.FuncFormatter(lambda x, _: f'{int(x//3600):02}:{int((x%3600)//60):02}:{int(x%60):02}'))

    # Enable gridlines
    ax.grid(True, which='both', axis='both', linestyle='--', linewidth=0.5)

    # Label the plot
    ax.set_xlabel('Time')
    ax.set_ylabel('RSSI (dBm)')
    ax.set_title(f'RSSI over Time for {device}')
    ax.legend(title='Device')

    # Show the plot for the current device
    plt.show()
