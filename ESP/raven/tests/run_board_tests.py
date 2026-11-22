"""Reset the connected prototype, capture its tests, then watch for runtime faults.

Run with the ESP-IDF Python environment (pyserial). Only use on a development
board running Raven RND; resetting a board interrupts its current firmware.
"""
import argparse
import pathlib
import time

import serial


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--timeout", type=float, default=40)
    parser.add_argument("--soak", type=float, default=15)
    parser.add_argument("--log", type=pathlib.Path, default=pathlib.Path("build/rnd-board-tests.log"))
    args = parser.parse_args()
    args.log.parent.mkdir(parents=True, exist_ok=True)
    passed = False
    failed = False
    tests = set()
    boots = 0
    deadline = time.monotonic() + args.timeout
    with serial.Serial(args.port, 115200, timeout=0.25) as port, args.log.open("w", encoding="utf-8") as log:
        port.dtr = False
        port.rts = True
        time.sleep(0.15)
        port.reset_input_buffer()
        port.rts = False
        while time.monotonic() < deadline:
            text = port.readline().decode("utf-8", errors="replace").rstrip()
            if not text:
                continue
            print(text, flush=True)
            log.write(text + "\n")
            log.flush()
            if text.startswith("rst:"):
                boots += 1
                failed |= boots > 1
            if " TEST " in text and " PASS" in text:
                tests.add(text.split(" TEST ", 1)[1].split()[0])
            if any(error in text for error in (" FAIL", "Guru Meditation", "abort()", "watchdog got triggered", "Stack canary")):
                failed = True
            if "RND_SELF_TEST PASS" in text and not passed:
                passed = True
                deadline = time.monotonic() + args.soak
    print(f"Board tests: {len(tests)} passed; completed={passed}; runtime_fault={failed}")
    return 0 if passed and not failed and len(tests) >= 20 else 1


if __name__ == "__main__":
    raise SystemExit(main())
