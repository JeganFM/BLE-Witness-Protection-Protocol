import pandas as pd
from datetime import datetime, timedelta
import plotly.graph_objects as go
import os
import re
import itertools

def parse_log(file):
    data = {}
    start_time = None
    end_time = None
    fifteen_minutes = timedelta(minutes=15)

    if not os.path.exists(file):
        print(f"File not found: {file}")
        return data

    # Define both regex patterns
    pattern1 = re.compile(r'\[(\d{2}:\d{2}:\d{2})\].*RSSI (-?\d+).*Name: (Iris\d+_\d+)')
    pattern2 = re.compile(r'\[(\d{2}:\d{2}:\d{2})\].*Iris(\d+_\d+), RSSI: (-?\d+)')

    with open(file, 'r') as f:
        for line in f:
            line = line.strip()
            # Try matching with pattern 1
            match = pattern1.search(line)
            if not match:
                # If no match with pattern 1, try pattern 2
                match = pattern2.search(line)
            if match:
                # Determine if pattern1 or pattern2 matched
                if len(match.groups()) == 3:  # pattern1 matched
                    time_str, rssi, device_name = match.groups()
                else:  # pattern2 matched
                    time_str, device_name, rssi = match.groups()

                time = datetime.strptime(time_str, '%H:%M:%S')

                if start_time is None:
                    start_time = time
                    end_time = start_time + fifteen_minutes

                if start_time <= time <= end_time:
                    rssi = int(rssi)
                    iris_group = '_'.join(device_name.split('_')[:-1])

                    if iris_group not in data:
                        data[iris_group] = {'Time': [], 'RSSI': [], 'Rotation': []}

                    # Append data for the group
                    data[iris_group]['Time'].append(time)
                    data[iris_group]['RSSI'].append(rssi)
                    data[iris_group]['Rotation'].append(device_name)  # Track device name as rotation

    return data

def high_contrast_colors():
    """Generate a list of high-contrast colors."""
    return [
        'rgba(255, 0, 0, 1)',    # Red
        'rgba(0, 255, 0, 1)',    # Green
        'rgba(0, 0, 255, 1)',    # Blue
        'rgba(255, 255, 0, 1)',  # Yellow
        'rgba(255, 0, 255, 1)',  # Magenta
        'rgba(0, 255, 255, 1)',  # Cyan
        'rgba(255, 165, 0, 1)',  # Orange
        'rgba(128, 0, 128, 1)',  # Purple
        'rgba(0, 128, 128, 1)'   # Teal
    ]

def extract_number_from_rotation(rotation):
    """Extracts the numerical part from the rotation string for sorting purposes."""
    match = re.search(r'(\d+)', rotation)
    return int(match.group(1)) if match else 0

log_file = r"SyncI5_Test_4D.log" 

data = parse_log(log_file)

if not data:
    print("No data parsed from the log file.")
else:
    fig = go.Figure()

    # Collect all unique Iris groups and rotations
    iris_groups = list(data.keys())
    colors = high_contrast_colors()
    color_cycle = itertools.cycle(colors)
    
    # Loop over each Iris group
    for iris_group, device_data in data.items():
        df = pd.DataFrame({
            'Time': device_data['Time'],
            'RSSI': device_data['RSSI'],
            'Rotation': device_data['Rotation']
        })

        # Sort rotations numerically
        df['Rotation'] = df['Rotation'].astype(str)
        sorted_rotations = sorted(df['Rotation'].unique(), key=extract_number_from_rotation)

        # Loop over each unique rotation and plot its data
        for rotation in sorted_rotations:
            rotation_df = df[df['Rotation'] == rotation]
            if not rotation_df.empty:
                fig.add_trace(go.Scatter(
                    x=rotation_df['Time'],
                    y=rotation_df['RSSI'],
                    mode='lines+markers',
                    name=f'{iris_group} - {rotation}',
                    line=dict(color=next(color_cycle)),  # Use a high-contrast color
                    text=rotation_df['Rotation'],
                    hovertemplate='Time: %{x}<br>RSSI: %{y}<br>Rotation: %{text}<extra></extra>'
                ))

    # Create dropdown buttons for each Iris group
    buttons = [
        {
            'label': 'Show All',
            'method': 'update',
            'args': [{'visible': [True] * len(fig.data)}]
        }
    ]
    
    # Add button for each Iris group
    for iris_group in iris_groups:
        visible_traces = [trace.name.startswith(iris_group) for trace in fig.data]
        buttons.append({
            'label': iris_group,
            'method': 'update',
            'args': [{'visible': visible_traces}]
        })
    
    buttons.append({
        'label': 'Hide All',
        'method': 'update',
        'args': [{'visible': [False] * len(fig.data)}]
    })

    fig.update_layout(
        title='RSSI Over Time for Each Iris Group and Rotation (First 15 Minutes)',
        xaxis_title='Time',
        yaxis_title='RSSI (dBm)',
        xaxis=dict(
            tickformat='%H:%M:%S'
        ),
        paper_bgcolor='rgba(30, 30, 30, 1)',  # Dark grey background for the entire figure
        plot_bgcolor='rgba(40, 40, 40, 1)',   # Slightly lighter grey for the plot area
        updatemenus=[{
            'buttons': buttons,
            'direction': 'down',
            'showactive': True,
        }],
        font=dict(color='white')  # White text for better contrast
    )

    fig.show()
