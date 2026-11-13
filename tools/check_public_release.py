#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check public-release boundaries, obvious secrets, PNG metadata and image hashes."""
from pathlib import Path
import hashlib
import json
import re
import struct
import sys

root=Path(__file__).resolve().parents[1]
errors=[]
for rel in ('LICENSE','NOTICE','THIRD_PARTY_NOTICES.md','licenses/Silicon-Labs-Zlib.txt',
            'licenses/LoRaMac-node-BSD.txt','firmware/manifest.json','docs/device-redacted.png'):
    if not (root/rel).is_file():errors.append('Missing '+rel)
for p in root.rglob('*'):
    if not p.is_file() or any(part in ('.git','.venv','__pycache__') for part in p.relative_to(root).parts):continue
    rel=p.relative_to(root)
    if any(part in ('backups','artifacts') for part in rel.parts):errors.append('Private directory: '+str(rel))
    if p.suffix in ('.elf','.map','.log','.zip') or (p.suffix=='.bin' and str(rel)!='firmware/opensidewalk-us915.bin'):
        errors.append('Unexpected artifact: '+str(rel))
    if p.name.startswith('credentials') and p.name!='credentials.example.json':errors.append('Credential file: '+str(rel))
    b=p.read_bytes()
    if re.search(rb'/Users/[A-Za-z0-9_.-]+/|/home/[A-Za-z0-9_.-]+/',b):errors.append('Personal filesystem path: '+str(rel))
    if re.search(rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----',b):errors.append('Private key: '+str(rel))
    if re.search(rb'"app_key"\s*:\s*"[0-9a-fA-F]{32}"',b):errors.append('Provisioned AppKey: '+str(rel))
    if p.suffix=='.png':
        pos=8
        if b[:8]!=b'\x89PNG\r\n\x1a\n':errors.append('Invalid PNG: '+str(rel));continue
        while pos<len(b):
            n=struct.unpack('>I',b[pos:pos+4])[0];kind=b[pos+4:pos+8]
            if kind not in (b'IHDR',b'PLTE',b'IDAT',b'IEND',b'tRNS'):errors.append('PNG metadata chunk: '+kind.decode(errors='replace'))
            pos+=n+12
image=root/'firmware/opensidewalk-us915.hex'
if image.is_file():
    base=0;eof=False;addresses=[]
    for line in image.read_text().splitlines():
        record=bytes.fromhex(line[1:]);assert line.startswith(':') and sum(record)%256==0
        length=record[0];offset=int.from_bytes(record[1:3],'big');kind=record[3];data=record[4:4+length]
        if kind==4:base=int.from_bytes(data,'big')<<16
        elif kind==2:base=int.from_bytes(data,'big')<<4
        elif kind==0:
            addresses.extend(range(base+offset,base+offset+length))
        elif kind==1:eof=True
    if not eof or not addresses or min(addresses)!=0 or max(addresses)>=0xf8000:errors.append('HEX includes unexpected memory ranges')
manifest=root/'firmware/manifest.json'
if manifest.is_file():
    for name,expected in json.loads(manifest.read_text())['sha256'].items():
        p=root/'firmware'/name
        if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=expected:errors.append('Firmware hash mismatch: '+name)
if errors:
    print('\n'.join(errors),file=sys.stderr);raise SystemExit(1)
print('PASS: public release files, credentials boundaries, metadata, image ranges and hashes.')
