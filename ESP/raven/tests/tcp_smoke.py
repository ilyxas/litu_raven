"""Exercise the RND protocol over an actual TCP connection; no iOS changes.

python tests/tcp_smoke.py --host 192.168.4.1
"""
import argparse
import socket
import struct
import time

ACK, NACK, SNAPSHOT = 0x7F01, 0x7F02, 0x7F03


def receive_exact(sock, size):
    result = bytearray()
    while len(result) < size:
        chunk = sock.recv(size - len(result))
        if not chunk:
            raise EOFError("connection closed")
        result.extend(chunk)
    return bytes(result)


def exchange(sock, msg_id, payload, fragmented=False):
    frame = struct.pack("<HI", msg_id, len(payload)) + payload
    if fragmented:
        for byte in frame:
            sock.sendall(bytes([byte]))
    else:
        sock.sendall(frame)
    kind, size = struct.unpack("<HI", receive_exact(sock, 6))
    if size > 128:
        raise ValueError("oversized response")
    return kind, receive_exact(sock, size)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", required=True)
    parser.add_argument("--port", type=int, default=8080)
    args = parser.parse_args()
    # Unique operation IDs for each run; firmware retains the last 16 results.
    seed = int(time.time_ns() // 1000) % 0x1FFFFFFF + 1
    with socket.create_connection((args.host, args.port), timeout=3) as sock:
        for arm, angle in ((0, 45.0), (1, -90.0)):
            kind, payload = exchange(sock, 0x1101, struct.pack("<IBf", seed + arm, arm, angle), True)
            assert kind == ACK and struct.unpack("<IHBBB", payload)[3] == 1 << arm
        end = time.monotonic() + 5
        complete = set()
        while time.monotonic() < end and len(complete) != 2:
            for arm in (0, 1):
                kind, payload = exchange(sock, 0x1105, struct.pack("<IBI", seed + 2, arm, seed + arm))
                assert kind == SNAPSHOT
                _, _, operation, status, position, target, _, _ = struct.unpack("<IBIBffBQ", payload)
                if status == 2:
                    assert operation == seed + arm and abs(position - target) < 0.02
                    complete.add(arm)
                elif status >= 3:
                    raise AssertionError(f"arm {arm} failed with status {status}")
            time.sleep(0.05)
        assert len(complete) == 2, "motion did not finish"
        kind, _ = exchange(sock, 0x1101, struct.pack("<I", seed + 3))
        assert kind == NACK, "malformed payload was accepted"
        kind, _ = exchange(sock, 0x1104, struct.pack("<IB", seed + 4, 7))
        assert kind == ACK
    print("PASS: fragmented TCP, independent motion, status polling, NACK, critical cut")


if __name__ == "__main__":
    main()
