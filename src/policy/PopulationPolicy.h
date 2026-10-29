#ifndef MOD_TWOW_BOT_LEVEL_BRACKETS_POPULATION_POLICY_H
#define MOD_TWOW_BOT_LEVEL_BRACKETS_POPULATION_POLICY_H

#include "ReadOnlyCatalog.h"
#include <cstdint>
#include <vector>
#include <string>

namespace BotLevelBrackets
{

enum class DestinationPolicy : uint8_t
{
    RequireExact = 0,
    AllowNativeChoice = 1
};

enum class OrchestrationMode : uint8_t
{
    ExistingNativeTasks = 0
};

enum class LevelPolicyMode : uint8_t
{
    ObserveNative = 0
};

enum class PopulationMode : uint8_t
{
    ObserveNative = 0
};

struct LevelBracketConfig
{
    uint8_t lower;
    uint8_t upper;
    uint8_t desiredPercent;
};

class PopulationPolicy
{
public:
    static PopulationPolicy& Instance();

    void LoadDefaults();

    // Policy getters & setters
    bool IsEnabled() const { return m_enabled; }
    void SetEnabled(bool val) { m_enabled = val; }

    bool IsCityEnabled() const { return m_cityEnabled; }
    void SetCityEnabled(bool val) { m_cityEnabled = val; }

    bool IsWorldEnabled() const { return m_worldEnabled; }
    void SetWorldEnabled(bool val) { m_worldEnabled = val; }

    bool IsDryRun() const { return m_dryRun; }
    void SetDryRun(bool val) { m_dryRun = val; }

    DestinationPolicy GetCityDestinationPolicy() const { return m_cityDestPolicy; }
    void SetCityDestinationPolicy(DestinationPolicy pol) { m_cityDestPolicy = pol; }

    DestinationPolicy GetWorldDestinationPolicy() const { return m_worldDestPolicy; }
    void SetWorldDestinationPolicy(DestinationPolicy pol) { m_worldDestPolicy = pol; }

    uint32_t GetCheckCadenceSeconds() const { return m_checkSeconds; }
    void SetCheckCadenceSeconds(uint32_t val) { m_checkSeconds = val; }

    uint32_t GetMaxRequestsPerCycle() const { return m_maxRequestsPerCycle; }
    void SetMaxRequestsPerCycle(uint32_t val) { m_maxRequestsPerCycle = val; }

    uint32_t GetMaxOutstandingPerFaction() const { return m_maxOutstandingPerFaction; }
    void SetMaxOutstandingPerFaction(uint32_t val) { m_maxOutstandingPerFaction = val; }

    uint32_t GetMaxLooseRequestsPerCycle() const { return m_maxLooseRequestsPerCycle; }
    void SetMaxLooseRequestsPerCycle(uint32_t val) { m_maxLooseRequestsPerCycle = val; }

    uint32_t GetRequestCooldownSeconds() const { return m_requestCooldownSeconds; }
    void SetRequestCooldownSeconds(uint32_t val) { m_requestCooldownSeconds = val; }

    uint32_t GetRejectionBackoffSeconds() const { return m_rejectionBackoffSeconds; }
    void SetRejectionBackoffSeconds(uint32_t val) { m_rejectionBackoffSeconds = val; }

    uint32_t GetStabilitySeconds() const { return m_stabilitySeconds; }
    void SetStabilitySeconds(uint32_t val) { m_stabilitySeconds = val; }

    uint32_t GetDispatchTimeoutSeconds() const { return m_dispatchTimeoutSeconds; }
    void SetDispatchTimeoutSeconds(uint32_t val) { m_dispatchTimeoutSeconds = val; }

    uint32_t GetEarlyDepartureBackoffSeconds() const { return m_earlyDepartureBackoffSeconds; }
    void SetEarlyDepartureBackoffSeconds(uint32_t val) { m_earlyDepartureBackoffSeconds = val; }

    uint32_t GetFieldReservePct() const { return m_fieldReservePct; }
    void SetFieldReservePct(uint32_t val) { m_fieldReservePct = val; }

    uint32_t GetCityDwellDurationMs() const { return m_cityDwellDurationMs; }
    void SetCityDwellDurationMs(uint32_t val) { m_cityDwellDurationMs = val; }

    // Quota math configuration
    uint32_t GetCityQuotaMode() const { return m_cityQuotaMode; } // 1 = count, 2 = pct
    void SetCityQuotaMode(uint32_t val) { m_cityQuotaMode = val; }

    bool IsCityMinLimitEnabled() const { return m_cityMinLimitEnabled; }
    void SetCityMinLimitEnabled(bool val) { m_cityMinLimitEnabled = val; }

    bool IsCityMaxLimitEnabled() const { return m_cityMaxLimitEnabled; }
    void SetCityMaxLimitEnabled(bool val) { m_cityMaxLimitEnabled = val; }

    uint32_t GetCityLimitsMode() const { return m_cityLimitsMode; } // 1 = count, 2 = pct
    void SetCityLimitsMode(uint32_t val) { m_cityLimitsMode = val; }

    uint32_t GetCityMinBots() const { return m_cityMinBots; }
    void SetCityMinBots(uint32_t val) { m_cityMinBots = val; }

    uint32_t GetCityMaxBots() const { return m_cityMaxBots; }
    void SetCityMaxBots(uint32_t val) { m_cityMaxBots = val; }

    uint32_t GetCityHysteresis() const { return m_cityHysteresis; }
    void SetCityHysteresis(uint32_t val) { m_cityHysteresis = val; }

    int32_t GetCityTarget(size_t cityIndex) const;
    void SetCityTarget(size_t cityIndex, int32_t val);

    // Level brackets
    const std::vector<LevelBracketConfig>& GetLevelBrackets(uint8_t team) const;
    void SetLevelBrackets(uint8_t team, const std::vector<LevelBracketConfig>& brackets);

    bool UseDynamicDistribution() const { return m_useDynamicDistribution; }
    void SetUseDynamicDistribution(bool val) { m_useDynamicDistribution = val; }

    float GetRealPlayerWeight() const { return m_realPlayerWeight; }
    void SetRealPlayerWeight(float val) { m_realPlayerWeight = val; }

    bool SyncFactions() const { return m_syncFactions; }
    void SetSyncFactions(bool val) { m_syncFactions = val; }

    uint32_t GetConfigGeneration() const { return m_configGeneration; }
    void IncrementConfigGeneration() { ++m_configGeneration; }

private:
    PopulationPolicy();

    bool m_enabled = true;
    bool m_cityEnabled = true;
    bool m_worldEnabled = true;
    bool m_dryRun = false;

    DestinationPolicy m_cityDestPolicy = DestinationPolicy::RequireExact;
    DestinationPolicy m_worldDestPolicy = DestinationPolicy::RequireExact;

    uint32_t m_checkSeconds = 90;
    uint32_t m_maxRequestsPerCycle = 5;
    uint32_t m_maxOutstandingPerFaction = 10;
    uint32_t m_maxLooseRequestsPerCycle = 1;
    uint32_t m_requestCooldownSeconds = 1800;
    uint32_t m_rejectionBackoffSeconds = 600;
    uint32_t m_stabilitySeconds = 60;
    uint32_t m_dispatchTimeoutSeconds = 30;
    uint32_t m_earlyDepartureBackoffSeconds = 3600;
    uint32_t m_fieldReservePct = 70;
    uint32_t m_cityDwellDurationMs = 180000;

    uint32_t m_cityQuotaMode = 2; // default: 2 (percentage)
    bool m_cityMinLimitEnabled = false;
    bool m_cityMaxLimitEnabled = true;
    uint32_t m_cityLimitsMode = 1; // default: 1 (headcount)
    uint32_t m_cityMinBots = 0;
    uint32_t m_cityMaxBots = 50;
    uint32_t m_cityHysteresis = 2;

    int32_t m_cityTargets[8] = { 7, 5, 4, 4, 7, 5, 4, 4 }; // Alliance: SW, IF, Darn, Alah; Horde: Org, UC, TB, Spark

    std::vector<LevelBracketConfig> m_allianceBrackets;
    std::vector<LevelBracketConfig> m_hordeBrackets;

    bool m_useDynamicDistribution = false;
    float m_realPlayerWeight = 1.0f;
    bool m_syncFactions = false;

    uint32_t m_configGeneration = 1;
};

} // namespace BotLevelBrackets

#endif // MOD_TWOW_BOT_LEVEL_BRACKETS_POPULATION_POLICY_H
