#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Send USB console commands without echoing OTAA secrets into logs."""
import argparse
import json
import re
import time
from pathlib import Path
import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--port", required=True, help="Your board serial port")
parser.add_argument("--provision", type=Path, help="Read DevEUI, JoinEUI, AppKey from a local JSON file")
parser.add_argument("--command", action="append", default=[])
parser.add_argument("--wait", type=float, default=3)
parser.add_argument("--log", type=Path)
args = parser.parse_args()
commands = []
secrets = []
if args.provision:
    credentials = json.loads(args.provision.read_text())
    values = [credentials[k] for k in ("dev_eui", "join_eui", "app_key")]
    for value, size in zip(values, (16, 16, 32)):
        if not re.fullmatch(r"[0-9a-fA-F]{%d}" % size, value):
            parser.error("Invalid hexadecimal credentials")
    secrets.append(values[2])
    commands.append("lw subband " + str(int(credentials.get("subband", 2))))
    commands.append("lw provision " + " ".join(values))
commands.extend(args.command or ["lw status"])

port = serial.Serial()
port.port = args.port
port.baudrate = 115200
port.timeout = 0.1
port.dtr = False
port.rts = False
port.open()
output = bytearray()

def collect(seconds):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        output.extend(port.read(max(1, port.in_waiting)))

try:
    port.write(b"\r")
    collect(0.3)
    # Disable shell echo before sending any AppKey, including after a user enabled it.
    port.write(b"shell echo off\r")
    collect(0.3)
    for command in commands:
        port.write((command + "\r").encode())
        port.flush()
        collect(0.4)
    collect(args.wait)
finally:
    port.close()
text = output.decode(errors="replace")
text = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", text)
for secret in secrets:
    text = re.sub(re.escape(secret), "[APPKEY REDACTED]", text, flags=re.IGNORECASE)
if args.log:
    args.log.parent.mkdir(parents=True, exist_ok=True)
    args.log.write_text(text)
print(text)
