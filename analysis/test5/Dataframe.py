import pandas as pd

# Load the CSV file
file_path = "parsed_log_data.csv"  # Replace with the actual path to your file
df = pd.read_csv(file_path)

# Ensure Timestamp is in a datetime format
df["Timestamp"] = pd.to_datetime(df["Timestamp"], format="%H:%M:%S")
