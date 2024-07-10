import asyncio
from bleak import BleakScanner

async def scan_for_devices(time):
    devices = await BleakScanner.discover(timeout=time)
    for d in devices:
        print(f"MAC Address: {d.address}, Name: {d.name}, Details: {d.details}")

async def main():
    while True:
        await scan_for_devices(5)  # Scan for 5 seconds
        await asyncio.sleep(1)  # Short pause before the next scan

asyncio.run(main())
