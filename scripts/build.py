#!/usr/bin/env python3
"""Build a signed, sideloadable arm64 APK using the Android SDK and NDK."""
import argparse
import hashlib
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import zipfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / '.build'
SDK = Path(os.environ.get('ANDROID_HOME', str(Path.home() / 'Library/Android/sdk')))
NDK = Path(os.environ.get('ANDROID_NDK_HOME', str(ROOT.parents[1] / 'work/toolchain/android-ndk-r27d')))
JAVA = Path(os.environ.get('JAVA_HOME', '/Applications/Android Studio.app/Contents/jbr/Contents/Home'))
HOST = 'darwin-x86_64' if platform.system() == 'Darwin' else 'linux-x86_64'
TOOLS = SDK / 'build-tools/36.0.0'
ANDROID_JAR = SDK / 'platforms/android-36/android.jar'
LLVM = NDK / 'toolchains/llvm/prebuilt' / HOST / 'bin'

def run(*args):
    subprocess.run([str(a) for a in args], check=True, cwd=ROOT,
                   env={**os.environ, 'JAVA_HOME':str(JAVA)})

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--debug',action='store_true',help='Build a debuggable APK instead of the default non-debuggable preview')
    args=parser.parse_args()
    for p in [LLVM/'clang++', TOOLS/'aapt2', JAVA/'bin/javac', ANDROID_JAR]:
        if not p.exists(): raise SystemExit(f'Missing {p}. Set ANDROID_HOME, ANDROID_NDK_HOME, and JAVA_HOME; install SDK platform/build-tools 36.')
    BUILD.mkdir(exist_ok=True)
    # Keep the source manifest non-debuggable. Debugging is an explicit opt-in.
    ET.register_namespace('android','http://schemas.android.com/apk/res/android')
    manifest=ET.parse(ROOT/'app/src/main/AndroidManifest.xml')
    manifest.find('application').set('{http://schemas.android.com/apk/res/android}debuggable','true' if args.debug else 'false')
    manifest_path=BUILD/'AndroidManifest.xml'
    manifest.write(manifest_path,encoding='utf-8',xml_declaration=True)
    assets=ROOT/'app/src/main/assets'
    print('Compiling Vulkan shaders', flush=True)
    for stem in ['splat','hud']:
        for stage in ['vert','frag']:
            run(NDK/'shader-tools'/HOST/'glslc',f'-fshader-stage={stage}','--target-env=vulkan1.1','-O',ROOT/f'shaders/{stem}.{stage}','-o',assets/f'{stem}.{stage}.spv')
    glue=NDK/'sources/android/native_app_glue'
    run(LLVM/'aarch64-linux-android29-clang','-O2','-fPIC','-c',glue/'android_native_app_glue.c','-o',BUILD/'glue.o')
    print('Compiling native renderer and OpenXR application',flush=True)
    sources=[ROOT/'native'/n for n in ['core.cpp','gpu.cpp','main.cpp']]+[ROOT/'vendor/spz'/n for n in ['load-spz.cc','splat-types.cc']]
    run(LLVM/'aarch64-linux-android29-clang++','-std=c++17','-O2','-g','-fPIC','-shared','-static-libstdc++',
        '-Wall','-Wextra','-Wno-missing-field-initializers','-Wno-sign-compare','-Wno-unused-parameter',
        '-I'+str(ROOT/'native'),'-I'+str(ROOT/'vendor/openxr/include'),'-I'+str(ROOT/'vendor/openxr'),'-I'+str(ROOT/'vendor/spz'),'-I'+str(glue),
        *sources,BUILD/'glue.o','-L'+str(ROOT/'vendor/openxr'),'-lopenxr_loader','-landroid','-llog','-lvulkan','-lz',
        '-Wl,-z,max-page-size=16384','-Wl,-soname,libsplativerse.so','-o',BUILD/'libsplativerse.so')
    run(LLVM/'llvm-strip','--strip-unneeded',BUILD/'libsplativerse.so','-o',BUILD/'libsplativerse-stripped.so')
    classes=BUILD/'classes';dex=BUILD/'dex'
    if classes.exists():shutil.rmtree(classes)
    if dex.exists():shutil.rmtree(dex)
    classes.mkdir();dex.mkdir()
    run(JAVA/'bin/javac','-source','8','-target','8','-Xlint:-options','-classpath',ANDROID_JAR,'-d',classes,*sorted((ROOT/'app/src/main/java').rglob('*.java')))
    run(TOOLS/'d8','--lib',ANDROID_JAR,'--min-api','29','--output',dex,*sorted(classes.rglob('*.class')))
    run(TOOLS/'aapt2','link','--manifest',manifest_path,'-I',ANDROID_JAR,'-A',assets,'-o',BUILD/'unsigned.apk')
    with zipfile.ZipFile(BUILD/'unsigned.apk','a',compression=zipfile.ZIP_DEFLATED) as apk:
        apk.write(dex/'classes.dex','classes.dex')
        apk.write(BUILD/'libsplativerse-stripped.so','lib/arm64-v8a/libsplativerse.so')
        apk.write(ROOT/'vendor/openxr/libopenxr_loader.so','lib/arm64-v8a/libopenxr_loader.so')
    key=BUILD/'debug.keystore'
    if not key.exists():
        run(JAVA/'bin/keytool','-genkeypair','-keystore',key,'-storepass','android','-keypass','android','-alias','androiddebugkey','-dname','CN=Splativerse Development,O=Splativerse,C=US','-keyalg','RSA','-keysize','2048','-validity','10000')
    run(TOOLS/'zipalign','-f','-p','4',BUILD/'unsigned.apk',BUILD/'aligned.apk')
    output=ROOT/('Splativerse-Quest-debug.apk' if args.debug else 'Splativerse-Quest-preview.apk')
    # Preserve the preview certificate so existing sideload installations update
    # without uninstalling scans. This is not a new production signing identity.
    run(TOOLS/'apksigner','sign','--ks',key,'--ks-pass','pass:android','--key-pass','pass:android','--v2-signing-enabled','true','--v3-signing-enabled','true','--out',output,BUILD/'aligned.apk')
    run(TOOLS/'apksigner','verify','--verbose',output)
    run(sys.executable,ROOT/'scripts/verify_apk.py',output,*(['--allow-debug'] if args.debug else []))
    digest=hashlib.sha256(output.read_bytes()).hexdigest()
    output.with_suffix('.apk.sha256').write_text(digest+'  '+output.name+'\n')
    print(f'Built {output} ({output.stat().st_size/1048576:.1f} MiB)')

if __name__=='__main__':main()
