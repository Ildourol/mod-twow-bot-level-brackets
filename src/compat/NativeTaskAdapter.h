#ifndef MOD_TWOW_BOT_LEVEL_BRACKETS_NATIVE_TASK_ADAPTER_H
#define MOD_TWOW_BOT_LEVEL_BRACKETS_NATIVE_TASK_ADAPTER_H

#include "../world/ReadOnlyCatalog.h"
#include <cstdint>
#include <string>

class Player;
struct ObjectGuid;

namespace BotLevelBrackets
{

enum class EngineKind : uint8_t
{
    TortoiseBots = 0,
    LegacyPlayerBots = 1,
    GenericHeadless = 2
};

enum class TaskAdmissionResult : uint8_t
{
    Accepted = 0,
    AlreadyActive = 1,
    Busy = 2,
    Protected = 3,
    UnsupportedCity = 4,
    UnsafeRoute = 5,
    Unavailable = 6,
    BudgetExhausted = 7,
    Failed = 8
};

const char* GetTaskAdmissionResultName(TaskAdmissionResult res);

class NativeTaskAdapter
{
public:
    static EngineKind GetEngineKind();
    static const char* GetEngineName();

    // Universal Bot Identification & Availability
    static bool IsBot(Player* player);
    static bool IsRandomBot(Player* player);
    static bool IsFreeBot(Player* player);
    static bool HasRealPlayerMaster(Player* player);
    static bool HasRealPlayerInParty(Player* player);
    static bool IsInWorldAndReady(Player* player);

    // City Visit Orchestration (Option 1 Safe Interface)
    static TaskAdmissionResult RequestCityVisit(Player* bot, CityId cityId,
                                                uint32_t dwellDurationMs,
                                                uint32_t configGeneration,
                                                uint64_t& outRequestId);

    static bool CancelCityVisit(ObjectGuid botGuid, const std::string& ownerId = "mod-twow-bot-level-brackets");
    static bool HasActiveCityVisit(ObjectGuid botGuid);
    static CityId GetTargetCity(ObjectGuid botGuid);
    static uint32_t GetCityInboundCount(CityId cityId);
    static uint32_t GetCityResidentCount(CityId cityId);
    static bool IsBotInCity(Player* bot, CityId cityId);

    // Native Action Dispatch (Gate-Preserving via DoSpecificAction)
    static bool DispatchNativeAction(Player* bot, const std::string& actionName);

    // Native Travel State Inspection (Read-Only)
    static bool IsBotTraveling(Player* bot);
    static std::string GetBotTravelStatusString(Player* bot);
};

} // namespace BotLevelBrackets

#endif // MOD_TWOW_BOT_LEVEL_BRACKETS_NATIVE_TASK_ADAPTER_H
