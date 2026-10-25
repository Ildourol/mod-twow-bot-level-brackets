"""Verify quota helpers and population imbalance logic.

Runs pure Python behavioral checks and optionally compiles the native C++ harness if a compiler is available.
Run: python tests/test_hub_population.py
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

# ---------------------------------------------------------------------------
# 1. Pure Python Reference Verification (Cross-Platform)
# ---------------------------------------------------------------------------

class D:
    Neutral = 0
    Underpopulated = 1
    Overpopulated = 2

def py_get_hub_quota(hub_idx, faction_total, quota_mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts):
    if hub_idx >= len(city_pcts) or faction_total == 0:
        return 0
    configured_value = city_pcts[hub_idx]
    if configured_value <= 0:
        return 0
    value = configured_value
    quota = value if quota_mode == 1 else ((faction_total * value + 50) // 100)
    min_bound = min_bots if limits_mode == 1 else ((faction_total * min_bots + 50) // 100)
    max_bound = max_bots if limits_mode == 1 else ((faction_total * max_bots + 50) // 100)
    if max_limit_enabled:
        quota = min(quota, max_bound)
    if min_limit_enabled:
        effective_min = min(min_bound, max_bound) if max_limit_enabled else min_bound
        quota = max(quota, effective_min)
    return min(quota, faction_total)

def py_get_hub_minimum(quota):
    return quota

def py_classify(quota, effective_count, hysteresis=2):
    minimum = py_get_hub_minimum(quota)
    if quota == 0:
        return D.Overpopulated if effective_count > 0 else D.Neutral
    else:
        if effective_count > quota + hysteresis:
            return D.Overpopulated
        elif effective_count + hysteresis < minimum:
            return D.Underpopulated
        else:
            return D.Neutral

def run_python_suite():
    for mode in [1, 2]:
        min_limit_enabled = False
        max_limit_enabled = True
        limits_mode = 1  # 1 = Headcount, 2 = Percentage
        min_bots = 0
        max_bots = 50
        city_pcts = [7, 5, 4, 4, 7, 5, 4, 4]

        # Mode 1 reads 7 bots; mode 2 reads 7% of 1000, capped to 50.
        assert py_get_hub_quota(0, 1000, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == (7 if mode == 1 else 50)

        # Enable min limit: 10 bots
        min_limit_enabled = True
        min_bots = 10
        assert py_get_hub_quota(0, 100, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 10

        city_pcts[0] = 20
        assert py_get_hub_quota(0, 100, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 20
        assert py_get_hub_minimum(20) == 20
        assert py_classify(20, 17) == D.Underpopulated
        assert py_classify(20, 18) == D.Neutral
        assert py_classify(20, 22) == D.Neutral
        assert py_classify(20, 23) == D.Overpopulated

        # Max limit clamps to 50
        city_pcts[0] = 70
        assert py_get_hub_quota(0, 100, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 50

        # Disable max limit
        max_limit_enabled = False
        assert py_get_hub_quota(0, 100, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 70

        # Disable both limits
        min_limit_enabled = False
        city_pcts[0] = 5
        assert py_get_hub_quota(0, 100, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 5

        # Test LimitsMode 2 (Percentage limits)
        limits_mode = 2
        min_limit_enabled = True
        max_limit_enabled = True
        min_bots = 10   # 10%
        max_bots = 25   # 25%
        city_pcts[0] = 5   # Raw target: 5 bots for 100 bots
        # 5% raw gets raised to 10% minimum (10 bots for 100 bots)
        assert py_get_hub_quota(0, 100, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == (10 if mode == 2 else 10)
        city_pcts[0] = 40  # 40% raw gets capped to 25% maximum (25 bots for 100 bots)
        assert py_get_hub_quota(0, 100, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == (25 if mode == 2 else 25)

        # Reset limits_mode to 1
        limits_mode = 1
        max_bots = 50
        min_bots = 30
        city_pcts[0] = 0
        assert py_get_hub_quota(0, 100, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 0

        city_pcts[0] = 7
        assert py_get_hub_quota(0, 10, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 10
        assert py_get_hub_quota(0, 0, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 0
        assert py_get_hub_quota(8, 100, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 0

    city_pcts = [250, 5, 4, 4, 7, 5, 4, 4]
    assert py_get_hub_quota(0, 1000, 1, False, False, 1, 0, 50, city_pcts) == 250
    assert py_get_hub_quota(0, 100, 1, False, False, 1, 0, 50, city_pcts) == 100

    city_pcts[0] = 7
    assert py_get_hub_quota(0, 50, 2, False, False, 1, 0, 50, city_pcts) == 4
    assert py_get_hub_quota(0, 7, 2, False, False, 1, 0, 50, city_pcts) == 0
    assert py_get_hub_quota(0, 8, 2, False, False, 1, 0, 50, city_pcts) == 1

# ---------------------------------------------------------------------------
# 2. Native C++ Harness Compilation (if compiler available)
# ---------------------------------------------------------------------------

def run_native_cpp_suite():
    compiler = shutil.which('g++') or shutil.which('clang++')
    if not compiler:
        return False

    source = (Path(__file__).resolve().parents[1] / 'src/mod-twow-bot-level-brackets.cpp').read_text()
    helpers = source[source.index('static uint32 GetHubQuota('):source.index('static bool ShouldExcludeBot(')]
    start = source.index('        HubImbalanceDirection dir =', source.index('struct HubEval'))
    end = source.index('        HubStabilityRecord& stab', start)
    classification = source[start:end]
    harness = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstddef>
using uint32 = uint32_t;
using uint64 = uint64_t;
using int32 = int32_t;
static uint32 g_HubDisperseQuotaMode = 1;
static bool g_HubDisperseMinBotsLimitEnabled = false;
static bool g_HubDisperseMaxBotsLimitEnabled = true;
static uint32 g_HubDisperseLimitsMode = 1;
static uint32 g_HubDisperseMinBotsPerHub = 0;
static uint32 g_HubDisperseMaxBotsPerHub = 50;
static uint32 g_HubDisperseHysteresisBots = 2;
static const size_t s_NumHubAreas = 8;
static int32 g_HubCityPcts[8] = {7,5,4,4,7,5,4,4};
enum class HubImbalanceDirection { Neutral, Underpopulated, Overpopulated };
''' + helpers + '''
static HubImbalanceDirection classify(uint32 quota, int32 effectiveCount)
{
    uint32 minimum = GetHubMinimum(quota);
''' + classification + '''
    return dir;
}
int main()
{
    using D = HubImbalanceDirection;
    for (uint32 mode : {1u, 2u}) {
        g_HubDisperseQuotaMode = mode;
        g_HubDisperseMinBotsLimitEnabled = false;
        g_HubDisperseMaxBotsLimitEnabled = true;
        g_HubDisperseLimitsMode = 1;
        g_HubDisperseMinBotsPerHub = 0;
        g_HubDisperseMaxBotsPerHub = 50;
        assert(GetHubQuota(0, 1000) == (mode == 1 ? 7u : 50u));
        g_HubDisperseMinBotsLimitEnabled = true;
        g_HubDisperseMinBotsPerHub = 10;
        assert(GetHubQuota(0, 100) == 10);
        g_HubCityPcts[0] = 20;
        assert(GetHubQuota(0, 100) == 20);
        assert(GetHubMinimum(20) == 20);
        assert(classify(20, 17) == D::Underpopulated);
        assert(classify(20, 18) == D::Neutral);
        assert(classify(20, 22) == D::Neutral);
        assert(classify(20, 23) == D::Overpopulated);
        g_HubCityPcts[0] = 70;
        assert(GetHubQuota(0, 100) == 50);
        g_HubDisperseMaxBotsLimitEnabled = false;
        assert(GetHubQuota(0, 100) == 70);
        g_HubCityPcts[0] = 5;
        assert(GetHubQuota(0, 100) == 10); // Min limit 10 still active
        g_HubDisperseMinBotsLimitEnabled = false;
        assert(GetHubQuota(0, 100) == 5);
        g_HubCityPcts[0] = 0;
        assert(GetHubQuota(0, 100) == 0);
        g_HubCityPcts[0] = 7;
        assert(GetHubQuota(0, 10) == 10);
        assert(GetHubQuota(0, 0) == 0);
        assert(GetHubQuota(8, 100) == 0);
    }
    return 0;
}
'''
    with tempfile.TemporaryDirectory() as folder:
        cpp = Path(folder) / 'hub_population.cpp'
        binary = Path(folder) / 'hub_population'
        cpp.write_text(harness)
        subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror', str(cpp), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    return True

if __name__ == '__main__':
    run_python_suite()
    if run_native_cpp_suite():
        print('Hub population behavior checks passed (Python and Native C++ suites).')
    else:
        print('Hub population behavior checks passed (Python reference suite; native C++ skipped - no g++/clang++ on PATH).')
