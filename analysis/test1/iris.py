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
    fifteen_minutes = timedelta(minutes=10)

    if not os.path.exists(file):
        print(f"File not found: {file}")
        return data

    # Updated regex pattern to match lines with "Iris" names and RSSI values
    pattern = re.compile(r'\[(\d{2}:\d{2}:\d{2})\].*RSSI (-?\d+).*Name: (Iris\d+)')

    with open(file, 'r') as f:
        for line in f:
            line = line.strip()
            
            # Try matching the pattern
            match = pattern.search(line)
            if match:
                time_str, rssi, device_name = match.groups()
                time = datetime.strptime(time_str, '%H:%M:%S')

                if start_time is None:
                    start_time = time
                    end_time = start_time + fifteen_minutes

                if start_time <= time <= end_time:
                    rssi = int(rssi)
                    if device_name not in data:
                        data[device_name] = {'Time': [], 'RSSI': []}

                    # Append data for the device
                    data[device_name]['Time'].append(time)
                    data[device_name]['RSSI'].append(rssi)

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

log_file = r"Observer_no_rotation.log"  # Use the path of the log file you want to process

data = parse_log(log_file)

if not data:
    print("No data parsed from the log file.")
else:
    fig = go.Figure()

    # Collect all unique Iris groups
    iris_groups = list(data.keys())
    colors = high_contrast_colors()
    color_cycle = itertools.cycle(colors)
    
    # Loop over each Iris group
    for iris_group, device_data in data.items():
        df = pd.DataFrame({
            'Time': device_data['Time'],
            'RSSI': device_data['RSSI']
        })

        fig.add_trace(go.Scatter(
            x=df['Time'],
            y=df['RSSI'],
            mode='lines+markers',
            name=f'{iris_group}',
            line=dict(color=next(color_cycle)),  # Use a high-contrast color
            hovertemplate='Time: %{x}<br>RSSI: %{y}<extra></extra>'
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
        title='Test_1:RSSI Over Time for Each Iris Device (No Rotation)',
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
