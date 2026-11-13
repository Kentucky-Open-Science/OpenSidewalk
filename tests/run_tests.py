#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Run host checks for KG100S join scheduling, shell interleavings, and the wire codec."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='kg100s-join-policy-') as directory:
    exe = Path(directory)/'join-policy'
    subprocess.run(['clang','-std=c17','-Wall','-Wextra','-Werror','-Isrc',
                    'tests/test_join_policy.c','src/join_policy.c','-o',str(exe)],cwd=root,check=True)
    subprocess.run([str(exe)],check=True)
with tempfile.TemporaryDirectory(prefix='opensidewalk-rf-') as directory:
    exe = Path(directory)/'rf-switch'
    subprocess.run(['clang','-std=c17','-Wall','-Wextra','-Werror','-Isrc',
                    'tests/test_rf_switch.c','-o',str(exe)],cwd=root,check=True)
    subprocess.run([str(exe)],check=True)
subprocess.run([sys.executable,str(root/'tests/test_control_races.py')],check=True)
subprocess.run(['node',str(root/'tests/test_codec.cjs')],check=True)
subprocess.run(['node',str(root/'tests/test_ble_codec.cjs')],check=True)
