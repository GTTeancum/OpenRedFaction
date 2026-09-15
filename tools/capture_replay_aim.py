"""Bake process-local AIM_INPUT telemetry into an ordinary RFI6 replay.

The output requires no aim controller and is suitable for the Xbox replay
harness. Only look inputs are changed; every movement/button word is retained.
"""
import argparse
import math
from pathlib import Path
import struct


def capture(source, log):
    data = bytearray(source)
    if data[:8] != b'RFI6' + struct.pack('<I', 48) or (len(data) - 8) % 48:
        raise ValueError('RFI6 input required')
    frames = (len(data) - 8) // 48
    seen = set()
    for line in log.splitlines():
        if not line.startswith('AIM_INPUT '):
            continue
        _, frame, uid, pitch, yaw = line.split()
        frame, uid = int(frame), int(uid)
        pitch, yaw = float(pitch), float(yaw)
        if frame in seen or not 0 <= frame < frames or uid <= 0 or not all(
                math.isfinite(v) and -1 <= v <= 1 for v in (pitch, yaw)):
            raise ValueError('Invalid or duplicate aim input')
        seen.add(frame)
        struct.pack_into('<2f', data, 8 + frame * 48 + 12, pitch, yaw)
    if not seen:
        raise ValueError('No recorded aim inputs')
    return data, len(seen)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('log', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    if args.output.resolve() in (args.source.resolve(), args.log.resolve()):
        parser.error('Output must preserve the source and log')
    data, count = capture(args.source.read_bytes(), args.log.read_text())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    print(f'Captured {count} look inputs; {len(data)} bytes')
