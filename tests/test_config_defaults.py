"""Keep literal configuration fallbacks aligned with the shipped template."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
source = '\n'.join(p.read_text(encoding='utf-8') for p in (root / 'src').glob('*.cpp'))
config = (root / 'conf/mod_twow_bot_level_brackets.conf.dist').read_text(encoding='utf-8')
values = dict(re.findall(r'^(BotLevelBrackets\.[\w.]+)\s*=\s*(.*?)\s*$', config, re.M))
checks = 0
for key, default in re.findall(r'(?:ReadBoundedUInt|GetIntDefault|GetFloatDefault|GetBoolDefault|GetStringDefault)\("([A-Za-z0-9_.]+)",\s*("[^"]*"|true|false|-?[\d.]+f?)', source):
    # Only check keys that start with BotLevelBrackets.
    if not key.startswith("BotLevelBrackets."):
        continue
    assert key in values, f'{key}: read in code but absent from template'
    expected = default.strip('"')
    if expected.endswith('f') and any(c.isdigit() for c in expected):
        expected = expected[:-1]
    if expected in ('true', 'false'):
        expected = '1' if expected == 'true' else '0'
    actual = values[key].strip('"')
    try:
        equal = float(actual) == float(expected)
    except ValueError:
        equal = actual == expected
    assert equal, f'{key}: code={default}, expected={expected}, actual={actual}'
    checks += 1
assert checks >= 40, f'default parser unexpectedly skipped most settings (only checked {checks})'
print(f'{checks} literal config fallbacks match the shipped template.')

# Ensure obsolete/removed settings are not advertised as active in documentation or source
readme = (root / 'README.md').read_text(encoding='utf-8')
audit_fixes = (root / 'docs/AUDIT_FIXES.md').read_text(encoding='utf-8') if (root / 'docs/AUDIT_FIXES.md').exists() else ""
assert 'BotLevelBrackets.HubDisperse.PopulationLimitsEnabled =' not in readme, 'README.md still assigns obsolete PopulationLimitsEnabled'
if audit_fixes:
    assert 'BotLevelBrackets.HubDisperse.PopulationLimitsEnabled =' not in audit_fixes, 'AUDIT_FIXES.md still assigns obsolete PopulationLimitsEnabled'
assert 'PopulationLimitsEnabled' not in source, 'src/ still references obsolete PopulationLimitsEnabled'
print('Documentation consistency verified: no active assignments of obsolete PopulationLimitsEnabled.')
