"""Compile the actual quota helpers and imbalance logic without the game server.

Run: python3 tests/test_hub_population.py (requires g++).
"""
from pathlib import Path
import subprocess
import tempfile

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
static bool g_HubDispersePopulationLimitsEnabled = true;
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
        g_HubDispersePopulationLimitsEnabled = true;
        g_HubDisperseMinBotsPerHub = 0;
        g_HubDisperseMaxBotsPerHub = 50;
        // Mode 1 reads 7 bots; mode 2 reads 7% of 1000, capped to 50.
        assert(GetHubQuota(0, 1000) == (mode == 1 ? 7u : 50u));
        g_HubDisperseMinBotsPerHub = 10;
        assert(GetHubQuota(0, 100) == 10);
        g_HubCityPcts[0] = 20;
        assert(GetHubQuota(0, 100) == 20); // Max is not the goal.
        assert(GetHubMinimum(20) == 20);
        assert(classify(20, 17) == D::Underpopulated);
        assert(classify(20, 18) == D::Neutral);
        assert(classify(20, 22) == D::Neutral);
        assert(classify(20, 23) == D::Overpopulated);
        g_HubCityPcts[0] = 70;
        assert(GetHubQuota(0, 100) == 50);
        g_HubDispersePopulationLimitsEnabled = false;
        assert(GetHubQuota(0, 100) == 70);
        g_HubCityPcts[0] = 5;
        assert(GetHubQuota(0, 100) == 5); // Floor ignored.
        g_HubDisperseMaxBotsPerHub = 0;
        g_HubDisperseMinBotsPerHub = 0;
        assert(GetHubQuota(0, 100) == 5); // Ceiling ignored.
        g_HubDispersePopulationLimitsEnabled = true;
        assert(GetHubQuota(0, 100) == 0);
        assert(classify(0, 1) == D::Overpopulated);
        g_HubDisperseMaxBotsPerHub = 50;
        g_HubDisperseMinBotsPerHub = 30;
        g_HubCityPcts[0] = 0;
        assert(GetHubQuota(0, 100) == 0); // Disabled city beats floor.
        g_HubCityPcts[0] = 7;
        assert(GetHubQuota(0, 10) == 10); // Faction availability.
        assert(GetHubQuota(0, 0) == 0);
        assert(GetHubQuota(8, 100) == 0);
    }
    g_HubDisperseQuotaMode = 1;
    g_HubDispersePopulationLimitsEnabled = false;
    g_HubCityPcts[0] = 250;
    assert(GetHubQuota(0, 1000) == 250);
    assert(GetHubQuota(0, 100) == 100);
    g_HubDisperseQuotaMode = 2;
    g_HubCityPcts[0] = 7;
    assert(GetHubQuota(0, 50) == 4); // Standard rounding.
    assert(GetHubQuota(0, 7) == 0);
    assert(GetHubQuota(0, 8) == 1);

}
'''
with tempfile.TemporaryDirectory() as folder:
    cpp = Path(folder) / 'hub_population.cpp'
    binary = Path(folder) / 'hub_population'
    cpp.write_text(harness)
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('Hub population behavior checks passed.')

