import pandas as pd
import re
from datetime import datetime
import plotly.graph_objects as go

# Define the log file path
log_file = "screen_output.log"

# Parsing function
def parse_logs(log_file):
    data = {
        "Timestamp": [],
        "Device": [],
        "Event_Type": [],
        "Details": [],
        "RSSI": [],
        "Counter": []
    }

    # Regex patterns to match log entries
    rotation_pattern = re.compile(r"\[(\d{2}:\d{2}:\d{2})\].*Rotated to device name: (\w+)_\d+ \(Counter: (\d+), (\w+)")
    observation_pattern = re.compile(r"Device: (\w+), RSSI: (-?\d+), Observed at: \[(\d{2}:\d{2}:\d{2})]")

    with open(log_file, "r") as file:
        for line in file:
            # Match rotations
            rotation_match = rotation_pattern.search(line)
            if rotation_match:
                timestamp, device, counter, rotation_type = rotation_match.groups()
                data["Timestamp"].append(timestamp)
                data["Device"].append(device)
                data["Event_Type"].append("Rotation")
                data["Details"].append(rotation_type)
                data["RSSI"].append(None)
                data["Counter"].append(int(counter))
                continue

            # Match observations
            observation_match = observation_pattern.search(line)
            if observation_match:
                device, rssi, timestamp = observation_match.groups()
                data["Timestamp"].append(timestamp)
                data["Device"].append(device)
                data["Event_Type"].append("Observation")
                data["Details"].append(None)
                data["RSSI"].append(int(rssi))
                data["Counter"].append(None)

    return pd.DataFrame(data)

# Parse the logs
df = parse_logs(log_file)

# Convert timestamps to datetime for sorting and plotting
df["Timestamp"] = pd.to_datetime(df["Timestamp"], format="%H:%M:%S")

# Save structured data to a CSV for verification
df.to_csv("parsed_log_data.csv", index=False)

# Print the parsed data for review
print(df)
