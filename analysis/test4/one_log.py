import matplotlib.pyplot as plt
import pandas as pd
from datetime import datetime
import numpy as np

# Function to parse the log and extract time, RSSI values, and names
def parse_log(file):
    times = []
    rssi_values = []
    names = []  # Add names list to store rotations

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

                rssi = int(parts[8].strip(',
