"""Validate a stable four-board capture from RAVEN_VALIDATION_MONITOR builds.

Checks actual peer freshness, immutable boot/session bindings, resource margins
and logging continuity. The initial reconnect window is excluded explicitly.
"""
import argparse
import json
import pathlib
import re

TIMED = re.compile(r"^\s*(\d+\.\d+)\s+(.*)$")
METRICS = re.compile(r"Validation: sessions=(\d+) free=(\d+) min=(\d+) largest=(\d+) stack=(\d+) stale=(\d+) drops=(\d+)")
PEER = re.compile(r"Validation: peer=([0-9A-F]+) boot=([0-9A-F]+) state=(\S+) slot=(\d+) gen=(\d+) seen=(\d+)")
FAULT = re.compile(r"Guru Meditation|assert failed|panic|watchdog|rst:|event=[56] ", re.IGNORECASE)


def inspect(path, expected, start, seconds):
    snapshots = []
    current = None
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        timed = TIMED.match(raw)
        if not timed:
            continue
        elapsed, line = float(timed[1]), timed[2]
        if elapsed < start:
            continue
        if FAULT.search(line):
            raise AssertionError(f"{path.name} unexpected fault/transition at {elapsed}: {line}")
        metrics = METRICS.search(line)
        if metrics:
            current = {"time": elapsed, "metrics": tuple(map(int, metrics.groups())), "peers": {}}
            snapshots.append(current)
        peer = PEER.search(line)
        if peer and current:
            identity, boot, state, slot, generation, seen = peer.groups()
            if state == "disconnected":
                continue  # retained offline history is not an active binding
            if identity in current["peers"]:
                raise AssertionError(f"{path.name}: duplicate active identity {identity}")
            current["peers"][identity] = (boot, state, int(slot), int(generation), int(seen))
    if len(snapshots) < 2:
        raise AssertionError(f"{path.name}: missing validation snapshots")
    duration = snapshots[-1]["time"] - snapshots[0]["time"]
    if duration < seconds:
        raise AssertionError(f"{path.name}: observed {duration:.1f}s, required {seconds}s")
    previous = None
    for sample in snapshots:
        sessions, free, minimum, largest, stack, _, drops = sample["metrics"]
        if sessions != expected or len(sample["peers"]) != expected:
            raise AssertionError(f"{path.name} at {sample['time']}: expected {expected} healthy sessions")
        if min(free, minimum) < 32768 or largest < 8192 or stack < 1024:
            raise AssertionError(f"{path.name}: resource margin failed: {sample['metrics']}")
        if previous and drops != previous["metrics"][6]:
            raise AssertionError(f"{path.name}: diagnostics dropped during the stable interval: {drops}")
        for identity, binding in sample["peers"].items():
            if binding[1] != "ready":
                raise AssertionError(f"{path.name}: peer {identity} is {binding[1]}")
            if previous:
                old = previous["peers"].get(identity)
                if not old or binding[:4] != old[:4] or binding[4] <= old[4]:
                    raise AssertionError(f"{path.name}: binding changed or keepalive stopped for {identity}")
        if previous and sample["time"] - previous["time"] > 15:
            raise AssertionError(f"{path.name}: capture gap exceeded 15 seconds")
        previous = sample
    return {
        "port": path.stem, "snapshots": len(snapshots), "stable_seconds": round(duration, 1),
        "minimum_heap": min(s["metrics"][2] for s in snapshots),
        "minimum_largest_block": min(s["metrics"][3] for s in snapshots),
        "minimum_stack": min(s["metrics"][4] for s in snapshots),
        "diagnostic_drops_before_interval": snapshots[0]["metrics"][6],
        "peers": sorted(snapshots[-1]["peers"]),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=pathlib.Path)
    parser.add_argument("--mc", required=True)
    parser.add_argument("--nodes", nargs="+", required=True)
    parser.add_argument("--from-second", type=float, default=20)
    parser.add_argument("--seconds", type=float, default=1800)
    args = parser.parse_args()
    reports = [inspect(args.directory / f"{args.mc}.log", len(args.nodes), args.from_second, args.seconds)]
    reports += [inspect(args.directory / f"{port}.log", 1, args.from_second, args.seconds) for port in args.nodes]
    controllers = {report["peers"][0] for report in reports[1:]}
    if len(controllers) != 1 or controllers.intersection(reports[0]["peers"]):
        raise AssertionError("Nodes do not agree on one separate MC identity")
    print(json.dumps({"result": "PASS", "boards": reports}, indent=2))


if __name__ == "__main__":
    main()
