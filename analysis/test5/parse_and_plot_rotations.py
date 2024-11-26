import pandas as pd
from datetime import datetime
import plotly.graph_objects as go
import os
import re
import itertools

# Parse the log file for rotation and observation data
def parse_log(file):
    data = {
        'rotations': [],  # Store rotation data
        'observations': []  # Store device observations
    }

    if not os.path.exists(file):
        print(f"File not found: {file}")
        return data

    # Define regex patterns for rotation and observations
    rotation_pattern = re.compile(r'\[(\d{2}:\d{2}:\d{2})\] Rotated to device name: (\w+),.*Rotation: (\w+)')
    observation_pattern = re.compile(r'\[(\d{2}:\d{2}:\d{2})\] Observed device: (\w+), RSSI: (-?\d+)')

    with open(file, 'r') as f:
        for line in f:
            line = line.strip()
            # Match rotation logs
            rotation_match = rotation_pattern.search(line)
            if rotation_match:
                time_str, device_name, rotation_type = rotation_match.groups()
                timestamp = datetime.strptime(time_str, '%H:%M:%S')
                data['rotations'].append({
                    'timestamp': timestamp,
                    'device_name': device_name,
                    'rotation_type': rotation_type
                })

            # Match observation logs
            observation_match = observation_pattern.search(line)
            if observation_match:
                time_str, device_name, rssi = observation_match.groups()
                timestamp = datetime.strptime(time_str, '%H:%M:%S')
                data['observations'].append({
                    'timestamp': timestamp,
                    'device_name': device_name,
                    'rssi': int(rssi)
                })

    return data

# Generate high-contrast colors for graphs
def high_contrast_colors():
    return [
        'rgba(255, 0, 0, 1)',    # Red
        'rgba(0, 255, 0, 1)',    # Green
        'rgba(0, 0, 255, 1)',    # Blue
        'rgba(255, 255, 0, 1)',  # Yellow
        'rgba(255, 0, 255, 1)',  # Magenta
        'rgba(0, 255, 255, 1)',  # Cyan
    ]

# Plot rotation and observation data
def plot_rotations_and_observations(data):
    fig = go.Figure()

    # Separate rotations and observations for easier plotting
    rotation_df = pd.DataFrame(data['rotations'])
    observation_df = pd.DataFrame(data['observations'])

    # Add rotation events to the graph
    colors = high_contrast_colors()
    color_cycle = itertools.cycle(colors)

    if not rotation_df.empty:
        for device_name, group in rotation_df.groupby('device_name'):
            fig.add_trace(go.Scatter(
                x=group['timestamp'],
                y=group.index,  # Use index as a stand-in for rotation count
                mode='lines+markers',
                name=f'{device_name} Rotations',
                line=dict(color=next(color_cycle)),
                text=group['rotation_type'],
                hovertemplate='Time: %{x}<br>Rotation Type: %{text}<extra></extra>'
            ))

    # Add observation data
    if not observation_df.empty:
        for device_name, group in observation_df.groupby('device_name'):
            fig.add_trace(go.Scatter(
                x=group['timestamp'],
                y=group['rssi'],
                mode='markers',
                name=f'{device_name} Observations',
                marker=dict(size=8, color=next(color_cycle)),
                hovertemplate='Time: %{x}<br>RSSI: %{y}<br>Device: %{text}<extra></extra>'
            ))

    # Configure layout
    fig.update_layout(
        title='Rotations and Observations',
        xaxis_title='Time',
        yaxis_title='RSSI / Rotation Count',
        legend_title='Legend',
        paper_bgcolor='rgba(30, 30, 30, 1)',
        plot_bgcolor='rgba(40, 40, 40, 1)',
        font=dict(color='white'),
        hovermode='closest'
    )

    fig.show()

# File to parse
log_file = "current_test_log.log"

# Parse the log file
parsed_data = parse_log(log_file)

# Plot the parsed data
if parsed_data:
    plot_rotations_and_observations(parsed_data)
else:
    print("No data to plot.")
