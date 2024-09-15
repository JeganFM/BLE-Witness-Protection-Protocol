import os

file_path = '\SyncI6_Test_4B.log'
if os.path.exists(file_path):
    print(f"File exists: {file_path}")
else:
    print(f"File not found: {file_path}")
