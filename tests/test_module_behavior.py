"""Compile and exercise the actual module/adapter with deterministic host doubles.

This checks behavior and all 3 conditional compilation paths; it is not a
substitute for a build against the actual core, engine and game database.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import sys
import os
import unittest


def run_behavior_checks(sanitize=False):
    compiler = shutil.which('g++') or shutil.which('clang++')
    if not compiler:
        return False

    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory() as temp:
        include = Path(temp) / 'include'
        include.mkdir(parents=True, exist_ok=True)

        # 1. Scan all module sources for included header names
        sources = list((root / 'src').rglob('*.h')) + list((root / 'src').rglob('*.cpp')) + [root / 'tests/test_module_behavior.cpp']
        headers = set()
        for source in sources:
            if source.exists():
                headers.update(re.findall(r'^\s*#\s*include\s*"([^"]+)"', source.read_text(encoding='utf-8'), re.M))

        # 2. Identify local module headers so we never overwrite real module headers
        local_headers = set()
        for f in (root / 'src').rglob('*.h'):
            local_headers.add(f.relative_to(root / 'src').as_posix())
            local_headers.add(f.name)

        # 3. Create mock stubs for core/engine headers
        for name in headers:
            if name.startswith('..') or name in local_headers or (root / 'src' / name).exists():
                continue
            target = include / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text('#include "Framework.h"\n', encoding='utf-8')

        # Ensure support/Framework.h stub exists for any include with support/ prefix
        (include / 'support').mkdir(parents=True, exist_ok=True)
        (include / 'support/Framework.h').write_text('#include "Framework.h"\n', encoding='utf-8')

        # 4. Compile and exercise all 3 engine compilation paths
        for engine in ('generic', 'native', 'legacy'):
            binary = Path(temp) / engine
            define = {
                'generic': [],
                'native': ['-DHAVE_TORTOISEBOTS=1'],
                'legacy': ['-DHAVE_LEGACY_PLAYERBOTS=1']
            }[engine]

            cmd = [
                compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-pthread',
                *define,
                *(['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if sanitize else []),
                '-I', str(include),
                '-I', str(root / 'tests/support'),
                '-I', str(root / 'tests'),
                '-I', str(root / 'src'),
                str(root / 'tests/test_module_behavior.cpp'),
                '-o', str(binary)
            ]
            subprocess.run(cmd, check=True)
            env = os.environ.copy()
            # LeakSanitizer requires /proc tracing unavailable in some managed CI sandboxes.
            env.setdefault("ASAN_OPTIONS", "detect_leaks=0")
            subprocess.run([str(binary)], check=True, env=env)
            print(f'{engine}: actual module behavior checks passed')
    return True


class TestModuleBehavior(unittest.TestCase):
    def test_module_behavior_engines(self):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if not compiler:
            self.skipTest('Native C++ compiler (g++/clang++) not on PATH; skipped locally (verified in Linux CI workflow).')
        self.assertTrue(run_behavior_checks(sanitize=False))


if __name__ == '__main__':
    compiler = shutil.which('g++') or shutil.which('clang++')
    if not compiler:
        print('Native C++ compiler (g++/clang++) not on PATH; skipped locally (verified in Linux CI workflow).')
        sys.exit(0)

    sanitize = '--sanitize' in sys.argv
    run_behavior_checks(sanitize=sanitize)
