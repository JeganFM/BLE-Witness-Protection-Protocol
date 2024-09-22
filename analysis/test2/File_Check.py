import os

file_path = '\Observer_rotation_1m.log'
if os.path.exists(file_path):
    print(f"File exists: {file_path}")
else:
    print(f"File not found: {file_path}")