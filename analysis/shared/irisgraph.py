import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker


# Load the data
file_path = 'iris_data (1).csv'
df = pd.read_csv(file_path)

# Convert TIME to a usable format (in seconds)
df['TIME'] = pd.to_datetime(df['TIME'], format='%H:%M:%S')
df['TIME_seconds'] = df['TIME'].dt.hour * 3600 + df['TIME'].dt.minute * 60 + df['TIME'].dt.second

# Extract the device prefix and rotation number from the NAME
df['Device'] = df['NAME'].str.extract(r'(Iris\d+)')
df['Rotation'] = df['NAME'].str.extract(r'_(\d+)')
df['Rotation'] = df['Rotation'].fillna(0).astype(int)

# Unique devices in the data
devices = df['Device'].unique()

# Define the time range for 2 hours in seconds (7200 seconds)
time_range = (0, 200)

# Set the RSSI range to -100 to -20 dBm
rssi_range = (-80, -2)

# Plot each device separately
for device in devices:
    device_df = df[df['Device'] == device]
    
    # Get the first 3 unique rotations for the current device
    rotations = device_df['Rotation'].unique()[:3]
    
    # Create a plot for the current device
    fig, ax = plt.subplots(figsize=(10, 6))
    
    # Plot each rotation in a different color
    for i, rotation in enumerate(rotations):
        rotation_df = device_df[device_df['Rotation'] == rotation]
        ax.plot(rotation_df['TIME_seconds'], rotation_df['RSSI'], label=f'{device}_Rotation_{rotation}')
    
    # Set the x-axis limit for a 2-hour period and the y-axis for RSSI
    ax.set_xlim(time_range)
    ax.set_ylim(rssi_range)
    
    # Set major ticks every 10 minutes (600 seconds) and minor ticks every minute (60 seconds)
    ax.xaxis.set_major_locator(ticker.MultipleLocator(600))  # 10-minute intervals
    ax.xaxis.set_minor_locator(ticker.MultipleLocator(30))   # 30-second intervals
    
    # Format x-axis labels to show time in HH:MM:SS format
    ax.xaxis.set_major_formatter(ticker.FuncFormatter(lambda x, _: f'{int(x//3600):02}:{int((x%3600)//60):02}:{int(x%60):02}'))
    
    # Enable gridlines
    ax.grid(True, which='both', axis='both', linestyle='--', linewidth=0.5)  # Dashed lines for both major and minor grids

    # Label the plot
    ax.set_xlabel('Time')
    ax.set_ylabel('RSSI (dBm)')
    ax.set_title(f'RSSI over Time for {device}')
    ax.legend(title='Rotation')
    
    # Show the plot for the current device
    plt.show()
