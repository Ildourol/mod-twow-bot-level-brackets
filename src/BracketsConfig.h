#ifndef BRACKETS_CONFIG_H
#define BRACKETS_CONFIG_H

#include "BracketsCommon.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <cstdint>

namespace Brackets
{
    struct LevelRange
    {
        uint8 lower;
        uint8 upper;
        uint8 desiredPercent;
    };

    struct ModuleConfiguration
    {
        uint32 generation{1};

        // Master & Domain switches
        bool   enabled{true};
        bool   cityEnabled{true};
        bool   worldEnabled{true};
        std::string cityActivationMode{"Augmenter"};

        // Opportunities shared scheduling budgets
        uint32 maxAdmissionsPerCycle{5};
        uint32 maxOutstandingPerFaction{10};
        uint32 checkSeconds{90};
        uint32 stabilitySeconds{60};
        uint32 crossDomainCooldownSeconds{1800};
        uint32 failureBackoffSeconds{600};

        // World outdoor distribution
        uint32 fieldReservePct{70};
        uint32 maxInboundPerRegion{3};
        bool   followHumanLocations{false};
        std::string regionConfig{""};

        // Level policy
        LevelPolicyMode levelPolicyMode{LEVEL_POLICY_OBSERVE_NATIVE};
        bool   reportDesiredTargets{true};
        uint32 minBracketChangeThreshold{1};
        float  realPlayerWeight{1.0f};
        bool   syncFactions{false};

        // Hub disperse quotas & limits
        uint32 hubQuotaMode{2}; // 1 = Fixed headcount, 2 = Percent
        uint32 hubLimitsMode{1}; // 0 = Disabled, 1 = Fixed Headcount, 2 = Percentage
        bool   hubMinLimitEnabled{false};
        bool   hubMaxLimitEnabled{true};
        uint32 hubMinBotsPerHub{0};
        uint32 hubMaxBotsPerHub{50};
        uint32 hubHysteresisBots{2};
        uint32 hubCityPcts[CITY_COUNT]{ 7, 5, 4, 4, 7, 5, 4, 4 };

        // City Fallback (Option 3 restricted compatibility path)
        bool   fallbackEnabled{false};
        std::string fallbackCities{""};
        std::unordered_set<std::string> fallbackCityAllowlist;
        std::string fallbackAllowedReason{"NativeRouteUnsupportedBeforeStart"};
        uint32 fallbackMaxPerCycle{1};
        uint32 fallbackMaxPerHour{2};
        uint32 fallbackMaxSharePct{10};
        uint32 fallbackCooldownSeconds{7200};
        uint32 fallbackEarlyDepartureSeconds{120};
        uint32 fallbackEarlyDepartureBackoffSeconds{21600};

        // Social protection & exclusions
        std::string excludeNamesStr{""};
        std::vector<std::string> excludeNames;
        bool   protectFriends{true};
        bool   protectGuilds{true};
        bool   protectOfflineHumans{true};
        uint32 socialCacheRefreshSeconds{120};

        // Activities master
        bool   activitiesEnabled{true};
        bool   activitiesRequireSupported{false};

        // Debug environment
        DebugMode debugMode{DEBUG_MODE_OFF};
        std::string debugOutput{"CoreLog"};
        std::string debugCategoriesStr{"Summary"};
        std::string debugBotGuidsStr{""};
        std::string debugActivitiesStr{""};
        std::string debugCitiesStr{""};
        std::string debugRegionsStr{""};
        uint32 debugSamplePct{10};
        uint32 debugMaxEventsPerSecond{50};
        uint32 debugRingBufferEvents{2000};
        uint32 debugRetentionSeconds{3600};
        bool   debugPerformanceEnabled{true};
        bool   debugAssertionsEnabled{false};
        bool   debugExportEnabled{false};
        std::string debugExportDirectory{"logs/bot_level_brackets"};
        uint32 debugExportMaxFileMiB{10};
        uint32 debugExportMaxFiles{5};
        uint32 debugTraceMaxSeconds{300};

        // Level Ranges
        uint32 numRanges{6};
        std::vector<LevelRange> allianceLevelRanges;
        std::vector<LevelRange> hordeLevelRanges;
        std::vector<uint8> allianceStaticPcts;
        std::vector<uint8> hordeStaticPcts;
    };

    class ConfigManager
    {
    public:
        static ConfigManager& Instance()
        {
            static ConfigManager instance;
            return instance;
        }

        bool LoadConfiguration(bool isReload = false);
        const ModuleConfiguration& GetConfig() const;

        uint32 GetGeneration() const;
        bool IsFallbackAllowedForCity(const std::string& cityName) const;

    private:
        ConfigManager();
        ~ConfigManager() = default;

        bool ParseAndValidate(ModuleConfiguration& staged, std::string& errorMsg);

        mutable std::mutex m_mutex;
        ModuleConfiguration m_activeConfig;
    };
}

#endif // BRACKETS_CONFIG_H
