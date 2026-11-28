"""Capture timestamped UART logs from independent boards; no console commands.

Requires pyserial (included in the ESP-IDF Python environment).
Reset is opt-in and uses the usual RTS/EN wiring, including USB Serial/JTAG.
"""
import argparse
import json
import pathlib
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ports", nargs="+")
    parser.add_argument("--seconds", type=float, default=60)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--reset", nargs="*", default=[])
    parser.add_argument("--rom-reset", nargs="*", default=[], help="Reset through esptool ROM commands; useful for native USB")
    parser.add_argument("--actions", type=pathlib.Path, help="JSON list: {at: seconds, port: COMx, action: hold|restart}")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    actions = sorted(json.loads(args.actions.read_text()) if args.actions else [], key=lambda action: action["at"])
    devices = []
    held = set()
    try:
        for port in args.ports:
            device = serial.Serial(port=None, baudrate=115200, timeout=0)
            device.dtr = False
            device.rts = False
            device.port = port
            device.open()
            log = (args.output / f"{port}.log").open("w", encoding="utf-8")
            devices.append((port, device, log, bytearray()))
        for port, device, _, _ in devices:
            if port in args.rom_reset:
                from esptool import detect_chip
                chip = detect_chip(device)
                chip.hard_reset()
                device.timeout = 0
            elif port in args.reset:
                device.rts = True
                time.sleep(0.15)
                device.rts = False
        start = time.monotonic()
        while time.monotonic() - start < args.seconds:
            if actions and time.monotonic() - start >= actions[0]["at"]:
                action = actions.pop(0)
                from esptool import detect_chip
                device = next(item[1] for item in devices if item[0] == action["port"])
                if action["action"] not in ("hold", "restart"):
                    raise ValueError(f"Invalid test action: {action}")
                print(f"ACTION {action}", flush=True)
                chip = detect_chip(device, connect_mode="no-reset" if action["port"] in held else "default-reset", connect_attempts=2)
                if action["action"] == "restart":
                    chip.hard_reset()
                    held.discard(action["port"])
                else:
                    held.add(action["port"])
                device.timeout = 0
                # ROM holds stop the application/radio without a GPIO actuator API.
                # This is a test instrument, not a production control protocol.
            for port, device, log, pending in devices:
                pending.extend(device.read(8192))
                while b"\n" in pending:
                    raw, _, rest = pending.partition(b"\n")
                    pending[:] = rest
                    line = raw.decode("utf-8", errors="replace").rstrip("\r")
                    text = f"{time.monotonic() - start:9.3f} {line}"
                    log.write(text + "\n")
                    log.flush()
                    if any(key in line for key in ("Raven", "Budget", "BoardProbe", "Error", "panic", "assert", "watchdog", "rst:")):
                        print(f"{port} {text}", flush=True)
            time.sleep(0.01)
    finally:
        for _, device, log, pending in devices:
            if pending:
                log.write(pending.decode("utf-8", errors="replace"))
            log.close()
            device.close()


if __name__ == "__main__":
    main()
