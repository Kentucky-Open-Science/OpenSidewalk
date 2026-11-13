#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build pinned Zephyr sources; stage the app to support checkouts with spaces."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

project = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--workspace',type=Path,default=project.parent)
p.add_argument('--zephyr-base',type=Path)
p.add_argument('--build-dir',type=Path)
p.add_argument('--sdk',type=Path)
a = p.parse_args()
workspace = a.workspace.resolve()
zephyr = (a.zephyr_base or workspace/'zephyr').resolve()
build = (a.build_dir or workspace/'build').resolve()
commit = subprocess.check_output(['git','rev-parse','HEAD'],cwd=zephyr,text=True).strip()
if commit != '41d10b59730406bbab4ca9f1fbbe9e96aafb4966':
    p.error('Zephyr revision differs from west.yml; run west update')
for patch in sorted((project/'patches').glob('*.patch')):
    applied = subprocess.run(['git','apply','--reverse','--check',str(patch)],cwd=zephyr,capture_output=True).returncode==0
    if not applied:
        subprocess.run(['git','apply','--check',str(patch)],cwd=zephyr,check=True)
        subprocess.run(['git','apply',str(patch)],cwd=zephyr,check=True)
stage = build.parent/'opensidewalk-app-stage'
stage.mkdir(parents=True,exist_ok=True)
for name in ('CMakeLists.txt','prj.conf','app.overlay','src'):
    source,dest=project/name,stage/name
    if source.is_dir():
        shutil.copytree(source,dest,dirs_exist_ok=True)
    else:
        shutil.copy2(source,dest)
env = dict(os.environ)
env['ZEPHYR_BASE']=str(zephyr)
if a.sdk:
    env['ZEPHYR_SDK_INSTALL_DIR']=str(a.sdk.resolve())
    env['ZEPHYR_TOOLCHAIN_VARIANT']='zephyr'
subprocess.run(['west','build','-p','auto','-b','kg100s_rb4332a','-d',str(build),str(stage),
                '--','-DPython3_EXECUTABLE='+sys.executable],cwd=workspace,env=env,check=True)
output=project/'firmware'
output.mkdir(exist_ok=True)
for ext in ('hex','bin'):
    shutil.copy2(build/'zephyr'/('zephyr.'+ext),output/('opensidewalk-us915.'+ext))
print('Firmware:',output)
