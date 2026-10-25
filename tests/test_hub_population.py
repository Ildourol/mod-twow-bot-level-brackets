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
        # Reset globals for headcount limits (LimitsMode = 1)
        limits_mode = 1
        min_limit_enabled = False
        max_limit_enabled = True
        min_bots = 0
        max_bots = 50
        city_pcts = [7, 5, 4, 4, 7, 5, 4, 4]

        # Mode 1 reads 7 bots; mode 2 reads 7% of 1000 (=70), capped to 50
        assert py_get_hub_quota(0, 1000, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == (7 if mode == 1 else 50)

        # Enable min limit: 10 bots
        min_limit_enabled = True
        min_bots = 10
        assert py_get_hub_quota(0, 100, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 10

        # Active city target and imbalance classification
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

        # Disable max limit while min limit (10) remains active
        max_limit_enabled = False
        assert py_get_hub_quota(0, 100, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 70
        city_pcts[0] = 5
        assert py_get_hub_quota(0, 100, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 10

        # Both limits disabled
        min_limit_enabled = False
        city_pcts[0] = 5
        assert py_get_hub_quota(0, 100, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 5

        # Both limits disabled with city 7 and available faction bots 10:
        # Mode 1 raw target is 7 -> returns 7.
        # Mode 2 raw target is (10 * 7 + 50) // 100 = 1 -> returns 1.
        city_pcts[0] = 7
        assert py_get_hub_quota(0, 10, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == (7 if mode == 1 else 1)

        # Min limit enabled with floor above total availability (availability cap test)
        min_limit_enabled = True
        max_limit_enabled = True
        min_bots = 30
        max_bots = 50
        city_pcts[0] = 7
        assert py_get_hub_quota(0, 10, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 10

        # Max 0 clamp
        min_limit_enabled = False
        max_limit_enabled = True
        max_bots = 0
        city_pcts[0] = 70
        assert py_get_hub_quota(0, 100, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 0

        # Contradictory bounds (Min > Max: Min 60, Max 50 -> clamped to 50)
        min_limit_enabled = True
        max_limit_enabled = True
        min_bots = 60
        max_bots = 50
        city_pcts[0] = 20
        assert py_get_hub_quota(0, 100, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 50

        # Disabled city (setting 0) is never elevated to minimum
        city_pcts[0] = 0
        min_bots = 10
        assert py_get_hub_quota(0, 100, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 0
        assert py_classify(0, 1) == D.Overpopulated

        # Edge cases: 0 faction total and out-of-range hub
        assert py_get_hub_quota(0, 0, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 0
        assert py_get_hub_quota(8, 100, mode, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 0

    # -----------------------------------------------------------------------
    # Comprehensive LimitsMode = 2 (Percentage Limits) Test Matrix
    # -----------------------------------------------------------------------
    # Test matrix across diverse populations: 0, 1, 10, 37, 100, 1000, 3000
    city_pcts = [40, 5, 4, 4, 7, 5, 4, 4]
    limits_mode = 2
    min_limit_enabled = True
    max_limit_enabled = True
    min_bots = 10  # 10%
    max_bots = 25  # 25%

    # Mode 2 (Percentage Quota) with Percentage Limits (LimitsMode 2)
    # Faction 37: city 40% (raw 15) capped by max 25% (9) -> 9
    assert py_get_hub_quota(0, 37, 2, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 9
    # Faction 37: city 5% (raw 2) elevated by min 10% (4) -> 4
    assert py_get_hub_quota(1, 37, 2, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 4
    # Faction 100: city 40% (raw 40) capped by max 25% (25) -> 25
    assert py_get_hub_quota(0, 100, 2, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 25
    # Faction 100: city 5% (raw 5) elevated by min 10% (10) -> 10
    assert py_get_hub_quota(1, 100, 2, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 10
    # Faction 1000: city 40% (raw 400) capped by max 25% (250) -> 250
    assert py_get_hub_quota(0, 1000, 2, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 250
    # Faction 3000: city 40% (raw 1200) capped by max 25% (750) -> 750
    assert py_get_hub_quota(0, 3000, 2, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 750
    # Faction 10: city 40% (raw 4) capped by max 25% (3) -> 3
    assert py_get_hub_quota(0, 10, 2, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 3
    # Faction 1: city 40% (raw 0), min 10% (0), max 25% (0) -> 0
    assert py_get_hub_quota(0, 1, 2, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 0
    # Faction 0 -> 0
    assert py_get_hub_quota(0, 0, 2, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 0

    # Mode 1 (Headcount Quota) with Percentage Limits (LimitsMode 2)
    # Faction 1000: city 300 bots, min 10% (100), max 25% (250) -> capped to 250
    city_pcts[0] = 300
    assert py_get_hub_quota(0, 1000, 1, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 250
    # Faction 1000: city 50 bots, min 10% (100), max 25% (250) -> elevated to 100
    city_pcts[0] = 50
    assert py_get_hub_quota(0, 1000, 1, min_limit_enabled, max_limit_enabled, limits_mode, min_bots, max_bots, city_pcts) == 100

    # Percentage limits toggle permutations
    city_pcts[0] = 40
    # Both off: raw 15 for faction 37
    assert py_get_hub_quota(0, 37, 2, False, False, limits_mode, min_bots, max_bots, city_pcts) == 15
    # Min only on: raw 2 for city 5% elevated to 4
    city_pcts[1] = 5
    assert py_get_hub_quota(1, 37, 2, True, False, limits_mode, min_bots, max_bots, city_pcts) == 4
    # Max only on: raw 15 for city 40% capped to 9
    assert py_get_hub_quota(0, 37, 2, False, True, limits_mode, min_bots, max_bots, city_pcts) == 9

    # -----------------------------------------------------------------------
    # Headcount high values and fraction rounding checks
    # -----------------------------------------------------------------------
    city_pcts = [250, 5, 4, 4, 7, 5, 4, 4]
    assert py_get_hub_quota(0, 1000, 1, False, False, 1, 0, 50, city_pcts) == 250
    assert py_get_hub_quota(0, 100, 1, False, False, 1, 0, 50, city_pcts) == 100

    city_pcts[0] = 7
    assert py_get_hub_quota(0, 50, 2, False, False, 1, 0, 50, city_pcts) == 4
    assert py_get_hub_quota(0, 7, 2, False, False, 1, 0, 50, city_pcts) == 0
    assert py_get_hub_quota(0, 8, 2, False, False, 1, 0, 50, city_pcts) == 1
    assert py_get_hub_quota(0, 10, 2, False, False, 1, 0, 50, city_pcts) == 1

# ---------------------------------------------------------------------------
# 2. Native C++ Harness Compilation (if compiler available)
# ---------------------------------------------------------------------------

def run_native_cpp_suite():
    compiler = shutil.which('g++') or shutil.which('clang++')
    if not compiler:
        return False

    source = (Path(__file__).resolve().parents[1] / 'src/mod-twow-bot-level-brackets.cpp').read_text(encoding='utf-8')
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
        // Reset globals for headcount limits (LimitsMode = 1)
        g_HubDisperseQuotaMode = mode;
        g_HubDisperseMinBotsLimitEnabled = false;
        g_HubDisperseMaxBotsLimitEnabled = true;
        g_HubDisperseLimitsMode = 1;
        g_HubDisperseMinBotsPerHub = 0;
        g_HubDisperseMaxBotsPerHub = 50;
        g_HubCityPcts[0] = 7;
        assert(GetHubQuota(0, 1000) == (mode == 1 ? 7u : 50u));

        // Enable min limit: 10 bots
        g_HubDisperseMinBotsLimitEnabled = true;
        g_HubDisperseMinBotsPerHub = 10;
        assert(GetHubQuota(0, 100) == 10);

        // Active city target and imbalance classification
        g_HubCityPcts[0] = 20;
        assert(GetHubQuota(0, 100) == 20);
        assert(GetHubMinimum(20) == 20);
        assert(classify(20, 17) == D::Underpopulated);
        assert(classify(20, 18) == D::Neutral);
        assert(classify(20, 22) == D::Neutral);
        assert(classify(20, 23) == D::Overpopulated);

        // Max limit clamps to 50
        g_HubCityPcts[0] = 70;
        assert(GetHubQuota(0, 100) == 50);

        // Disable max limit while min limit (10) remains active
        g_HubDisperseMaxBotsLimitEnabled = false;
        assert(GetHubQuota(0, 100) == 70);
        g_HubCityPcts[0] = 5;
        assert(GetHubQuota(0, 100) == 10); // Min limit 10 still active

        // Both limits disabled
        g_HubDisperseMinBotsLimitEnabled = false;
        assert(GetHubQuota(0, 100) == 5);

        // Both limits disabled with city 7 and available faction bots 10:
        // Mode 1 raw target is 7 -> returns 7.
        // Mode 2 raw target is (10 * 7 + 50) / 100 = 1 -> returns 1.
        g_HubCityPcts[0] = 7;
        assert(GetHubQuota(0, 10) == (mode == 1 ? 7u : 1u));

        // Min limit enabled with floor above total availability (availability cap test)
        g_HubDisperseMinBotsLimitEnabled = true;
        g_HubDisperseMaxBotsLimitEnabled = true;
        g_HubDisperseMinBotsPerHub = 30;
        g_HubDisperseMaxBotsPerHub = 50;
        g_HubCityPcts[0] = 7;
        assert(GetHubQuota(0, 10) == 10);

        // Max 0 clamp
        g_HubDisperseMinBotsLimitEnabled = false;
        g_HubDisperseMaxBotsLimitEnabled = true;
        g_HubDisperseMaxBotsPerHub = 0;
        g_HubCityPcts[0] = 70;
        assert(GetHubQuota(0, 100) == 0);

        // Contradictory bounds (Min > Max: Min 60, Max 50 -> clamped to 50)
        g_HubDisperseMinBotsLimitEnabled = true;
        g_HubDisperseMaxBotsLimitEnabled = true;
        g_HubDisperseMinBotsPerHub = 60;
        g_HubDisperseMaxBotsPerHub = 50;
        g_HubCityPcts[0] = 20;
        assert(GetHubQuota(0, 100) == 50);

        // Disabled city (setting 0) is never elevated to minimum
        g_HubCityPcts[0] = 0;
        g_HubDisperseMinBotsPerHub = 10;
        assert(GetHubQuota(0, 100) == 0);
        assert(classify(0, 1) == D::Overpopulated);

        // Edge cases: 0 faction total and out-of-range hub
        assert(GetHubQuota(0, 0) == 0);
        assert(GetHubQuota(8, 100) == 0);
    }

    // -----------------------------------------------------------------------
    // Comprehensive LimitsMode = 2 (Percentage Limits) Test Matrix
    // -----------------------------------------------------------------------
    g_HubCityPcts[0] = 40;
    g_HubCityPcts[1] = 5;
    g_HubDisperseLimitsMode = 2;
    g_HubDisperseMinBotsLimitEnabled = true;
    g_HubDisperseMaxBotsLimitEnabled = true;
    g_HubDisperseMinBotsPerHub = 10; // 10%
    g_HubDisperseMaxBotsPerHub = 25; // 25%

    // Mode 2 (Percentage Quota) with Percentage Limits (LimitsMode 2)
    g_HubDisperseQuotaMode = 2;
    // Faction 37: city 40% (raw 15) capped by max 25% (9) -> 9
    assert(GetHubQuota(0, 37) == 9);
    // Faction 37: city 5% (raw 2) elevated by min 10% (4) -> 4
    assert(GetHubQuota(1, 37) == 4);
    // Faction 100: city 40% (raw 40) capped by max 25% (25) -> 25
    assert(GetHubQuota(0, 100) == 25);
    // Faction 100: city 5% (raw 5) elevated by min 10% (10) -> 10
    assert(GetHubQuota(1, 100) == 10);
    // Faction 1000: city 40% (raw 400) capped by max 25% (250) -> 250
    assert(GetHubQuota(0, 1000) == 250);
    // Faction 3000: city 40% (raw 1200) capped by max 25% (750) -> 750
    assert(GetHubQuota(0, 3000) == 750);
    // Faction 10: city 40% (raw 4) capped by max 25% (3) -> 3
    assert(GetHubQuota(0, 10) == 3);
    // Faction 1: city 40% (raw 0), min 10% (0), max 25% (0) -> 0
    assert(GetHubQuota(0, 1) == 0);
    // Faction 0 -> 0
    assert(GetHubQuota(0, 0) == 0);

    // Mode 1 (Headcount Quota) with Percentage Limits (LimitsMode 2)
    g_HubDisperseQuotaMode = 1;
    // Faction 1000: city 300 bots, min 10% (100), max 25% (250) -> capped to 250
    g_HubCityPcts[0] = 300;
    assert(GetHubQuota(0, 1000) == 250);
    // Faction 1000: city 50 bots, min 10% (100), max 25% (250) -> elevated to 100
    g_HubCityPcts[0] = 50;
    assert(GetHubQuota(0, 1000) == 100);

    // Percentage limits toggle permutations
    g_HubDisperseQuotaMode = 2;
    g_HubCityPcts[0] = 40;
    // Both off: raw 15 for faction 37
    g_HubDisperseMinBotsLimitEnabled = false;
    g_HubDisperseMaxBotsLimitEnabled = false;
    assert(GetHubQuota(0, 37) == 15);
    // Min only on: raw 2 for city 5% elevated to 4
    g_HubDisperseMinBotsLimitEnabled = true;
    g_HubDisperseMaxBotsLimitEnabled = false;
    assert(GetHubQuota(1, 37) == 4);
    // Max only on: raw 15 for city 40% capped to 9
    g_HubDisperseMinBotsLimitEnabled = false;
    g_HubDisperseMaxBotsLimitEnabled = true;
    assert(GetHubQuota(0, 37) == 9);

    // -----------------------------------------------------------------------
    // Headcount high values and fraction rounding checks
    // -----------------------------------------------------------------------
    g_HubDisperseLimitsMode = 1;
    g_HubDisperseMinBotsLimitEnabled = false;
    g_HubDisperseMaxBotsLimitEnabled = false;
    g_HubDisperseQuotaMode = 1;
    g_HubCityPcts[0] = 250;
    assert(GetHubQuota(0, 1000) == 250);
    assert(GetHubQuota(0, 100) == 100);

    g_HubDisperseQuotaMode = 2;
    g_HubCityPcts[0] = 7;
    assert(GetHubQuota(0, 50) == 4);
    assert(GetHubQuota(0, 7) == 0);
    assert(GetHubQuota(0, 8) == 1);
    assert(GetHubQuota(0, 10) == 1);

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
