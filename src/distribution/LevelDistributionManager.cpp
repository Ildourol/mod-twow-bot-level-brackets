#include "LevelDistributionManager.h"
#include "../compat/NativeTaskAdapter.h"
#include "../policy/PopulationPolicy.h"
#include "../world/Census.h"
#include "../social/ProtectionCache.h"
#include "../scheduling/RequestLedger.h"

#ifdef HAVE_TORTOISEBOTS
#include "Common.h"
#include "Objects/Player.h"
#include "ObjectAccessor.h"
#include "Chat/Chat.h"
#include "Database/DatabaseEnv.h"
#include "Database/QueryResult.h"
#include "Log.h"
#include "Util.h"
#elif defined(HAVE_LEGACY_PLAYERBOTS)
#include "Common.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "Chat.h"
#include "Database/DatabaseEnv.h"
#include "Database/QueryResult.h"
#include "Log.h"
#include "Util.h"
#else
#include "Common.h"
#include "Objects/Player.h"
#include "ObjectAccessor.h"
#include "Chat/Chat.h"
#include "Database/DatabaseEnv.h"
#include "Database/QueryResult.h"
#include "Log.h"
#include "Util.h"
#endif

#include <ctime>
#include <algorithm>
#include <cmath>

namespace BotLevelBrackets
{

namespace
{

std::vector<int32_t> CalculateDesiredCounts(uint32_t totalBots, const std::vector<float>& desiredPcts)
{
    size_t numRanges = desiredPcts.size();
    std::vector<int32_t> desired(numRanges, 0);
    std::vector<double> remainder(numRanges, 0.0);
    std::vector<size_t> indices(numRanges);
    uint32_t assigned = 0;

    for (size_t i = 0; i < numRanges; ++i)
    {
        indices[i] = i;
        double raw = static_cast<double>(desiredPcts[i]) * totalBots / 100.0;
        desired[i] = static_cast<int32_t>(std::floor(raw));
        remainder[i] = raw - desired[i];
        assigned += static_cast<uint32_t>(desired[i]);
    }

    uint32_t remainderCount = totalBots > assigned ? (totalBots - assigned) : 0;
    std::sort(indices.begin(), indices.end(), [&remainder](size_t a, size_t b) {
        return remainder[a] > remainder[b];
    });

    for (size_t i = 0; i < remainderCount && i < numRanges; ++i)
    {
        desired[indices[i]]++;
    }

    return desired;
}

} // anonymous namespace

LevelDistributionManager& LevelDistributionManager::Instance()
{
    static LevelDistributionManager instance;
    return instance;
}

void LevelDistributionManager::Update(uint32_t diff)
{
    const auto& policy = PopulationPolicy::Instance();
    if (!policy.IsEnabled() || !policy.IsLevelDistributionEnabled())
        return;

    m_distributionTimer += diff;
    m_flaggedTimer += diff;
    m_starterZoneTimer += diff;
    m_wrongMapTimer += diff;

    // 1. Process flagged pending resets (default every 15s)
    uint32_t flaggedCadenceMs = policy.GetCheckFlaggedFrequency() * 1000;
    if (m_flaggedTimer >= flaggedCadenceMs)
    {
        m_flaggedTimer = 0;
        ProcessPendingResets();
    }

    // 2. Main distribution check cycle (warm-up run at 20s on startup, then regular check frequency)
    if (!m_initialPassDone)
    {
        if (m_distributionTimer >= 20000)
        {
            m_initialPassDone = true;
            m_distributionTimer = 0;
            RunDistributionPass();
        }
    }
    else
    {
        uint32_t distCadenceMs = policy.GetDistributionCheckFrequency() * 1000;
        if (m_distributionTimer >= distCadenceMs)
        {
            m_distributionTimer = 0;
            RunDistributionPass();
        }
    }

    // 3. Starting-zone dispersal scan (default every 120s)
    if (policy.IsStartingZoneDisperseEnabled())
    {
        uint32_t starterCadenceMs = policy.GetStartingZoneDisperseFrequency() * 1000;
        if (m_starterZoneTimer >= starterCadenceMs)
        {
            m_starterZoneTimer = 0;
            ProcessStartingZoneDisperse();
        }
    }

    // 4. Wrong-map dispersal scan (default every 180s)
    if (policy.IsWrongMapDisperseEnabled())
    {
        uint32_t wrongMapCadenceMs = policy.GetWrongMapDisperseFrequency() * 1000;
        if (m_wrongMapTimer >= wrongMapCadenceMs)
        {
            m_wrongMapTimer = 0;
            ProcessWrongMapDisperse();
        }
    }
}

void LevelDistributionManager::ForceDistributionCycle(ChatHandler* handler)
{
    RunDistributionPass(handler);
}

void LevelDistributionManager::ForceDispersalCycle(ChatHandler* handler)
{
    ProcessStartingZoneDisperse();
    ProcessWrongMapDisperse();
    if (handler)
        handler->SendSysMessage("[BotBrackets] Starting zone and wrong map dispersal passes executed.");
}

bool LevelDistributionManager::EnqueueReset(Player* bot, uint8_t targetRange, uint8_t teamId)
{
    if (!bot)
        return false;

    uint32_t guidLow = bot->GetObjectGuid().GetCounter();
    const auto& policy = PopulationPolicy::Instance();

    std::lock_guard<std::mutex> lock(m_queueMutex);

    uint32_t maxQueue = policy.GetMaxPendingQueueSize();
    if (maxQueue > 0 && m_pendingResets.size() >= maxQueue)
        return false;

    if (m_pendingResets.find(guidLow) != m_pendingResets.end())
        return false; // Already queued

    const auto& brackets = policy.GetLevelBrackets(teamId);
    if (targetRange >= brackets.size())
        return false;

    const auto& b = brackets[targetRange];
    uint8_t targetLevel = (b.lower == b.upper) ? b.lower : static_cast<uint8_t>(urand(b.lower, b.upper));

    PendingResetEntry entry{};
    entry.guidLow = guidLow;
    entry.targetRange = targetRange;
    entry.targetLevel = targetLevel;
    entry.teamId = teamId;
    entry.enqueuedAt = static_cast<uint32_t>(time(nullptr));

    m_pendingResets[guidLow] = entry;
    return true;
}

bool LevelDistributionManager::BoostBotDirect(Player* bot, uint8_t targetLevel)
{
    if (!bot || !bot->IsInWorld())
        return false;

    uint32_t guidLow = bot->GetObjectGuid().GetCounter();

    // Remove from pending queue if present
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_pendingResets.erase(guidLow);
    }

    // Cleanly cancel city visit if active
    NativeTaskAdapter::CancelCityVisit(bot->GetObjectGuid());
    const LedgerEntry* active = RequestLedger::Instance().GetActiveEntryForBot(guidLow);
    if (active)
    {
        RequestLedger::Instance().MarkReleased(active->requestId, "AdminBoost", static_cast<uint32_t>(time(nullptr)));
    }

    if (!NativeTaskAdapter::AdjustBotLevel(bot, targetLevel))
        return false;

    m_statsBotsAdjusted.fetch_add(1, std::memory_order_relaxed);

    if (PopulationPolicy::Instance().IsTeleportOnLevelChange())
    {
        NativeTaskAdapter::TeleportBotToLevelZone(bot, targetLevel, bot->GetTeamId());
        m_statsBotsTeleported.fetch_add(1, std::memory_order_relaxed);
    }

    return true;
}

size_t LevelDistributionManager::GetPendingQueueSize() const
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return m_pendingResets.size();
}

std::vector<PendingResetEntry> LevelDistributionManager::GetPendingQueueSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    std::vector<PendingResetEntry> snapshot;
    snapshot.reserve(m_pendingResets.size());
    for (const auto& kv : m_pendingResets)
        snapshot.push_back(kv.second);
    return snapshot;
}

void LevelDistributionManager::RemoveBot(uint32_t guidLow)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_pendingResets.erase(guidLow);
}

void LevelDistributionManager::ClearPendingQueue()
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_pendingResets.clear();
}

void LevelDistributionManager::RunDistributionPass(ChatHandler* handler)
{
    bool expected = false;
    if (!m_distributionInProgress.compare_exchange_strong(expected, true))
    {
        if (handler)
            handler->SendSysMessage("[BotBrackets] Level distribution cycle already in progress, skipping.");
        return;
    }

    m_statsCyclesRun.fetch_add(1, std::memory_order_relaxed);
    const auto& policy = PopulationPolicy::Instance();
    CensusSnapshot snap = Census::Instance().TakeSnapshot();

    auto runFaction = [this, &policy, &snap](uint8_t teamId, uint32_t totalBots, const std::vector<BracketCensusItem>& brackets)
    {
        if (totalBots == 0 || brackets.empty())
            return;

        size_t numRanges = brackets.size();
        std::vector<float> desiredPcts(numRanges);
        for (size_t i = 0; i < numRanges; ++i)
            desiredPcts[i] = brackets[i].desiredPercent;

        // Dynamic Real-Player Distribution (matching b4622bd)
        if (policy.UseDynamicDistribution())
        {
            uint32_t totalHumans = (teamId == 0) ? snap.allianceTotalHumans : snap.hordeTotalHumans;
            if (policy.SyncFactions())
                totalHumans = snap.allianceTotalHumans + snap.hordeTotalHumans;

            if (totalHumans > 0)
            {
                const float baseline = 1.0f;
                float realPlayerWeight = policy.GetRealPlayerWeight();
                std::vector<float> weights(numRanges, 0.0f);
                float totalWeight = 0.0f;

                for (size_t i = 0; i < numRanges; ++i)
                {
                    uint32_t humanCount = brackets[i].humanCount;
                    if (policy.SyncFactions())
                    {
                        uint32_t allyH = (i < snap.allianceBrackets.size()) ? snap.allianceBrackets[i].humanCount : 0;
                        uint32_t hordH = (i < snap.hordeBrackets.size()) ? snap.hordeBrackets[i].humanCount : 0;
                        humanCount = allyH + hordH;
                    }

                    weights[i] = baseline + realPlayerWeight * (1.0f / static_cast<float>(totalHumans)) * std::log(1.0f + static_cast<float>(humanCount));
                    totalWeight += weights[i];
                }

                if (totalWeight > 0.0f)
                {
                    for (size_t i = 0; i < numRanges; ++i)
                    {
                        desiredPcts[i] = (weights[i] / totalWeight) * 100.0f;
                    }
                }
            }
        }

        std::vector<int32_t> desiredCounts = CalculateDesiredCounts(totalBots, desiredPcts);
        std::vector<std::vector<Player*>> botsByRange(numRanges);
        std::vector<int32_t> actualCounts(numRanges, 0);

        // Collect online bots by bracket & catch out-of-bracket bots (matching b4622bd GetOrFlagPlayerBracket)
        const auto& playerMap = sObjectAccessor.GetPlayers();
        for (const auto& kv : playerMap)
        {
            Player* p = kv.second;
            if (!p || !p->IsInWorld() || !p->IsAlive() || p->GetTeamId() != teamId)
                continue;

            if (!NativeTaskAdapter::IsFreeBot(p))
                continue;

            uint32_t guidLow = p->GetObjectGuid().GetCounter();

            // City Mover coexistence: do NOT interrupt bots visiting cities unless under-leveled (< City.MinLevel)
            if (NativeTaskAdapter::HasActiveCityVisit(p->GetObjectGuid()))
            {
                if (p->GetLevel() >= policy.GetCityMinLevel())
                    continue;
            }

            if (RequestLedger::Instance().HasActiveRequest(guidLow))
                continue;

            if (ProtectionCache::Instance().IsBotProtected(p))
                continue;

            uint8_t lvl = p->GetLevel();
            int matchedRange = -1;
            for (size_t r = 0; r < numRanges; ++r)
            {
                if (lvl >= brackets[r].lower && lvl <= brackets[r].upper)
                {
                    matchedRange = static_cast<int>(r);
                    botsByRange[r].push_back(p);
                    ++actualCounts[r];
                    break;
                }
            }

            // Out-of-bracket catch (from b4622bd GetOrFlagPlayerBracket):
            // If bot does not fall into any bracket, flag it immediately for the nearest bracket!
            if (matchedRange < 0)
            {
                int targetRange = -1;
                int smallestDiff = 1000;
                for (size_t r = 0; r < numRanges; ++r)
                {
                    if (brackets[r].lower > brackets[r].upper)
                        continue;
                    int diff = 0;
                    if (lvl < brackets[r].lower)
                        diff = brackets[r].lower - lvl;
                    else if (lvl > brackets[r].upper)
                        diff = lvl - brackets[r].upper;

                    if (diff < smallestDiff)
                    {
                        smallestDiff = diff;
                        targetRange = static_cast<int>(r);
                    }
                }

                if (targetRange >= 0)
                {
                    EnqueueReset(p, static_cast<uint8_t>(targetRange), teamId);
                }
            }
        }

        uint32_t threshold = policy.GetMinBracketChangeThreshold();

        // Identify surplus brackets and enqueue bots to deficit brackets
        for (size_t src = 0; src < numRanges; ++src)
        {
            int32_t surplus = actualCounts[src] - desiredCounts[src];
            if (threshold > 0 && surplus <= static_cast<int32_t>(threshold))
                continue;

            size_t candidateIdx = 0;
            while (actualCounts[src] > desiredCounts[src] && candidateIdx < botsByRange[src].size())
            {
                if (threshold > 0 && (actualCounts[src] - desiredCounts[src]) <= static_cast<int32_t>(threshold))
                    break;

                int32_t targetDeficit = -1;
                int32_t maxShortage = 0;
                for (size_t dst = 0; dst < numRanges; ++dst)
                {
                    int32_t shortage = desiredCounts[dst] - actualCounts[dst];
                    if (shortage > maxShortage)
                    {
                        maxShortage = shortage;
                        targetDeficit = static_cast<int32_t>(dst);
                    }
                }

                if (targetDeficit < 0)
                    break;

                Player* cand = botsByRange[src][candidateIdx++];
                if (EnqueueReset(cand, static_cast<uint8_t>(targetDeficit), teamId))
                {
                    --actualCounts[src];
                    ++actualCounts[targetDeficit];
                }
            }
        }
    };

    runFaction(0, snap.allianceTotalBots, snap.allianceBrackets);
    runFaction(1, snap.hordeTotalBots, snap.hordeBrackets);

    m_distributionInProgress.store(false);

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        sLog.outString("[BotLevelBrackets] Level distribution pass: A=%u bots, H=%u bots. Resets queued: %u.",
            snap.allianceTotalBots, snap.hordeTotalBots, static_cast<uint32_t>(m_pendingResets.size()));
    }

    if (handler)
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        handler->PSendSysMessage("[BotBrackets] Level distribution pass completed. Pending resets queued: %u.",
            static_cast<uint32_t>(m_pendingResets.size()));
    }
}

void LevelDistributionManager::ProcessPendingResets()
{
    const auto& policy = PopulationPolicy::Instance();
    uint32_t limit = policy.GetFlaggedProcessLimit();
    uint32_t ttl = policy.GetPendingQueueTTL();
    uint32_t now = static_cast<uint32_t>(time(nullptr));

    std::vector<PendingResetEntry> candidates;

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_pendingResets.empty())
            return;

        // Scale limit up to 25 bots per tick when queue is deep to prevent hour-long backlogs
        if (m_pendingResets.size() > 20 && limit > 0 && limit < 25)
            limit = 25;

        for (auto it = m_pendingResets.begin(); it != m_pendingResets.end();)
        {
            if (ttl > 0 && now >= it->second.enqueuedAt && (now - it->second.enqueuedAt) > ttl)
            {
                it = m_pendingResets.erase(it);
                continue;
            }

            Player* bot = sObjectAccessor.FindPlayer(ObjectGuid(it->first));
            if (!bot || !bot->IsInWorld() || !bot->GetSession() || bot->GetSession()->isLogingOut())
            {
                it = m_pendingResets.erase(it);
                continue;
            }

            if (ProtectionCache::Instance().IsBotProtected(bot))
            {
                it = m_pendingResets.erase(it);
                continue;
            }

            // Check if bot is temporarily unsafe for level reset (combat, taxi flight, dead, trading, looting, duel, BG, dungeon/raid)
            if (!bot->IsAlive() || bot->IsInCombat() || bot->IsTaxiFlying() || bot->IsBeingTeleported() ||
                bot->GetTrader() || !bot->GetLootGuid().IsEmpty() || bot->m_duel != nullptr ||
                bot->InBattleGround() || bot->InBattleGroundQueue() ||
                (bot->GetMap() && (bot->GetMap()->IsDungeon() || bot->GetMap()->IsRaid() || bot->GetMap()->IsBattleGround())))
            {
                // Temporarily busy - leave in queue for next tick!
                m_statsBotsSkipped.fetch_add(1, std::memory_order_relaxed);
                ++it;
                continue;
            }

            if (limit > 0 && candidates.size() >= limit)
                break;

            candidates.push_back(it->second);
            ++it;
        }
    }

    std::vector<uint32_t> processed;
    for (const auto& entry : candidates)
    {
        Player* bot = sObjectAccessor.FindPlayer(ObjectGuid(entry.guidLow));
        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            continue;

        bool inCity = NativeTaskAdapter::HasActiveCityVisit(bot->GetObjectGuid());

        if (NativeTaskAdapter::AdjustBotLevel(bot, entry.targetLevel))
        {
            processed.push_back(entry.guidLow);
            m_statsBotsAdjusted.fetch_add(1, std::memory_order_relaxed);

            if (inCity)
            {
                // In city visit: adjust in place, do NOT teleport to open-world leveling zone! Keep in city and re-apply city arrival protocol
                NativeTaskAdapter::ApplyCityArrivalProtocol(bot, CityId::None);
            }
            else if (policy.IsTeleportOnLevelChange())
            {
                NativeTaskAdapter::TeleportBotToLevelZone(bot, entry.targetLevel, entry.teamId);
                m_statsBotsTeleported.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    // Remove successfully adjusted candidates from queue
    if (!processed.empty())
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        for (uint32_t guidLow : processed)
        {
            m_pendingResets.erase(guidLow);
        }
    }
}

void LevelDistributionManager::ProcessStartingZoneDisperse()
{
    const auto& playerMap = sObjectAccessor.GetPlayers();
    uint32_t count = 0;

    for (const auto& kv : playerMap)
    {
        if (count >= 50)
            break;

        Player* bot = kv.second;
        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            continue;

        if (!NativeTaskAdapter::IsFreeBot(bot))
            continue;

        if (bot->GetLevel() <= 9)
            continue;

        if (!NativeTaskAdapter::IsInStarterValley(bot))
            continue;

        if (!NativeTaskAdapter::IsBotSafeForDispersal(bot))
            continue;

        if (ProtectionCache::Instance().IsBotProtected(bot))
            continue;

        if (NativeTaskAdapter::HasActiveCityVisit(bot->GetObjectGuid()))
            continue;

        // Check if bot is already queued for reset
        uint32_t guidLow = bot->GetObjectGuid().GetCounter();
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_pendingResets.find(guidLow) != m_pendingResets.end())
                continue;
        }

        if (NativeTaskAdapter::TeleportBotToLevelZone(bot, bot->GetLevel(), bot->GetTeamId()))
        {
            m_statsDispersedStarter.fetch_add(1, std::memory_order_relaxed);
            ++count;
        }
    }

    if (count > 0)
    {
        sLog.outDetail("[BotLevelBrackets] StartingZoneDisperse: evacuated %u over-leveled bots to leveling zones.", count);
    }
}

void LevelDistributionManager::ProcessWrongMapDisperse()
{
    const auto& playerMap = sObjectAccessor.GetPlayers();
    uint32_t count = 0;

    for (const auto& kv : playerMap)
    {
        if (count >= 50)
            break;

        Player* bot = kv.second;
        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            continue;

        if (!NativeTaskAdapter::IsFreeBot(bot))
            continue;

        if (!NativeTaskAdapter::IsOnWrongMap(bot))
            continue;

        if (!NativeTaskAdapter::IsBotSafeForDispersal(bot))
            continue;

        if (ProtectionCache::Instance().IsBotProtected(bot))
            continue;

        if (NativeTaskAdapter::TeleportBotToLevelZone(bot, bot->GetLevel(), bot->GetTeamId()))
        {
            m_statsDispersedWrongMap.fetch_add(1, std::memory_order_relaxed);
            ++count;
        }
    }

    if (count > 0)
    {
        sLog.outDetail("[BotLevelBrackets] WrongMapDisperse: evacuated %u stranded bots to open world.", count);
    }
}

} // namespace BotLevelBrackets
