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
using int32 = int32_t;
static uint32 g_HubDisperseQuotaMode = 1;
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
    assert(GetHubQuota(0, 1000) == 50);
    assert(GetHubMinimum(50) == 50); // Legacy headcount target.
    assert(classify(50, 47) == D::Underpopulated);
    assert(classify(50, 48) == D::Neutral);
    assert(classify(50, 52) == D::Neutral);
    assert(classify(50, 53) == D::Overpopulated);
    g_HubDisperseMinBotsPerHub = 30;
    assert(GetHubMinimum(50) == 30);
    for (int32 count = 0; count <= 100; ++count)
        assert(classify(50, count) == (count < 28 ? D::Underpopulated :
            count > 52 ? D::Overpopulated : D::Neutral));
    g_HubDisperseHysteresisBots = 0;
    assert(classify(50, 29) == D::Underpopulated);
    assert(classify(50, 30) == D::Neutral);
    assert(classify(50, 50) == D::Neutral);
    assert(classify(50, 51) == D::Overpopulated);
    g_HubDisperseQuotaMode = 2;
    assert(GetHubQuota(0, 1000) == 50); // Percentage cap.
    assert(GetHubQuota(0, 100) == 30); // Percentage floor.
    assert(GetHubQuota(0, 10) == 10); // Available faction population.
    assert(GetHubQuota(0, 0) == 0);
    assert(GetHubQuota(8, 100) == 0);
    g_HubCityPcts[0] = 0;
    assert(GetHubQuota(0, 100) == 0); // Disabled city beats floor.
    g_HubCityPcts[0] = 7;
    g_HubDisperseMinBotsPerHub = 0;
    assert(GetHubQuota(0, 100) == 7);
    assert(GetHubQuota(0, 50) == 4); // Rounding.
    assert(GetHubQuota(0, 7) == 0);
    assert(GetHubQuota(0, 8) == 1);
    assert(GetHubMinimum(7) == 7);
    g_HubDisperseMaxBotsPerHub = 0;
    for (uint32 mode : {1u, 2u}) {
        g_HubDisperseQuotaMode = mode;
        assert(GetHubQuota(0, 1000) == 0);
        assert(GetHubMinimum(0) == 0);
        assert(classify(0, 0) == D::Neutral);
        assert(classify(0, 1) == D::Overpopulated);
    }
}
'''
with tempfile.TemporaryDirectory() as folder:
    cpp = Path(folder) / 'hub_population.cpp'
    binary = Path(folder) / 'hub_population'
    cpp.write_text(harness)
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('Hub population behavior checks passed.')
