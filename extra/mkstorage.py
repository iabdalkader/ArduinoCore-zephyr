#!/usr/bin/env python3
"""
Deploy ELF files to target via LittleFS + ADB + OpenOCD.
"""

import argparse
import json
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from littlefs import LittleFS
from adb_shell.adb_device import AdbDeviceUsb


def build_image(elf_files, block_size, block_count):
    """Build LittleFS image in memory, return (image_bytes, manifest)."""
    fs = LittleFS(block_size=block_size, block_count=block_count)
    manifest = []

    for entry in elf_files:
        # Parse path[:flags]
        if ":" in entry:
            elf_path, flags_str = entry.rsplit(":", 1)
            flags = int(flags_str, 0)
        else:
            elf_path, flags = entry, 0

        elf = Path(elf_path)
        if not elf.is_file():
            sys.exit(f"Error: {elf} not found")

        # Write ELF to filesystem
        with fs.open(elf.name, "wb") as f:
            f.write(elf.read_bytes())

        # Add to manifest (strip .ino.elf -> name)
        name = elf.stem
        if name.endswith(".ino"):
            name = name[:-4]

        manifest.append({
            "name": name,
            "path": f"/storage/{elf.name}",
            "flags": flags,
        })

    # Write manifest
    with fs.open("manifest.json", "w") as f:
        f.write(json.dumps(manifest, indent=2))

    return bytes(fs.context.buffer), manifest


def check_adb_server():
    """Check if ADB server is running and prompt user to kill it."""
    try:
        result = subprocess.run(["pgrep", "-x", "adb"], capture_output=True)
        if result.returncode == 0:
            answer = input("ADB server is running. Kill it? [y/N] ").strip().lower()
            if answer == "y":
                subprocess.run(["killall", "adb"])
                # Wait for the process to exit and release the USB device
                for _ in range(10):
                    if subprocess.run(["pgrep", "-x", "adb"], capture_output=True).returncode != 0:
                        break
                    time.sleep(0.5)
                else:
                    sys.exit("Error: failed to kill ADB server.")
                time.sleep(1)
                print("ADB server killed.")
            else:
                sys.exit("Aborted: ADB server must be stopped for USB access.")
    except FileNotFoundError:
        pass


def flash(image, address, serial=None, timeout=120):
    """Flash image to target via ADB + OpenOCD."""
    remote = "/tmp/remoteocd/storage.img"

    dev = AdbDeviceUsb(serial=serial, default_transport_timeout_s=timeout)
    dev.connect(auth_timeout_s=5)

    try:
        print("Pushing image to board...")
        dev.shell("mkdir -p /tmp/remoteocd")
        with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as tmp:
            tmp.write(image)
            tmp.flush()
            dev.push(tmp.name, remote)

        print(f"Flashing to {address:#x}...")
        cmd = (
            f"/opt/openocd/bin/openocd -s /opt/openocd/ -f openocd_gpiod.cfg "
            f"-c 'init; reset halt; flash write_image erase {remote} {address:#x}; "
            f"reset run; shutdown'"
        )
        for line in dev.streaming_shell(cmd, transport_timeout_s=timeout, read_timeout_s=timeout):
            print(line, end="", flush=True)
    finally:
        dev.close()


def main():
    parser = argparse.ArgumentParser(description="Deploy ELF files to target storage")
    parser.add_argument("elf_files", nargs="*",
                   help="ELF files (path[:flags]). flags: 0=deferred (default), 1=boot")
    parser.add_argument("-s", "--serial", default=None,
                   help="ADB device serial number (default: first device found)")
    parser.add_argument("-a", "--address", type=lambda x: int(x, 0), default=0x081c0000,
                   help="Storage address (default: 0x081c0000)")
    parser.add_argument("-b", "--block-size", type=int, default=8192, help="Block size (default: 8192)")
    parser.add_argument("-c", "--block-count", type=int, default=32, help="Block count (default: 32)")
    parser.add_argument("-o", "--output", help="Save image to file")
    parser.add_argument("-t", "--timeout", type=int, default=120, help="ADB timeout in seconds (default: 120)")
    parser.add_argument("-n", "--no-flash", action="store_true", help="Don't flash, just build image")

    args, extra = parser.parse_known_args()
    args.elf_files.extend(extra)

    if not args.elf_files:
        parser.error("No ELF files provided")

    print("Building image...")
    image, manifest = build_image(args.elf_files, args.block_size, args.block_count)

    print("\nManifest:")
    print(json.dumps(manifest, indent=2))
    print()

    if args.output:
        Path(args.output).write_bytes(image)
        print(f"Saved to {args.output}")

    if not args.no_flash:
        check_adb_server()
        flash(image, args.address, serial=args.serial, timeout=args.timeout)
        print("Done.")


if __name__ == "__main__":
    main()
