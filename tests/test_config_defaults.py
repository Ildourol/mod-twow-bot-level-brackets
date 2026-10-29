"""Keep literal configuration fallbacks aligned with the shipped template."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
source = (root / 'src/mod-twow-bot-level-brackets.cpp').read_text(encoding='utf-8')
config = (root / 'conf/mod_twow_bot_level_brackets.conf.dist').read_text(encoding='utf-8')
values = dict(re.findall(r'^(BotLevelBrackets\.[\w.]+)\s*=\s*(.*?)\s*$', config, re.M))
checks = 0
for key, default in re.findall(r'(?:ReadBoundedUInt|GetIntDefault|GetFloatDefault|GetBoolDefault|GetStringDefault)\("(BotLevelBrackets\.[\w.]+)",\s*("[^"]*"|true|false|-?[\d.]+f?)', source):
    assert key in values, f'{key}: read in code but absent from template'
    if default.startswith('"') and default.endswith('"'):
        expected = default[1:-1]
    else:
        expected = default.removesuffix('f')
    if expected in ('true', 'false'):
        expected = '1' if expected == 'true' else '0'
    actual = values[key].strip('"')
    try:
        equal = float(actual) == float(expected)
    except ValueError:
        equal = actual == expected
    assert equal, f'{key}: code={default}, template={values[key]}'
    checks += 1
assert checks >= 40, 'default parser unexpectedly skipped most settings'
print(f'{checks} literal config fallbacks match the shipped template.')

# Ensure obsolete/removed settings are not advertised as active in documentation or source
readme = (root / 'README.md').read_text(encoding='utf-8')
audit_fixes = (root / 'docs/AUDIT_FIXES.md').read_text(encoding='utf-8')
assert 'BotLevelBrackets.HubDisperse.PopulationLimitsEnabled =' not in readme, 'README.md still assigns obsolete PopulationLimitsEnabled'
assert 'BotLevelBrackets.HubDisperse.PopulationLimitsEnabled =' not in audit_fixes, 'AUDIT_FIXES.md still assigns obsolete PopulationLimitsEnabled'
assert 'PopulationLimitsEnabled' not in source, 'src/mod-twow-bot-level-brackets.cpp still references obsolete PopulationLimitsEnabled'
print('Documentation consistency verified: no active assignments of obsolete PopulationLimitsEnabled.')
