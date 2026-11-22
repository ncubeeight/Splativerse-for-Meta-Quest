#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess
import sys
import re
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'.build';BUILD.mkdir(exist_ok=True)
subprocess.run([os.environ.get('CXX','clang++'),'-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(ROOT/'native'),'-I'+str(ROOT/'vendor/spz'),str(ROOT/'native/core.cpp'),str(ROOT/'vendor/spz/load-spz.cc'),str(ROOT/'vendor/spz/splat-types.cc'),str(ROOT/'tests/core_tests.cpp'),str(ROOT/'tests/visibility_tests.cpp'),str(ROOT/'tests/viewpoint_tests.cpp'),'-lz','-o',str(BUILD/'core-tests')],check=True)
subprocess.run([str(BUILD/'core-tests'),str(ROOT),*sys.argv[1:]],check=True,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1','ASAN_OPTIONS':'halt_on_error=1'})
# GLSL float literals are float32; preserve that in the host compilation.
helper=(ROOT/'shaders/filter.glsl').read_text().replace('inout float','float&')
helper=re.sub(r'(?<![\w.])(\d+\.\d+)(?![\w.])',r'\1f',helper)
(BUILD/'filter-port.h').write_text(helper)
subprocess.run([os.environ.get('CXX','clang++'),'-std=c++17','-O2','-I'+str(BUILD),str(ROOT/'tests/filter_tests.cpp'),'-o',str(BUILD/'filter-tests')],check=True)
subprocess.run([str(BUILD/'filter-tests')],check=True)
subprocess.run([os.environ.get('CXX','clang++'),'-std=c++17','-g','-fsanitize=address,undefined',
    '-I'+str(ROOT/'native'),'-I'+str(ROOT/'vendor/spz'),str(ROOT/'tests/menu_tests.cpp'),
    '-o',str(BUILD/'menu-tests')],check=True)
subprocess.run([str(BUILD/'menu-tests')],check=True,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1','ASAN_OPTIONS':'halt_on_error=1'})
