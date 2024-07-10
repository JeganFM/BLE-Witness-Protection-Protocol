import asyncio
from bleak import BleakScanner
from datetime import datetime

async def scan_for_devices(time):
    devices = await BleakScanner.discover(timeout=time)
    print(f"\n--- Scan at {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} ---")  # Header
    for d in devices:
        if d.name:
            print(f"MAC Address: {d.address}, Name: {d.name}, Details: {d.details}")
    print("-" * 20)  # Separator

async def main():
    while True:
        await scan_for_devices(5)  # Scan for 5 seconds
        await asyncio.sleep(1)  # Short pause before the next scan

asyncio.run(main())