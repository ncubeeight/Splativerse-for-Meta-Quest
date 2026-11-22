#!/usr/bin/env python3
"""Device helpers. Never select an arbitrary device when several are connected."""
import argparse
import os
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
SDK=Path(os.environ.get('ANDROID_HOME',str(Path.home()/'Library/Android/sdk')))
ADB=str(SDK/'platform-tools/adb')
parser=argparse.ArgumentParser()
parser.add_argument('action',choices=['install','launch','logs','push'])
parser.add_argument('file',nargs='?')
parser.add_argument('--serial')
args=parser.parse_args()
prefix=[ADB]+(['-s',args.serial] if args.serial else [])
if not args.serial:
    result=subprocess.check_output([ADB,'devices'],text=True)
    devices=[l.split()[0] for l in result.splitlines()[1:] if len(l.split())>=2 and l.split()[1]=='device']
    if len(devices)!=1:raise SystemExit('Connect and authorize one headset, or pass --serial.\n'+result)
    prefix=[ADB,'-s',devices[0]]
if args.action=='install':subprocess.run(prefix+['install','--no-incremental','-r',str(ROOT/'Splativerse-Quest-preview.apk')],check=True)
elif args.action=='launch':subprocess.run(prefix+['shell','am','start','-n','com.splativerse.quest/.LibraryActivity'],check=True)
elif args.action=='logs':subprocess.run(prefix+['logcat','-s','Splativerse','OpenXR-Loader','AndroidRuntime'],check=True)
else:
    if not args.file:raise SystemExit('push requires a scan file')
    source=Path(args.file).resolve()
    if source.suffix.lower() not in ['.ply','.spz','.splat']:raise SystemExit('Choose PLY, SPZ, or SPLAT')
    # adb push receives arguments directly; avoid shell interpolation of filenames.
    destination='/sdcard/Android/data/com.splativerse.quest/files/Scans/'
    subprocess.run(prefix+['shell','mkdir','-p',destination],check=True)
    subprocess.run(prefix+['push',str(source),destination+source.name],check=True)
