#include "mod-twow-bot-level-brackets.h"
#include "compat/NativeTaskAdapter.h"
#include "policy/PopulationPolicy.h"
#include "policy/QuotaAllocator.h"
#include "world/ReadOnlyCatalog.h"
#include "world/Census.h"
#include "scheduling/Coordinator.h"
#include "scheduling/RequestLedger.h"
#include "activities/ActivityRegistry.h"
#include "social/ProtectionCache.h"
#include "commands/BotBracketsCommands.h"
#include "debug/DiagnosticManager.h"

#ifdef HAVE_TORTOISEBOTS
#include "Common.h"
#include "ScriptObjects.h"
#include "ScriptMgr.h"
#include "Objects/Player.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "Chat/Chat.h"
#include "Log.h"
#include "Config/Config.h"
#include "World.h"
#include "Group/Group.h"
#include "Guild/Guild.h"
#elif defined(HAVE_LEGACY_PLAYERBOTS)
#include "Common.h"
#include "ScriptObjects.h"
#include "ScriptMgr.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "Chat.h"
#include "Log.h"
#include "Config/Config.h"
#include "World.h"
#include "Group.h"
#include "Guild.h"
#else
// Test doubles / generic
#include "support/Framework.h"
#endif

#include <string>
#include <algorithm>

using namespace BotLevelBrackets;

namespace
{

void LoadModuleConfiguration()
{
    auto& policy = PopulationPolicy::Instance();
    auto& reg = ActivityRegistry::Instance();
    auto& diag = DiagnosticManager::Instance();
    auto& prot = ProtectionCache::Instance();

    // 1. Core Orchestration Settings
    policy.SetEnabled(sConfig.GetBoolDefault("BotLevelBrackets.Enabled", true));
    policy.SetDryRun(sConfig.GetBoolDefault("BotLevelBrackets.Orchestration.DryRun", false));
    policy.SetCheckCadenceSeconds(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Orchestration.CheckSeconds", 90)));
    policy.SetMaxRequestsPerCycle(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Orchestration.MaxRequestsPerCycle", 5)));
    policy.SetMaxOutstandingPerFaction(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Orchestration.MaxOutstandingPerFaction", 10)));
    policy.SetMaxLooseRequestsPerCycle(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Orchestration.MaxLooseRequestsPerCycle", 1)));
    policy.SetRequestCooldownSeconds(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Orchestration.RequestCooldownSeconds", 1800)));
    policy.SetRejectionBackoffSeconds(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Orchestration.RejectionBackoffSeconds", 600)));
    policy.SetStabilitySeconds(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Orchestration.StabilitySeconds", 60)));
    policy.SetDispatchTimeoutSeconds(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Orchestration.DispatchTimeoutSeconds", 30)));
    policy.SetEarlyDepartureBackoffSeconds(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Orchestration.EarlyDepartureBackoffSeconds", 3600)));

    std::string modeStr = sConfig.GetStringDefault("BotLevelBrackets.Orchestration.Mode", "ExistingNativeTasks");
    std::string unkPolicyStr = sConfig.GetStringDefault("BotLevelBrackets.Orchestration.UnknownStatePolicy", "ObserveOnly");

    // 2. City Orchestration Settings
    policy.SetCityEnabled(sConfig.GetBoolDefault("BotLevelBrackets.City.Enabled", true));

    std::string cityDestStr = sConfig.GetStringDefault("BotLevelBrackets.City.DestinationPolicy", "RequireExact");
    policy.SetCityDestinationPolicy((cityDestStr == "AllowNativeChoice") ? DestinationPolicy::AllowNativeChoice : DestinationPolicy::RequireExact);

    policy.SetCityQuotaMode(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.City.QuotaMode", 2)));
    policy.SetCityMinLimitEnabled(sConfig.GetBoolDefault("BotLevelBrackets.City.MinLimitEnabled", false));
    policy.SetCityMaxLimitEnabled(sConfig.GetBoolDefault("BotLevelBrackets.City.MaxLimitEnabled", true));
    policy.SetCityLimitsMode(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.City.LimitsMode", 1)));
    policy.SetCityMinBots(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.City.MinBotsPerCity", 0)));
    policy.SetCityMaxBots(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.City.MaxBotsPerCity", 50)));
    policy.SetCityHysteresis(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.City.HysteresisBots", 2)));
    policy.SetCityDwellDurationMs(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.City.DwellDurationMs", 180000)));

    policy.SetCityTarget(0, sConfig.GetIntDefault("BotLevelBrackets.City.StormwindTarget", 7));
    policy.SetCityTarget(1, sConfig.GetIntDefault("BotLevelBrackets.City.IronforgeTarget", 5));
    policy.SetCityTarget(2, sConfig.GetIntDefault("BotLevelBrackets.City.DarnassusTarget", 4));
    policy.SetCityTarget(3, sConfig.GetIntDefault("BotLevelBrackets.City.AlahThalasTarget", 4));
    policy.SetCityTarget(4, sConfig.GetIntDefault("BotLevelBrackets.City.OrgrimmarTarget", 7));
    policy.SetCityTarget(5, sConfig.GetIntDefault("BotLevelBrackets.City.UndercityTarget", 5));
    policy.SetCityTarget(6, sConfig.GetIntDefault("BotLevelBrackets.City.ThunderBluffTarget", 4));
    policy.SetCityTarget(7, sConfig.GetIntDefault("BotLevelBrackets.City.SparkwaterTarget", 4));

    // 3. Outdoor World Distribution Settings
    policy.SetWorldEnabled(sConfig.GetBoolDefault("BotLevelBrackets.World.Enabled", true));
    std::string worldDestStr = sConfig.GetStringDefault("BotLevelBrackets.World.DestinationPolicy", "RequireExact");
    policy.SetWorldDestinationPolicy((worldDestStr == "AllowNativeChoice") ? DestinationPolicy::AllowNativeChoice : DestinationPolicy::RequireExact);
    policy.SetFieldReservePct(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.World.FieldReservePct", 70)));

    // 4. Level & Population Policy Settings (Advisory)
    std::string lvlModeStr = sConfig.GetStringDefault("BotLevelBrackets.LevelPolicy.Mode", "ObserveNative");
    std::string popModeStr = sConfig.GetStringDefault("BotLevelBrackets.Population.Mode", "ObserveNative");
    policy.SetUseDynamicDistribution(sConfig.GetBoolDefault("BotLevelBrackets.LevelPolicy.UseDynamic", false));
    policy.SetRealPlayerWeight(sConfig.GetFloatDefault("BotLevelBrackets.LevelPolicy.HumanWeight", 1.0f));
    policy.SetSyncFactions(sConfig.GetBoolDefault("BotLevelBrackets.LevelPolicy.SyncFactions", false));

    // 5. Social & Identity Protections
    prot.SetIgnoreFriendListed(sConfig.GetBoolDefault("BotLevelBrackets.Protection.IgnoreFriendListed", true));
    prot.SetIgnoreRealPlayerGuilds(sConfig.GetBoolDefault("BotLevelBrackets.Protection.IgnoreRealPlayerGuilds", true));
    prot.SetIgnoreArenaTeams(sConfig.GetBoolDefault("BotLevelBrackets.Protection.IgnoreArenaTeams", true));
    prot.SetSocialRefreshCadence(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Protection.SocialRefreshSeconds", 300)));
    prot.SetExcludedNames(sConfig.GetStringDefault("BotLevelBrackets.Protection.ExcludedBotNames", ""));

    // 6. Activity Registry Controls
    reg.SetActivitiesEnabled(sConfig.GetBoolDefault("BotLevelBrackets.Activities.Enabled", true));
    reg.SetRequireSupported(sConfig.GetBoolDefault("BotLevelBrackets.Activities.RequireSupported", false));

    auto loadAct = [&reg](const std::string& id)
    {
        ActivityDefinition* a = reg.GetActivity(id);
        if (!a)
            return;
        std::string prefix = "BotLevelBrackets.Activities." + id + ".";
        a->enabled = sConfig.GetBoolDefault((prefix + "Enabled").c_str(), a->enabled);
        a->cityEnabled = sConfig.GetBoolDefault((prefix + "CityEnabled").c_str(), a->cityEnabled);
        a->worldEnabled = sConfig.GetBoolDefault((prefix + "WorldEnabled").c_str(), a->worldEnabled);
        a->weight = sConfig.GetFloatDefault((prefix + "Weight").c_str(), a->weight);
        a->cooldownSeconds = static_cast<uint32_t>(sConfig.GetIntDefault((prefix + "CooldownSeconds").c_str(), a->cooldownSeconds));
        a->maxOutstandingPerFaction = static_cast<uint32_t>(sConfig.GetIntDefault((prefix + "MaxOutstandingPerFaction").c_str(), a->maxOutstandingPerFaction));
    };

    loadAct("QuestAccept");
    loadAct("QuestObjective");
    loadAct("QuestTurnIn");
    loadAct("Grinding");
    loadAct("Exploration");
    loadAct("GenericRpg");
    loadAct("VendorSell");
    loadAct("VendorBuy");
    loadAct("Repair");
    loadAct("ClassTraining");
    loadAct("ProfessionTraining");
    loadAct("AuctionSell");
    loadAct("Mailbox");
    loadAct("FlightDiscovery");

    // 7. Debug Settings
    std::string debugModeStr = sConfig.GetStringDefault("BotLevelBrackets.Debug.Mode", "Off");
    if (debugModeStr == "Summary")
        diag.SetDebugMode(DebugMode::Summary);
    else if (debugModeStr == "Trace")
        diag.SetDebugMode(DebugMode::Trace);
    else if (debugModeStr == "DryRun")
        diag.SetDebugMode(DebugMode::DryRun);
    else
        diag.SetDebugMode(DebugMode::Off);

    diag.SetMaxEventsPerSecond(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Debug.MaxEventsPerSecond", 50)));
    diag.SetRingBufferCapacity(static_cast<size_t>(sConfig.GetIntDefault("BotLevelBrackets.Debug.RingBufferEvents", 2000)));

    policy.IncrementConfigGeneration();
    RequestLedger::Instance().WithdrawStaleProposals(policy.GetConfigGeneration());
}

} // anonymous namespace

class BotLevelBracketsWorldScript : public WorldScript
{
public:
    BotLevelBracketsWorldScript() : WorldScript("BotLevelBracketsWorldScript") {}

    void OnStartup() override
    {
        sLog.outString("======================================================================");
        sLog.outString("  mod-twow-bot-level-brackets: Option 1 Native-Task Orchestrator      ");
        sLog.outString("  Unchanged Playerbots Executes; Zero Core/Bot Modifications           ");
        sLog.outString("  Active Engine: %s", NativeTaskAdapter::GetEngineName());
        sLog.outString("======================================================================");

        LoadModuleConfiguration();
        ProtectionCache::Instance().Initialize();
    }

    void OnAfterConfigLoad(bool reload) override
    {
        if (reload)
        {
            sLog.outString("mod-twow-bot-level-brackets: Reloading configuration...");
            LoadModuleConfiguration();
            ProtectionCache::Instance().RefreshFromDatabase();
            sLog.outString("mod-twow-bot-level-brackets: Configuration reloaded successfully.");
        }
    }

    void OnUpdate(uint32 diff) override
    {
        Coordinator::Instance().Update(diff);
    }
};

class BotLevelBracketsPlayerScript : public PlayerScript
{
public:
    BotLevelBracketsPlayerScript() : PlayerScript("BotLevelBracketsPlayerScript") {}

    void OnLogin(Player* player) override
    {
        (void)player;
        // On bot or player login, census updates automatically on next cycle
    }

    void OnLogout(Player* player) override
    {
        if (!player)
            return;

        uint32_t guidLow = player->GetObjectGuid().GetCounter();
        const LedgerEntry* active = RequestLedger::Instance().GetActiveEntryForBot(guidLow);
        if (active)
        {
            RequestLedger::Instance().MarkReleased(
                active->requestId, "BotLogout", static_cast<uint32_t>(time(nullptr)));
        }
    }
};

class BotLevelBracketsGuildScript : public GuildScript
{
public:
    BotLevelBracketsGuildScript() : GuildScript("BotLevelBracketsGuildScript") {}

    void OnAddMember(Guild* guild, Player* player, uint8&) override
    {
        if (!guild || !player)
            return;

        bool isReal = player->GetSession() && !player->GetSession()->IsHeadless();
        if (isReal)
            ProtectionCache::Instance().OnGuildMemberAdd(guild->GetId(), true);
    }

    void OnRemoveMember(Guild* guild, Player* player, bool, bool) override
    {
        if (!guild || !player)
            return;

        bool isReal = player->GetSession() && !player->GetSession()->IsHeadless();
        if (isReal)
            ProtectionCache::Instance().OnGuildMemberRemove(guild->GetId(), true);
    }
};

class BotLevelBracketsCommandScript : public AllCommandScript
{
public:
    BotLevelBracketsCommandScript() : AllCommandScript("BotLevelBracketsCommandScript") {}

    bool CanExecuteCommand(ChatHandler* handler, const char* cmd, const char* subCmd) override
    {
        if (!cmd)
            return true;

        if (strcmp(cmd, "blb") == 0)
        {
            if (handler && handler->GetSession() && handler->GetSession()->GetSecurity() < SEC_MODERATOR)
            {
                handler->SendSysMessage("You do not have permission to execute BotLevelBrackets commands.");
                return false;
            }

            BotBracketsCommands::HandleCommand(handler, subCmd, "");
            return false; // Command fully consumed
        }
        return true;
    }
};

void Addmod_twow_bot_level_bracketsScripts()
{
    new BotLevelBracketsWorldScript();
    new BotLevelBracketsPlayerScript();
    new BotLevelBracketsGuildScript();
    new BotLevelBracketsCommandScript();
}
