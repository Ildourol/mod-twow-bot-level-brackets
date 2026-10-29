#include "BracketsConfig.h"
#include "ActivityRegistry.h"
#include "Config/Config.h"
#include "Log.h"
#include "Util.h"

#include <sstream>
#include <algorithm>
#include <cctype>

namespace Brackets
{
    static uint32 ReadBoundedUInt(const char* name, uint32 def, uint32 minVal, uint32 maxVal)
    {
        int32 val = sConfig.GetIntDefault(name, static_cast<int32>(def));
        if (val < static_cast<int32>(minVal))
            return minVal;
        if (static_cast<uint32>(val) > maxVal)
            return maxVal;
        return static_cast<uint32>(val);
    }

    static bool GetBoolDefault(const char* name, bool def)
    {
        return sConfig.GetBoolDefault(name, def);
    }

    static int32 GetIntDefault(const char* name, int32 def)
    {
        return sConfig.GetIntDefault(name, def);
    }

    static float GetFloatDefault(const char* name, float def)
    {
        return sConfig.GetFloatDefault(name, def);
    }

    static std::string GetStringDefault(const char* name, const char* def)
    {
        return sConfig.GetStringDefault(name, def);
    }

    ConfigManager::ConfigManager()
    {
        LoadConfiguration(false);
    }

    const ModuleConfiguration& ConfigManager::GetConfig() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_activeConfig;
    }

    uint32 ConfigManager::GetGeneration() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_activeConfig.generation;
    }

    bool ConfigManager::IsFallbackAllowedForCity(const std::string& cityName) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_activeConfig.fallbackEnabled)
            return false;
        return m_activeConfig.fallbackCityAllowlist.find(cityName) != m_activeConfig.fallbackCityAllowlist.end();
    }

    bool ConfigManager::ParseAndValidate(ModuleConfiguration& staged, std::string& errorMsg)
    {
        // 1. Master & Domains
        staged.enabled = GetBoolDefault("BotLevelBrackets.Enabled", true);
        staged.cityEnabled = GetBoolDefault("BotLevelBrackets.City.Enabled", true);
        staged.worldEnabled = GetBoolDefault("BotLevelBrackets.World.Enabled", true);
        staged.cityActivationMode = GetStringDefault("BotLevelBrackets.City.ActivationMode", "Augmenter");

        // 2. Opportunities shared budgets & cadence
        staged.maxAdmissionsPerCycle = ReadBoundedUInt("BotLevelBrackets.Opportunities.MaxAdmissionsPerCycle", 5, 1, 100);
        staged.maxOutstandingPerFaction = ReadBoundedUInt("BotLevelBrackets.Opportunities.MaxOutstandingPerFaction", 10, 1, 200);
        staged.checkSeconds = ReadBoundedUInt("BotLevelBrackets.Opportunities.CheckSeconds", 90, 5, 3600);
        staged.stabilitySeconds = ReadBoundedUInt("BotLevelBrackets.Opportunities.StabilitySeconds", 60, 5, 3600);
        staged.crossDomainCooldownSeconds = ReadBoundedUInt("BotLevelBrackets.Opportunities.CrossDomainCooldownSeconds", 1800, 0, 86400);
        staged.failureBackoffSeconds = ReadBoundedUInt("BotLevelBrackets.Opportunities.FailureBackoffSeconds", 600, 10, 86400);

        // 3. World outdoor distribution
        staged.fieldReservePct = ReadBoundedUInt("BotLevelBrackets.World.FieldReservePct", 70, 0, 100);
        staged.maxInboundPerRegion = ReadBoundedUInt("BotLevelBrackets.World.MaxInboundPerRegion", 3, 1, 50);
        staged.followHumanLocations = GetBoolDefault("BotLevelBrackets.World.FollowHumanLocations", false);
        staged.regionConfig = GetStringDefault("BotLevelBrackets.World.RegionConfig", "");

        // 4. Level policy
        std::string modeStr = GetStringDefault("BotLevelBrackets.LevelPolicy.Mode", "ObserveNative");
        if (modeStr == "ObserveNative")
            staged.levelPolicyMode = LEVEL_POLICY_OBSERVE_NATIVE;
        else
            staged.levelPolicyMode = LEVEL_POLICY_ADVISORY;

        staged.reportDesiredTargets = GetBoolDefault("BotLevelBrackets.LevelPolicy.ReportDesiredTargets", true);
        staged.minBracketChangeThreshold = ReadBoundedUInt("BotLevelBrackets.LevelPolicy.MinChangeThreshold", 1, 0, 50);
        staged.realPlayerWeight = GetFloatDefault("BotLevelBrackets.LevelPolicy.RealPlayerWeight", 1.0f);
        if (staged.realPlayerWeight < 0.0f)
            staged.realPlayerWeight = 0.0f;
        staged.syncFactions = GetBoolDefault("BotLevelBrackets.LevelPolicy.SyncFactions", false);

        // 5. Hub disperse quotas & limits
        staged.hubQuotaMode = ReadBoundedUInt("BotLevelBrackets.HubDisperse.QuotaMode", 2, 1, 2);
        staged.hubLimitsMode = ReadBoundedUInt("BotLevelBrackets.HubDisperse.LimitsMode", 1, 0, 2);
        staged.hubMinLimitEnabled = GetBoolDefault("BotLevelBrackets.HubDisperse.MinBotsLimitEnabled", false);
        staged.hubMaxLimitEnabled = GetBoolDefault("BotLevelBrackets.HubDisperse.MaxBotsLimitEnabled", true);
        staged.hubMinBotsPerHub = ReadBoundedUInt("BotLevelBrackets.HubDisperse.MinBotsPerHub", 0, 0, 500);
        staged.hubMaxBotsPerHub = ReadBoundedUInt("BotLevelBrackets.HubDisperse.MaxBotsPerHub", 50, 0, 500);
        staged.hubHysteresisBots = ReadBoundedUInt("BotLevelBrackets.HubDisperse.HysteresisBots", 2, 0, 20);

        staged.hubCityPcts[CITY_STORMWIND]   = ReadBoundedUInt("BotLevelBrackets.HubDisperse.StormwindPct", 7, 0, 100);
        staged.hubCityPcts[CITY_IRONFORGE]   = ReadBoundedUInt("BotLevelBrackets.HubDisperse.IronforgePct", 5, 0, 100);
        staged.hubCityPcts[CITY_DARNASSUS]   = ReadBoundedUInt("BotLevelBrackets.HubDisperse.DarnassusPct", 4, 0, 100);
        staged.hubCityPcts[CITY_ALAH_THALAS] = ReadBoundedUInt("BotLevelBrackets.HubDisperse.AlahThalasPct", 4, 0, 100);
        staged.hubCityPcts[CITY_ORGRIMMAR]   = ReadBoundedUInt("BotLevelBrackets.HubDisperse.OrgrimmarPct", 7, 0, 100);
        staged.hubCityPcts[CITY_UNDERCITY]   = ReadBoundedUInt("BotLevelBrackets.HubDisperse.UndercityPct", 5, 0, 100);
        staged.hubCityPcts[CITY_THUNDER_BLUFF] = ReadBoundedUInt("BotLevelBrackets.HubDisperse.ThunderBluffPct", 4, 0, 100);
        staged.hubCityPcts[CITY_SPARKWATER]  = ReadBoundedUInt("BotLevelBrackets.HubDisperse.SparkwaterPct", 4, 0, 100);

        // 6. City Fallback (Option 3 restricted compatibility path)
        staged.fallbackEnabled = GetBoolDefault("BotLevelBrackets.City.Fallback.Enabled", false);
        staged.fallbackCities = GetStringDefault("BotLevelBrackets.City.Fallback.Cities", "");
        staged.fallbackCityAllowlist.clear();
        if (!staged.fallbackCities.empty())
        {
            std::stringstream ss(staged.fallbackCities);
            std::string item;
            while (std::getline(ss, item, ','))
            {
                size_t start = item.find_first_not_of(" \t");
                size_t end = item.find_last_not_of(" \t");
                if (start != std::string::npos && end != std::string::npos)
                    staged.fallbackCityAllowlist.insert(item.substr(start, end - start + 1));
            }
        }
        staged.fallbackAllowedReason = GetStringDefault("BotLevelBrackets.City.Fallback.AllowedReason", "NativeRouteUnsupportedBeforeStart");
        staged.fallbackMaxPerCycle = ReadBoundedUInt("BotLevelBrackets.City.Fallback.MaxPerCycle", 1, 1, 10);
        staged.fallbackMaxPerHour = ReadBoundedUInt("BotLevelBrackets.City.Fallback.MaxPerHour", 2, 1, 50);
        staged.fallbackMaxSharePct = ReadBoundedUInt("BotLevelBrackets.City.Fallback.MaxSharePct", 10, 1, 100);
        staged.fallbackCooldownSeconds = ReadBoundedUInt("BotLevelBrackets.City.Fallback.CooldownSeconds", 7200, 60, 86400);
        staged.fallbackEarlyDepartureSeconds = ReadBoundedUInt("BotLevelBrackets.City.Fallback.EarlyDepartureSeconds", 120, 10, 3600);
        staged.fallbackEarlyDepartureBackoffSeconds = ReadBoundedUInt("BotLevelBrackets.City.Fallback.EarlyDepartureBackoffSeconds", 21600, 60, 172800);

        // 7. Social protection & exclusions
        staged.excludeNamesStr = GetStringDefault("BotLevelBrackets.ExcludeNames", "");
        staged.excludeNames.clear();
        if (!staged.excludeNamesStr.empty())
        {
            std::stringstream ss(staged.excludeNamesStr);
            std::string name;
            while (std::getline(ss, name, ','))
            {
                size_t start = name.find_first_not_of(" \t");
                size_t end = name.find_last_not_of(" \t");
                if (start != std::string::npos && end != std::string::npos)
                    staged.excludeNames.push_back(name.substr(start, end - start + 1));
            }
        }
        staged.protectFriends = GetBoolDefault("BotLevelBrackets.ProtectFriends", true);
        staged.protectGuilds = GetBoolDefault("BotLevelBrackets.ProtectGuilds", true);
        staged.protectOfflineHumans = GetBoolDefault("BotLevelBrackets.ProtectOfflineHumans", true);
        staged.socialCacheRefreshSeconds = ReadBoundedUInt("BotLevelBrackets.SocialCacheRefreshSeconds", 120, 10, 3600);

        // 8. Activities Master
        staged.activitiesEnabled = GetBoolDefault("BotLevelBrackets.Activities.Enabled", true);
        staged.activitiesRequireSupported = GetBoolDefault("BotLevelBrackets.Activities.RequireSupported", false);

        // Load 43 activities configurations from config
        auto& actReg = ActivityRegistry::Instance();
        for (const auto& def : actReg.GetAllDefinitions())
        {
            std::string prefix = std::string("BotLevelBrackets.Activities.") + def.name + ".";
            ActivityConfig ac;
            ac.enabled = GetBoolDefault((prefix + "Enabled").c_str(), def.defaultConfig.enabled);
            ac.cityEnabled = GetBoolDefault((prefix + "CityEnabled").c_str(), def.defaultConfig.cityEnabled);
            ac.worldEnabled = GetBoolDefault((prefix + "WorldEnabled").c_str(), def.defaultConfig.worldEnabled);
            ac.weight = GetFloatDefault((prefix + "Weight").c_str(), def.defaultConfig.weight);
            ac.cooldownSeconds = ReadBoundedUInt((prefix + "CooldownSeconds").c_str(), def.defaultConfig.cooldownSeconds, 0, 86400);
            ac.maxOutstandingPerFaction = ReadBoundedUInt((prefix + "MaxOutstandingPerFaction").c_str(), def.defaultConfig.maxOutstandingPerFaction, 0, 50);

            actReg.SetConfig(def.id, ac);
        }

        // 9. Debug environment
        std::string dbgModeStr = GetStringDefault("BotLevelBrackets.Debug.Mode", "Off");
        if (dbgModeStr == "Summary")
            staged.debugMode = DEBUG_MODE_SUMMARY;
        else if (dbgModeStr == "Trace")
            staged.debugMode = DEBUG_MODE_TRACE;
        else if (dbgModeStr == "DryRun")
            staged.debugMode = DEBUG_MODE_DRY_RUN;
        else
            staged.debugMode = DEBUG_MODE_OFF;

        staged.debugOutput = GetStringDefault("BotLevelBrackets.Debug.Output", "CoreLog");
        staged.debugCategoriesStr = GetStringDefault("BotLevelBrackets.Debug.Categories", "Summary");
        staged.debugBotGuidsStr = GetStringDefault("BotLevelBrackets.Debug.BotGuids", "");
        staged.debugActivitiesStr = GetStringDefault("BotLevelBrackets.Debug.Activities", "");
        staged.debugCitiesStr = GetStringDefault("BotLevelBrackets.Debug.Cities", "");
        staged.debugRegionsStr = GetStringDefault("BotLevelBrackets.Debug.Regions", "");

        staged.debugSamplePct = ReadBoundedUInt("BotLevelBrackets.Debug.SamplePct", 10, 1, 100);
        staged.debugMaxEventsPerSecond = ReadBoundedUInt("BotLevelBrackets.Debug.MaxEventsPerSecond", 50, 1, 1000);
        staged.debugRingBufferEvents = ReadBoundedUInt("BotLevelBrackets.Debug.RingBufferEvents", 2000, 100, 50000);
        staged.debugRetentionSeconds = ReadBoundedUInt("BotLevelBrackets.Debug.RetentionSeconds", 3600, 60, 86400);

        staged.debugPerformanceEnabled = GetBoolDefault("BotLevelBrackets.Debug.Performance.Enabled", true);
        staged.debugAssertionsEnabled = GetBoolDefault("BotLevelBrackets.Debug.Assertions.Enabled", false);
        staged.debugExportEnabled = GetBoolDefault("BotLevelBrackets.Debug.Export.Enabled", false);
        staged.debugExportDirectory = GetStringDefault("BotLevelBrackets.Debug.Export.Directory", "logs/bot_level_brackets");
        staged.debugExportMaxFileMiB = ReadBoundedUInt("BotLevelBrackets.Debug.Export.MaxFileMiB", 10, 1, 100);
        staged.debugExportMaxFiles = ReadBoundedUInt("BotLevelBrackets.Debug.Export.MaxFiles", 5, 1, 50);
        staged.debugTraceMaxSeconds = ReadBoundedUInt("BotLevelBrackets.Debug.TraceMaxSeconds", 300, 10, 3600);

        // 10. Level ranges
        staged.numRanges = ReadBoundedUInt("BotLevelBrackets.NumRanges", 6, 1, 15);
        staged.allianceLevelRanges.clear();
        staged.hordeLevelRanges.clear();
        staged.allianceStaticPcts.clear();
        staged.hordeStaticPcts.clear();

        for (uint32 i = 1; i <= staged.numRanges; ++i)
        {
            std::string aPre = "BotLevelBrackets.Alliance.Range" + std::to_string(i) + ".";
            std::string hPre = "BotLevelBrackets.Horde.Range" + std::to_string(i) + ".";

            LevelRange ar;
            ar.lower = static_cast<uint8>(ReadBoundedUInt((aPre + "Lower").c_str(), (i == 1 ? 1 : (i - 1) * 10), 1, 60));
            ar.upper = static_cast<uint8>(ReadBoundedUInt((aPre + "Upper").c_str(), (i == staged.numRanges ? 60 : i * 10 - 1), 1, 60));
            ar.desiredPercent = static_cast<uint8>(ReadBoundedUInt((aPre + "Pct").c_str(), 100 / staged.numRanges, 0, 100));
            staged.allianceLevelRanges.push_back(ar);
            staged.allianceStaticPcts.push_back(ar.desiredPercent);

            LevelRange hr;
            hr.lower = static_cast<uint8>(ReadBoundedUInt((hPre + "Lower").c_str(), (i == 1 ? 1 : (i - 1) * 10), 1, 60));
            hr.upper = static_cast<uint8>(ReadBoundedUInt((hPre + "Upper").c_str(), (i == staged.numRanges ? 60 : i * 10 - 1), 1, 60));
            hr.desiredPercent = static_cast<uint8>(ReadBoundedUInt((hPre + "Pct").c_str(), 100 / staged.numRanges, 0, 100));
            staged.hordeLevelRanges.push_back(hr);
            staged.hordeStaticPcts.push_back(hr.desiredPercent);
        }

        return true;
    }

    bool ConfigManager::LoadConfiguration(bool isReload)
    {
        ModuleConfiguration staged;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            staged.generation = isReload ? m_activeConfig.generation + 1 : 1;
        }

        std::string errorMsg;
        if (!ParseAndValidate(staged, errorMsg))
        {
            sLog.outError("BotLevelBrackets: Configuration validation failed: %s. Previous configuration retained.",
                          errorMsg.c_str());
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_activeConfig = staged;
        }

        sLog.outString("BotLevelBrackets: Configuration generation %u published successfully (City=%s, World=%s, Fallback=%s).",
                       staged.generation,
                       staged.cityEnabled ? "ON" : "OFF",
                       staged.worldEnabled ? "ON" : "OFF",
                       staged.fallbackEnabled ? "ON" : "OFF");
        return true;
    }
}
