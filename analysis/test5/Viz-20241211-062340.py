import pandas as pd
from datetime import datetime
import plotly.graph_objects as go
import os
import re
import itertools

def parse_log(file):
    data = {}
    seen_rotations = set()

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

                if device_name in seen_rotations:
                    continue
                seen_rotations.add(device_name)

                if iris_group not in data:
                    data[iris_group] = {'Time': [], 'RSSI': [], 'Rotation': []}

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

def determine_rotation_type(rotation, time):
    if time % 10 == 0:
        return 'Sync Signal'
    elif time % 5 == 0:
        return 'Automated Rotation'
    else:
        return ' Observation Rotation'

log_file = r"$RZ5PLPR.log"
data = parse_log(log_file)

if not data:
    print("No data parsed from the log file.")
else:
    fig = go.Figure()
    iris_groups = list(data.keys())
    colors = high_contrast_colors()
    color_cycle = itertools.cycle(colors)
    rotation_colors = {}

    # Pre-assign colors to each unique rotation
    for iris_group, device_data in data.items():
        for rotation in device_data['Rotation']:
            if rotation not in rotation_colors:
                rotation_colors[rotation] = next(color_cycle)

    trace_visibility_mapping = {group: [] for group in iris_groups}

    for iris_group, device_data in data.items():
        df = pd.DataFrame({
            'Time': device_data['Time'],
            'RSSI': device_data['RSSI'],
            'Rotation': device_data['Rotation']
        })

        for i in range(len(df) - 1):
            current_rotation = df['Rotation'].iloc[i]
            color = rotation_colors[current_rotation]
            segment = go.Scatter(
                x=df['Time'].iloc[i:i+2],
                y=df['RSSI'].iloc[i:i+2],
                mode='lines',
                line=dict(color=color),
                name=f'{iris_group} - Segment {i+1}',
                showlegend=False
            )
            fig.add_trace(segment)
            trace_visibility_mapping[iris_group].append(True)

        for i, rotation in enumerate(df['Rotation'].unique()):
            rotation_df = df[df['Rotation'] == rotation]
            color = rotation_colors[rotation]

            rotation_time = rotation_df['Time'].iloc[0]
            rotation_type = determine_rotation_type(rotation, rotation_time)
            
            if rotation_type == 'Sync Signal':
                marker_shape = 'circle'  # Sync signal rotations
            elif rotation_type == 'Automated Rotation':
                marker_shape = 'square'  # Automated rotations
            else:
                marker_shape = 'diamond'  # Default for Observation rotations
                
            trace = go.Scatter(
                x=rotation_df['Time'],
                y=rotation_df['RSSI'],
                mode='markers',
                name=f'{iris_group} - {rotation}',
                marker=dict(color=color, size=8, symbol=marker_shape),
                text=rotation_df['Rotation'],
                hovertemplate=(
                    'Time (minutes): %{x}<br>'
                    'RSSI: %{y}<br>'
                    'Rotation: %{text}<br>'
                    f'Rotation Type: {rotation_type}<extra></extra>'
                )
            )
            fig.add_trace(trace)
            trace_visibility_mapping[iris_group].append(True)

    dropdown_buttons = [
        {
            'label': 'Show All',
            'method': 'update',
            'args': [{'visible': [True] * len(fig.data)}]
        }
    ]

    for iris_group, visibility_flags in trace_visibility_mapping.items():
        dropdown_buttons.append({
            'label': iris_group,
            'method': 'update',
            'args': [{'visible': [
                flag if group == iris_group else False
                for group, flags in trace_visibility_mapping.items()
                for flag in flags
            ]}]
        })

    dropdown_buttons.append({
        'label': 'Hide All',
        'method': 'update',
        'args': [{'visible': [False] * len(fig.data)}]
    })

    fig.update_layout(
        title='Test 5: RSSI Over Time with Rotation (Sync Signal, Automated Rotation, Observation Rotation) ',
        xaxis_title='Time (minutes)',
        yaxis_title='RSSI (dBm)',
        xaxis=dict(range=[0, 30]),
        paper_bgcolor='rgba(30, 30, 30, 1)',
        plot_bgcolor='rgba(40, 40, 40, 1)',
        font=dict(color='white'),
        updatemenus=[{
            'buttons': dropdown_buttons,
            'direction': 'down',
            'showactive': True,
        }]
    )

    fig.show()
