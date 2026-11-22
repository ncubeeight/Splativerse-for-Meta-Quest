#!/usr/bin/env python3
"""Check upload-related settings in the compiled APK, not just the source XML."""
import argparse
import os
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('apk', type=Path)
parser.add_argument('--allow-debug', action='store_true')
args = parser.parse_args()
sdk = Path(os.environ.get('ANDROID_HOME', str(Path.home()/'Library/Android/sdk')))
output = subprocess.check_output([str(sdk/'build-tools/36.0.0/aapt2'), 'dump',
    'xmltree', str(args.apk), '--file', 'AndroidManifest.xml'], text=True)
stack, nodes = [], []
for line in output.splitlines():
    depth = len(line)-len(line.lstrip())
    element = re.match(r'\s*E: (\S+)', line)
    if element:
        while stack and stack[-1][0] >= depth:
            stack.pop()
        node = {'tag': element[1], 'attributes': {}, 'children': []}
        if stack:
            stack[-1][1]['children'].append(node)
        stack.append((depth, node));nodes.append(node)
    attribute = re.match(r'\s*A: (?:http://schemas.android.com/apk/res/android:)?(\w+)(?:\([^)]*\))?=(.*)', line)
    if attribute and stack:
        value = attribute[2].split(' (Raw:')[0]
        stack[-1][1]['attributes'][attribute[1]] = value.strip('"')

def require(condition, message):
    if not condition:
        raise SystemExit('FAIL: '+message)

manifest = next(n['attributes'] for n in nodes if n['tag']=='manifest')
app = next(n['attributes'] for n in nodes if n['tag']=='application')
sdk_info = next(n['attributes'] for n in nodes if n['tag']=='uses-sdk')
require(manifest.get('installLocation')=='0', 'installLocation must be auto')
require(args.allow_debug or app.get('debuggable', 'false')=='false', 'APK must not be debuggable')
require(int(sdk_info['targetSdkVersion'], 0)==34, 'Quest preview must target API 34')
require(any(n['tag']=='uses-feature' and n['attributes'].get('name')=='android.hardware.vulkan.version'
    and int(n['attributes'].get('version', '0'), 0)==0x00401000
    and n['attributes'].get('required')=='true' for n in nodes), 'Vulkan 1.1 requirement missing')
main = []
for n in nodes:
    if n['tag']!='activity':continue
    if any(c['tag']=='action' and c['attributes'].get('name')=='android.intent.action.MAIN'
           for f in n['children'] if f['tag']=='intent-filter' for c in f['children']):
        a=n['attributes'];main.append(a['name'])
        require(a.get('launchMode') in {'2','3','4'}, a['name']+': unsupported MAIN launch mode')
        require(a.get('screenOrientation')=='0', a['name']+': MAIN activity must be landscape')
        require(a.get('excludeFromRecents')=='true', a['name']+': excludeFromRecents missing')
require(bool(main), 'No MAIN activity')
library=next(n for n in nodes if n['tag']=='activity' and n['attributes'].get('name')=='.LibraryActivity')
viewer=next(n for n in nodes if n['tag']=='activity' and n['attributes'].get('name')=='.ViewerActivity')
require(viewer['attributes'].get('taskAffinity')=='com.splativerse.quest.viewer', 'Viewer must have its own task affinity')
require(viewer['attributes'].get('resizeableActivity')=='false', 'Immersive viewer must not be resizeable')
require(any(c['tag']=='category' and c['attributes'].get('name')=='com.oculus.intent.category.2D'
    for f in library['children'] if f['tag']=='intent-filter' for c in f['children']), 'Library must be a 2D panel')
print('PASS: hybrid panel category and separate, non-resizeable immersive task')
print('PASS: compiled APK manifest; install auto; API 34; Vulkan 1.1; '+
      ('debug explicitly allowed' if args.allow_debug else 'non-debuggable')+
      '; all MAIN activities single-task/landscape/excluded from recents: '+', '.join(main))
