#include "support/Framework.h"
#include "../src/mod-twow-bot-level-brackets.cpp"

static void Reset()
{
    sObjectAccessor.players.clear(); sConfig.values.clear(); CharacterDatabase.writes.clear();
    g_PendingTeleports.clear(); g_PendingLevelResets.clear(); g_CityStrategyOwners.clear();
    g_PendingCommands.clear(); g_LoggedOutGuids.clear(); g_ExcludeBotNames.clear();
    g_SocialFriendsList.clear(); g_RealPlayerGuildIds.clear(); g_PersistentRealPlayerGuildIds.clear();
    g_HubBotCooldowns.clear(); g_ConfigGeneration=1; g_TeleportExecutionLimit=25; g_TeleportMaxAttempts=3;
    g_PendingQueueTTL=0; g_HubDisperseEnabled=true; g_BotLevelBracketsEnabled=true;
    g_TeleportOnLevelChange=true; g_HubDisperseQuotaMode=1;
    g_HubDisperseMinBotsLimitEnabled=false;g_HubDisperseMaxBotsLimitEnabled=true;g_HubDisperseLimitsMode=1;g_HubDisperseMinBotsPerHub=0;g_HubDisperseMaxBotsPerHub=50;
    g_HubDisperseHysteresisBots=0;g_HubDisperseStabilitySeconds=0;g_NextHubStart=0;
    g_HubDisperseCooldownSeconds=0;g_HubDisperseBotsPerCycle=50;
    g_ManageCityStrategies=true;g_WorldTeleportResetStrategies=true;
    std::fill_n(g_HubCityPcts,8,0);std::fill_n(g_HubStability,8,HubStabilityRecord{});
    g_NumRanges=2;g_RandomBotMinLevel=1;g_RandomBotMaxLevel=60;++g_ConfigGeneration;
    g_AllianceLevelRanges={{1,29,50},{30,60,50}};g_HordeLevelRanges=g_AllianceLevelRanges;
    g_AllianceStaticPcts={50,50};g_HordeStaticPcts={50,50};
}
static void Add(Player& p){sObjectAccessor.players[p.guid]=&p;}
static void InCity(Player& p,size_t h){const auto&a=s_HubAreas[h];p.team=a.teamId;p.map.id=a.mapId;p.x=a.cx;p.y=a.cy;p.z=a.cz;}

int main()
{
    Reset();Player p(1);Add(p);InCity(p,0);
    assert(TeleportBotToLevelZone(&p,60,TEAM_ALLIANCE));assert(!IsInAnyHub(&p));
    InCity(p,4);assert(TeleportBotToLevelZone(&p,60,TEAM_HORDE));assert(!IsInAnyHub(&p));
    // Changing the bracket layout cannot route level 45 to starter geography.
    g_AllianceLevelRanges={{1,60,100}};g_NumRanges=1;
    assert(TeleportBotToLevelZone(&p,45,TEAM_ALLIANCE));assert(p.map.id==1 && p.x==s_AllianceZones[4].x);

    // Hub evacuation must also leave an overlapping custom starter/capital sphere.
    p.level=5;p.race=RACE_HIGH_ELF;InCity(p,3);
    assert(TeleportBotToLevelZone(&p,5,TEAM_ALLIANCE,true));assert(!IsInAnyHub(&p));

    Reset();Player a(1),b(2),c(3);Add(a);Add(b);Add(c);
    assert(EnqueuePendingTeleport(&a,60,0));assert(EnqueuePendingTeleport(&b,60,0));assert(EnqueuePendingTeleport(&c,60,0));
    g_TeleportExecutionLimit=1;ProcessPendingTeleports();assert(a.teleports+b.teleports+c.teleports==1);assert(g_PendingTeleports.size()==2);
    ProcessPendingTeleports();ProcessPendingTeleports();assert(a.teleports+b.teleports+c.teleports==3);
    assert(g_PendingTeleports.empty());

    Reset();Add(a);a.teleports=0;assert(EnqueuePendingTeleport(&a,60,0));g_SocialFriendsList.insert(a.guid.GetCounter());
    ProcessPendingTeleports();assert(a.teleports==0 && g_PendingTeleports.empty());
    g_SocialFriendsList.clear();assert(EnqueuePendingTeleport(&a,60,0));a.guild=7;g_PersistentRealPlayerGuildIds.insert(7);
    ProcessPendingTeleports();assert(a.teleports==0 && g_PendingTeleports.empty());
    a.guild=0;assert(EnqueuePendingTeleport(&a,60,0));g_ExcludeBotNames.push_back("bot");
    ProcessPendingTeleports();assert(a.teleports==0 && g_PendingTeleports.empty());

    Reset();Player failing(1);failing.failTeleport=true;Add(failing);EnqueuePendingTeleport(&failing,60,0);
    for(uint32 n=0;n<3;++n){g_PendingTeleports[failing.guid].nextAttemptAt=0;ProcessPendingTeleports();}
    assert(failing.teleports==3 && g_PendingTeleports.empty());

    Reset();Player stale(1);Add(stale);EnqueuePendingTeleport(&stale,60,0);
    ++g_ConfigGeneration;ProcessPendingTeleports();assert(stale.teleports==0 && g_PendingTeleports.empty());
    EnqueuePendingTeleport(&stale,60,0);g_PendingQueueTTL=1;
    g_PendingTeleports[stale.guid].enqueuedAt=static_cast<uint32>(time(nullptr))-3;
    ProcessPendingTeleports();assert(stale.teleports==0 && g_PendingTeleports.empty());

    Reset();Player offline(4);assert(EnqueuePendingReset(offline.guid,1,0));
    ProcessPendingLevelResets();assert(g_PendingLevelResets.size()==1);
    Add(offline);BotLevelBracketsPlayerScript lifecycle;lifecycle.OnLogout(&offline);assert(g_PendingLevelResets.size()==1);
    offline.level=10;ProcessPendingLevelResets();assert(offline.level>=30 && g_PendingLevelResets.empty());

    Reset();Player protectedBot(1),wild(2);Add(protectedBot);Add(wild);InCity(protectedBot,0);
    protectedBot.guild=10;g_PersistentRealPlayerGuildIds.insert(10);g_HubCityPcts[0]=1;
    auto census=BuildHubCensus();assert(census.alliance==2 && census.residents[0]==1 && census.movable[0]==0);
    ProcessHubPopulate();assert(g_PendingTeleports.empty()); // No extra refill around protected residents.
    EnqueuePendingHubTeleport(&wild,0,s_HubAreas[0].cx,s_HubAreas[0].cy,s_HubAreas[0].cz,0);
    census=BuildHubCensus();assert(census.inbound[0]==1 && EffectiveHubPopulation(census,0)==2);

    Reset();Player excess(1);Add(excess);InCity(excess,0);ProcessHubPopulate();ProcessPendingTeleports();
    assert(!IsInAnyHub(&excess)); // Max/disabled-city evacuation really leaves the city.

    Reset();Player f1(1),f2(2),f3(3),f4(4);Add(f1);Add(f2);Add(f3);Add(f4);
    g_HubDisperseBotsPerCycle=1;g_HubCityPcts[0]=1;g_HubCityPcts[1]=1;
    ProcessHubPopulate();ProcessPendingTeleports();
    ProcessHubPopulate();ProcessPendingTeleports();
    auto fair=BuildHubCensus();assert(fair.residents[0]==1 && fair.residents[1]==1);

    Reset();Player r1(1),r2(2),r3(3),r4(4);r1.level=r2.level=r3.level=r4.level=10;
    Add(r1);Add(r2);Add(r3);Add(r4);g_MinBracketChangeThreshold=0;
    RunDistributionCycle(nullptr);assert(g_PendingLevelResets.size()==2);
    RunDistributionCycle(nullptr);assert(g_PendingLevelResets.size()==2); // Existing moves already fill the deficit.

    Reset();Player gm(99);gm.session.headless=false;Add(gm);ChatHandler chat(&gm.session);
    BotLevelBracketsCommandScript commands;auto cycles=g_StatsCyclesRun.load();
    assert(!commands.CanExecuteCommand(&chat,"blb","force"));assert(g_StatsCyclesRun.load()==cycles);
    DrainBotBracketsCommands();assert(g_StatsCyclesRun.load()==cycles+1);
    gm.session.security=SEC_MODERATOR;commands.CanExecuteCommand(&chat,"blb","force");
    DrainBotBracketsCommands();assert(g_StatsCyclesRun.load()==cycles+1);
    g_BotLevelBracketsEnabled=false;gm.session.security=SEC_ADMINISTRATOR;
    commands.CanExecuteCommand(&chat,"blb","reload");BotLevelBracketsWorldScript world;world.OnUpdate(1);
    assert(g_BotLevelBracketsEnabled); // Reload drains even while disabled.

    // Legacy PopulationLimitsEnabled migration fallback tests
    Reset();
    sConfig.values.clear();
    sConfig.values["BotLevelBrackets.HubDisperse.PopulationLimitsEnabled"] = "0";
    LoadBotLevelBracketsConfig();
    assert(!g_HubDisperseMinBotsLimitEnabled && !g_HubDisperseMaxBotsLimitEnabled);

    Reset();
    sConfig.values.clear();
    sConfig.values["BotLevelBrackets.HubDisperse.PopulationLimitsEnabled"] = "1";
    LoadBotLevelBracketsConfig();
    assert(g_HubDisperseMinBotsLimitEnabled && g_HubDisperseMaxBotsLimitEnabled);

    // Explicit toggles take precedence over legacy toggle
    Reset();
    sConfig.values.clear();
    sConfig.values["BotLevelBrackets.HubDisperse.PopulationLimitsEnabled"] = "1";
    sConfig.values["BotLevelBrackets.HubDisperse.MinBotsLimitEnabled"] = "0";
    sConfig.values["BotLevelBrackets.HubDisperse.MaxBotsLimitEnabled"] = "0";
    LoadBotLevelBracketsConfig();
    assert(!g_HubDisperseMinBotsLimitEnabled && !g_HubDisperseMaxBotsLimitEnabled);

#if defined(HAVE_TORTOISEBOTS) || defined(HAVE_LEGACY_PLAYERBOTS)
    Reset();Player social(1);Add(social);InCity(social,0);
    SyncOwnedCityStrategies(&social,true);assert(social.strategies.count("rpg") && !social.strategies.count("grind"));
    auto applies=g_StatsCityStrategyApplies;SyncOwnedCityStrategies(&social,true);assert(g_StatsCityStrategyApplies==applies);
    social.strategies.insert("custom-user");social.strategies.erase("sit");
    SyncOwnedCityStrategies(&social,false);assert(social.strategies.count("grind") && social.strategies.count("loot"));
    assert(social.strategies.count("custom-user") && !social.strategies.count("rpg"));
    assert(g_CityStrategyOwners.empty());
    // An unrelated outdoor bot is never reset just because it does not grind.
    social.strategies={"custom-user"};SyncOwnedCityStrategies(&social,false);assert(social.strategies.size()==1);
    assert(BotEngineAdapter::GetBotAI(&social)); // Also exercises legacy global accessor resolution.
#endif
}
