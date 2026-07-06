#!/usr/bin/env python3
"""
ADM OSC spammer (no external dependencies).

Examples:
    python3 tools/adm_osc_spammer.py --port 4001 --object-id 1 --format xyz --rate 200
    python3 tools/adm_osc_spammer.py --port 4001 --object-id 1 --format aed --rate 200 --mode sine
"""

from __future__ import annotations

import argparse
import math
import random
import socket
import struct
import time
from typing import Iterable


def _pad4(data: bytes) -> bytes:
    pad = (4 - (len(data) % 4)) % 4
    return data + (b"\x00" * pad)


def osc_pack(address: str, values: Iterable[float]) -> bytes:
    addr = _pad4(address.encode("utf-8") + b"\x00")
    vals = list(values)
    typetags = _pad4(("," + ("f" * len(vals))).encode("ascii") + b"\x00")
    args = b"".join(struct.pack(">f", float(v)) for v in vals)
    return addr + typetags + args


def clamp(v: float, lo: float, hi: float) -> float:
    return lo if v < lo else hi if v > hi else v


def make_xyz(t: float, mode: str) -> tuple[float, float, float]:
    if mode == "random":
        return (
            random.uniform(-1.0, 1.0),
            random.uniform(-1.0, 1.0),
            random.uniform(-1.0, 1.0),
        )
    if mode == "sweep":
        p = (t * 0.5) % 1.0
        x = (p * 2.0) - 1.0
        y = 1.0 - (p * 2.0)
        z = math.sin(t * 2.0 * math.pi * 0.25)
        return clamp(x, -1.0, 1.0), clamp(y, -1.0, 1.0), clamp(z, -1.0, 1.0)

    # sine
    x = math.sin(t * 2.0 * math.pi * 0.61)
    y = math.cos(t * 2.0 * math.pi * 0.47)
    z = 0.7 * math.sin(t * 2.0 * math.pi * 0.29)
    return clamp(x, -1.0, 1.0), clamp(y, -1.0, 1.0), clamp(z, -1.0, 1.0)


def make_aed(t: float, mode: str) -> tuple[float, float, float]:
    if mode == "random":
        return (
            random.uniform(-180.0, 180.0),
            random.uniform(-90.0, 90.0),
            random.uniform(0.0, 1.0),
        )
    if mode == "sweep":
        az = ((t * 90.0) % 360.0) - 180.0
        el = 60.0 * math.sin(t * 2.0 * math.pi * 0.2)
        d = 0.5 + 0.5 * math.sin(t * 2.0 * math.pi * 0.13)
        return clamp(az, -180.0, 180.0), clamp(el, -90.0, 90.0), clamp(d, 0.0, 1.0)

    # sine
    az = 120.0 * math.sin(t * 2.0 * math.pi * 0.31)
    el = 45.0 * math.cos(t * 2.0 * math.pi * 0.17)
    d = 0.65 + 0.35 * math.sin(t * 2.0 * math.pi * 0.11)
    return clamp(az, -180.0, 180.0), clamp(el, -90.0, 90.0), clamp(d, 0.0, 1.0)


def main() -> int:
    parser = argparse.ArgumentParser(description="Spam ADM OSC xyz/aed to stress-test a receiver.")
    parser.add_argument("--host", default="127.0.0.1", help="Target host (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=4001, help="Target UDP port (default: 4001)")
    parser.add_argument("--object-id", type=int, default=1, help="ADM object id (default: 1)")
    parser.add_argument("--format", choices=("xyz", "aed"), default="xyz", help="OSC address format")
    parser.add_argument(
        "--mode",
        choices=("sine", "random", "sweep"),
        default="sine",
        help="Signal generation mode (default: sine)",
    )
    parser.add_argument("--rate", type=float, default=200.0, help="Packets per second (default: 200)")
    parser.add_argument(
        "--duration",
        type=float,
        default=0.0,
        help="Duration in seconds, 0 = infinite until Ctrl+C",
    )
    parser.add_argument(
        "--jitter",
        type=float,
        default=0.0,
        help="Random timing jitter in ms (+/-), default 0",
    )
    args = parser.parse_args()

    if args.port < 1 or args.port > 65535:
        raise SystemExit("Invalid --port (1..65535)")
    if args.object_id < 1 or args.object_id > 128:
        raise SystemExit("Invalid --object-id (1..128)")
    if args.rate <= 0:
        raise SystemExit("--rate must be > 0")

    address = f"/adm/obj/{args.object_id}/{args.format}"
    period = 1.0 / args.rate
    jitter_sec = max(0.0, args.jitter) / 1000.0
    start = time.perf_counter()
    next_deadline = start
    sent = 0

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    print(
        f"Sending {address} to {args.host}:{args.port} at {args.rate:.2f} Hz "
        f"(mode={args.mode}, duration={'infinite' if args.duration <= 0 else args.duration})"
    )
    try:
        while True:
            now = time.perf_counter()
            t = now - start
            if args.duration > 0 and t >= args.duration:
                break

            if args.format == "xyz":
                payload = make_xyz(t, args.mode)
            else:
                payload = make_aed(t, args.mode)

            packet = osc_pack(address, payload)
            sock.sendto(packet, (args.host, args.port))
            sent += 1

            next_deadline += period
            if jitter_sec > 0.0:
                next_deadline += random.uniform(-jitter_sec, jitter_sec)

            sleep_for = next_deadline - time.perf_counter()
            if sleep_for > 0:
                time.sleep(sleep_for)
            else:
                # If late, resync gently to avoid runaway drift.
                next_deadline = time.perf_counter()
    except KeyboardInterrupt:
        pass
    finally:
        elapsed = max(1e-9, time.perf_counter() - start)
        print(f"Stopped. Sent={sent} packets, avg_rate={sent / elapsed:.2f} Hz, elapsed={elapsed:.2f}s")
        sock.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
