import re
import matplotlib.pyplot as plt
from collections import defaultdict

# Define the file path for the log file and the output image prefix
log_file_path = 'screen_output.log'
output_image_prefix = 'rssi_over_time'

# Initialize a dictionary to store the parsed time (by minute) and RSSI values for each device
devices_data = {
    'My_Broadcaster': defaultdict(list),
    'My_Broadcaster_2': defaultdict(list),
    'My_Broadcaster_3': defaultdict(list)
}

# Define a regular expression to extract the minute, RSSI values, and device name
log_pattern = re.compile(r'\[(\d+:\d+):\d+\].*RSSI (-\d+).*Name: (My_Broadcaster(?:_2|_3)?)')

# Open the log file and parse its content
with open(log_file_path, 'r') as file:
    for line in file:
        match = log_pattern.search(line)
        if match:
            minute = match.group(1)  # Extract time up to the minute (HH:MM)
            rssi = int(match.group(2))
            device_name = match.group(3)
            if device_name in devices_data:
                devices_data[device_name][minute].append(rssi)

# Plot the graphs for each device
colors = ['blue', 'green', 'red']
for i, (device_name, data) in enumerate(devices_data.items()):
    minutes = []
    avg_rssi_values = []
    for minute, rssi_list in sorted(data.items()):
        avg_rssi = sum(rssi_list) / len(rssi_list)
        minutes.append(minute)
        avg_rssi_values.append(avg_rssi)
    
    plt.figure(figsize=(10, 5))
    plt.plot(minutes, avg_rssi_values, marker='o', linestyle='-', color=colors[i])
    plt.xlabel('Time (HH:MM)')
    plt.ylabel('Average RSSI (dBm)')
    plt.title(f'Average RSSI per Minute for {device_name}')
    plt.xticks(rotation=45)  # Rotate the x-axis labels if necessary
    plt.grid(True)
    
    # Save the figure
    plt.savefig(f'{output_image_prefix}_{device_name}.png', bbox_inches='tight')
    
    # Optionally, display the figure (remove if only saving is needed)
    plt.show()