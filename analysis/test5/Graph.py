import pandas as pd
from datetime import datetime, timedelta
import plotly.graph_objects as go
import os
import re
import itertools

def parse_log(file):
    data = {}
    if not os.path.exists(file):
        print(f"File not found: {file}")
        return data

    with open(file, 'r') as f:
        for line in f:
            line = line.strip()
            match = re.search(r'\[Filtered Device\]: (\S+) \(random\), RSSI: (-?\d+), Name: (Iris\d+_\d+), Time: (\d{2}:\d{2}:\d{2})', line)
            if match:
                addr, rssi, device_name, time_str = match.groups()
                time = datetime.strptime(time_str, '%H:%M:%S')
                iris_group = '_'.join(device_name.split('_')[:-1])

                if iris_group not in data:
                    data[iris_group] = {'Time': [], 'RSSI': [], 'Rotation': []}

                # Convert time to total minutes for 1-minute intervals
                time_in_minutes = time.hour * 60 + time.minute
                data[iris_group]['Time'].append(time_in_minutes)
                data[iris_group]['RSSI'].append(int(rssi))
                data[iris_group]['Rotation'].append(device_name)

    return data

def high_contrast_colors():
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
    match = re.search(r'(\d+)', rotation)
    return int(match.group(1)) if match else 0

log_file = r"screen_output.log"

data = parse_log(log_file)

if not data:
    print("No data parsed from the log file.")
else:
    fig = go.Figure()

    iris_groups = list(data.keys())
    colors = high_contrast_colors()
    color_cycle = itertools.cycle(colors)

    for iris_group, device_data in data.items():
        df = pd.DataFrame({
            'Time': device_data['Time'],
            'RSSI': device_data['RSSI'],
            'Rotation': device_data['Rotation']
        })

        df['Rotation'] = df['Rotation'].astype(str)
        sorted_rotations = sorted(df['Rotation'].unique(), key=extract_number_from_rotation)

        for rotation in sorted_rotations:
            rotation_df = df[df['Rotation'] == rotation]
            if not rotation_df.empty:
                fig.add_trace(go.Scatter(
                    x=rotation_df['Time'],  # Time in minutes
                    y=rotation_df['RSSI'],
                    mode='lines+markers',
                    name=f'{iris_group} - {rotation}',
                    line=dict(color=next(color_cycle)),
                    text=rotation_df['Rotation'],
                    hovertemplate='Time (minutes): %{x}<br>RSSI: %{y}<br>Rotation: %{text}<extra></extra>'
                ))

    fig.update_layout(
        title='RSSI Over Time with Rotation Types',
        xaxis_title='Time (minutes)',
        yaxis_title='RSSI (dBm)',
        xaxis=dict(range=[0, 30]),  # Set the range for 30 minutes
        paper_bgcolor='rgba(30, 30, 30, 1)',
        plot_bgcolor='rgba(40, 40, 40, 1)',
        font=dict(color='white')
    )

    fig.show()
