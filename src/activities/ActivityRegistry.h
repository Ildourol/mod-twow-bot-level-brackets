#ifndef MOD_TWOW_BOT_LEVEL_BRACKETS_ACTIVITY_REGISTRY_H
#define MOD_TWOW_BOT_LEVEL_BRACKETS_ACTIVITY_REGISTRY_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace BotLevelBrackets
{

enum class SupportClass : uint8_t
{
    RequestableExact = 0,
    RequestableNativeChoice = 1,
    LocalOnly = 2,
    ObserveOnly = 3,
    Unavailable = 4
};

const char* GetSupportClassName(SupportClass sc);

struct ActivityDefinition
{
    std::string id;
    SupportClass supportClass;
    bool enabled;
    bool cityEnabled;
    bool worldEnabled;
    float weight;
    uint32_t cooldownSeconds;
    uint32_t maxOutstandingPerFaction;
    std::string nativeActionName;
    std::string description;
};

class ActivityRegistry
{
public:
    static ActivityRegistry& Instance();

    void LoadDefaults();

    bool IsActivitiesEnabled() const { return m_activitiesMasterEnabled; }
    void SetActivitiesEnabled(bool val) { m_activitiesMasterEnabled = val; }

    bool RequireSupported() const { return m_requireSupported; }
    void SetRequireSupported(bool val) { m_requireSupported = val; }

    const std::vector<ActivityDefinition>& GetAllActivities() const { return m_activities; }
    ActivityDefinition* GetActivity(const std::string& id);
    const ActivityDefinition* GetActivity(const std::string& id) const;

    bool IsRequestable(const std::string& id, bool isCity) const;

private:
    ActivityRegistry();
    bool m_activitiesMasterEnabled = true;
    bool m_requireSupported = false;
    std::vector<ActivityDefinition> m_activities;
    std::unordered_map<std::string, size_t> m_indexMap;
};

} // namespace BotLevelBrackets

#endif // MOD_TWOW_BOT_LEVEL_BRACKETS_ACTIVITY_REGISTRY_H
