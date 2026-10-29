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

class TestModuleBehavior(unittest.TestCase):
    def test_module_behavior(self):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if not compiler:
            self.skipTest('Native C++ compiler (g++/clang++) not on PATH; skipped locally (verified in Linux CI workflow).')

        root = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as temp:
            include = Path(temp) / 'include'
            sources = [root / 'src/mod-twow-bot-level-brackets.cpp', root / 'src/BotEngineAdapter.h']
            headers = set()
            for source in sources:
                headers.update(re.findall(r'^#include "([^"]+)"', source.read_text(encoding='utf-8'), re.M))
            for name in headers:
                if name in ('BotEngineAdapter.h', 'mod-twow-bot-level-brackets.h'):
                    continue
                target = include / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text('#include "Framework.h"\n')
            for engine in ('generic', 'native', 'legacy'):
                binary = Path(temp) / engine
                define = {'generic': [], 'native': ['-DHAVE_TORTOISEBOTS=1'], 'legacy': ['-DHAVE_LEGACY_PLAYERBOTS=1']}[engine]
                subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-pthread',
                                *define, *(['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if '--sanitize' in sys.argv else []), '-I', str(include), '-I', str(root / 'tests/support'),
                                str(root / 'tests/test_module_behavior.cpp'), '-o', str(binary)], check=True)
                env = os.environ.copy()
                env.setdefault("ASAN_OPTIONS", "detect_leaks=0")
                subprocess.run([str(binary)], check=True, env=env)
                print(f'{engine}: actual module behavior checks passed')

if __name__ == '__main__':
    unittest.main()
