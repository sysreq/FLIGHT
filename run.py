import subprocess
import os
import re
import serial
import serial.tools.list_ports
import time
import sys
import argparse
from pathlib import Path

class PicoFlashMonitor:
    def __init__(self):
        self.serial_conn = None

    def extract_chip_id(self, picotool_output):
        """Extract and format chip ID from picotool output."""
        match = re.search(r'chipid:\s+0x([0-9a-fA-F]+)', picotool_output)
        return match.group(1).upper() if match else None

    def run_picotool_command(self, command):
        """Execute a picotool command"""
        picotool_path = os.path.expanduser("~/.pico-sdk/picotool/2.2.0-a4/picotool/picotool.exe")
        print(f"Running: picotool {' '.join(command)}")
        result = subprocess.run(
            [picotool_path] + command,
            capture_output=True,
            text=True,
        )
    
        if result.returncode == 0:
            print(result.stdout)
            chip_id = self.extract_chip_id(result.stdout)
            if chip_id:
                print(f"\nChip ID: {chip_id}")
                return chip_id
        else:
            print("No devices found or error occurred")
            sys.exit(0)

    def list(self):
        self.run_picotool_command(['info', '-d'])
        sys.exit(0)

    def reboot(self):
        chip_id = self.run_picotool_command(['reboot', '-fu'])
        sys.exit(0)

def main():
    parser = argparse.ArgumentParser(
        description='Flash and monitor Raspberry Pi Pico 2'
    )
    parser.add_argument(
        'elf_file',
        type=str,
        nargs='?',
        help='Path to the ELF file to flash'
    )
    parser.add_argument(
        '-l', '--list',
        action='store_true',
        help='List attached devices'
    )
    parser.add_argument(
        '-r', '--reboot',
        action='store_true',
        help='List attached devices'
    )
    
    args = parser.parse_args()
    monitor = PicoFlashMonitor()

    if(args.list is True):
        monitor.list()

    if(args.reboot is True):
        monitor.reboot()

    if not args.elf_file:
        parser.error("elf_file is required!")
        sys.exit(0)

    success = True
    
    exit(0 if success else 1)


if __name__ == '__main__':
    main()