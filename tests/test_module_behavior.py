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


def run_module_behavior_checks(sanitize=False):
    compiler = shutil.which('g++') or shutil.which('clang++')
    if not compiler:
        return False

    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory() as temp:
        include = Path(temp) / 'include'
        sources = [
            root / 'src/ActivityRegistry.cpp',
            root / 'src/DestinationCatalog.cpp',
            root / 'src/OpportunityLedger.cpp',
            root / 'src/PopulationCensus.cpp',
            root / 'src/SocialProtection.cpp',
            root / 'src/CityRelocationFallback.cpp',
            root / 'src/BracketsConfig.cpp',
            root / 'src/BracketsDebug.cpp',
            root / 'src/OpportunityCoordinator.cpp',
            root / 'src/mod-twow-bot-level-brackets.cpp',
            root / 'src/BotEngineAdapter.h',
        ]
        headers = set()
        for source in sources:
            if source.exists():
                headers.update(re.findall(r'^#include "([^"]+)"', source.read_text(encoding='utf-8'), re.M))

        for name in headers:
            # Module's internal headers are located directly in src/
            if (root / 'src' / name).exists():
                continue
            target = include / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text('#include "Framework.h"\n')

        for engine in ('generic', 'native', 'legacy'):
            binary = Path(temp) / engine
            define = {
                'generic': ['-DBEHAVIOR_TEST_HARNESS=1'],
                'native': ['-DHAVE_TORTOISEBOTS=1', '-DBEHAVIOR_TEST_HARNESS=1'],
                'legacy': ['-DHAVE_LEGACY_PLAYERBOTS=1', '-DBEHAVIOR_TEST_HARNESS=1'],
            }[engine]
            cmd = [
                compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-pthread',
                *define,
                *(['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if sanitize else []),
                '-I', str(include),
                '-I', str(root / 'src'),
                '-I', str(root / 'tests/support'),
                str(root / 'tests/test_module_behavior.cpp'),
                '-o', str(binary)
            ]
            subprocess.run(cmd, check=True)
            env = os.environ.copy()
            # LeakSanitizer requires /proc tracing unavailable in some managed sandboxes.
            env.setdefault("ASAN_OPTIONS", "detect_leaks=0")
            subprocess.run([str(binary)], check=True, env=env)
            print(f'{engine}: actual module behavior checks passed')

    return True


class TestModuleBehavior(unittest.TestCase):
    def test_module_behavior(self):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if not compiler:
            self.skipTest('Native C++ compiler (g++/clang++) not on PATH; skipped locally (verified in Linux CI workflow).')
        self.assertTrue(run_module_behavior_checks(sanitize=False))


if __name__ == '__main__':
    sanitize_flag = '--sanitize' in sys.argv
    compiler_bin = shutil.which('g++') or shutil.which('clang++')
    if not compiler_bin:
        print('Native C++ compiler (g++/clang++) not on PATH; skipped locally (verified in Linux CI workflow).')
        sys.exit(0)
    run_module_behavior_checks(sanitize=sanitize_flag)
