#pragma once
// Test doubles for deterministic module behavior checks. These are not server headers.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using uint8=uint8_t; using int8=int8_t; using uint16=uint16_t;
using uint32=uint32_t; using int32=int32_t; using uint64=uint64_t; using int64=int64_t;
#define UI64FMTD "%llu"

enum {
    TEAM_ALLIANCE = 0, TEAM_HORDE = 1, CONFIG_UINT32_MAX_PLAYER_LEVEL = 0,
    CURRENT_GENERIC_SPELL = 0, CURRENT_CHANNELED_SPELL = 1, PLAYER_XP = 0, PLAYER_NEXT_LEVEL_XP = 1,
    RACE_HUMAN = 1, RACE_ORC = 2, RACE_DWARF = 3, RACE_NIGHTELF = 4, RACE_UNDEAD = 5,
    RACE_TAUREN = 6, RACE_GNOME = 7, RACE_TROLL = 8, RACE_GOBLIN = 9, RACE_HIGH_ELF = 10,
    CLASS_WARRIOR = 1, CLASS_PALADIN = 2, CLASS_HUNTER = 3, CLASS_ROGUE = 4, CLASS_PRIEST = 5,
    CLASS_SHAMAN = 7, CLASS_MAGE = 8, CLASS_WARLOCK = 9, CLASS_DRUID = 11,
    WORLDHOOK_ON_STARTUP = 1, WORLDHOOK_ON_UPDATE = 2, WORLDHOOK_ON_AFTER_CONFIG_LOAD = 3,
    PLAYERHOOK_ON_LOGIN = 1, PLAYERHOOK_ON_LOGOUT = 2
};

enum AccountTypes { SEC_PLAYER = 0, SEC_MODERATOR = 1, SEC_ADMINISTRATOR = 3 };

struct ObjectGuid {
    uint64 value = 0;
    ObjectGuid(uint64 v = 0) : value(v) {}
    uint64 GetRawValue() const { return value; }
    uint32 GetCounter() const { return static_cast<uint32>(value); }
    bool IsEmpty() const { return !value; }
    bool operator==(ObjectGuid o) const { return value == o.value; }
    bool operator!=(ObjectGuid o) const { return value != o.value; }
    bool operator<(ObjectGuid o) const { return value < o.value; }
};

namespace std {
    template<> struct hash<ObjectGuid> {
        size_t operator()(ObjectGuid g) const { return static_cast<size_t>(g.value); }
    };
}

struct Player;

struct WorldSession {
    Player* player = nullptr;
    bool headless = true;
    bool logout = false;
    AccountTypes security = SEC_ADMINISTRATOR;

    bool IsHeadless() const { return headless; }
    bool isLogingOut() const { return logout; }
    Player* GetPlayer() { return player; }
    AccountTypes GetSecurity() const { return security; }
};

struct GroupReference {
    Player* player = nullptr;
    GroupReference* following = nullptr;
    Player* GetSource() { return player; }
    GroupReference* next() { return following; }
};

struct Group {
    GroupReference* first = nullptr;
    GroupReference* GetFirstMember() { return first; }
};

struct Map {
    uint32 id = 0;
    bool dungeon = false, raid = false, bg = false;
    uint32 GetId() const { return id; }
    bool IsDungeon() const { return dungeon; }
    bool IsRaid() const { return raid; }
    bool IsBattleGround() const { return bg; }
};

struct Player {
    ObjectGuid guid;
    WorldSession session;
    Map map;
    Group* group = nullptr;
    std::string name = "bot";
    uint8 level = 60, team = TEAM_ALLIANCE, race = RACE_HUMAN;
    uint32 guild = 0, area = 0;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    bool inWorld = true, alive = true, combat = false, taxi = false, teleporting = false;
    bool mounted = false, arena = false, bg = false, bgQueue = false;
    bool master = false, random = true;
    uint32 teleports = 0, saves = 0;

    explicit Player(uint64 id = 1) : guid(id) { session.player = this; }

    ObjectGuid GetObjectGuid() const { return guid; }
    uint32 GetGUIDLow() const { return guid.GetCounter(); }
    WorldSession* GetSession() { return &session; }
    bool IsInWorld() const { return inWorld; }
    bool IsAlive() const { return alive; }
    bool IsInCombat() const { return combat; }
    bool IsTaxiFlying() const { return taxi; }
    bool IsBeingTeleported() const { return teleporting; }
    bool IsMounted() const { return mounted; }
    void Unmount() { mounted = false; }
    uint8 GetLevel() const { return level; }
    void SetLevel(uint8 l) { level = l; }
    void GiveLevel(uint8 l) { level = l; }
    void InitStatsForLevel(bool) {}
    void SetUInt32Value(uint32, uint32) {}
    void SaveToDB() { ++saves; }
    uint8 GetTeamId() const { return team; }
    uint8 GetRace() const { return race; }
    uint8 GetClass() const { return CLASS_WARRIOR; }
    uint32 GetGuildId() const { return guild; }
    uint32 GetAreaId() const { return area; }
    uint32 GetMapId() const { return map.id; }
    Map* GetMap() { return &map; }
    Group* GetGroup() { return group; }
    const char* GetName() const { return name.c_str(); }
    float GetPositionX() const { return x; }
    float GetPositionY() const { return y; }
    float GetPositionZ() const { return z; }
    bool InArena() const { return arena; }
    bool InGurubashiArena(bool) const { return false; }
    bool InBattleGround() const { return bg; }
    bool InBattleGroundQueue() const { return bgQueue; }
    void* GetTrader() const { return nullptr; }
    ObjectGuid GetLootGuid() const { return {}; }
    void* GetCurrentSpell(uint32) const { return nullptr; }
    bool TeleportTo(uint32 m, float a, float b, float c, float) {
        ++teleports;
        map.id = m; x = a; y = b; z = c;
        return true;
    }
};

template<class T>
struct HashMapHolder {
    using MapType = std::unordered_map<ObjectGuid, T*>;
};

struct Accessor {
    HashMapHolder<Player>::MapType players;
    auto& GetPlayers() { return players; }
    Player* FindPlayer(ObjectGuid g) {
        auto it = players.find(g);
        return it == players.end() ? nullptr : it->second;
    }
} inline sObjectAccessor;

struct World {
    uint32 getConfig(uint32) { return 60; }
} inline sWorld;

struct ObjectManager {
    uint32 GetXPForLevel(uint32) { return 100; }
} inline sObjectMgr;

struct Logger {
    template<class... T> void outString(const char*, T...) {}
    template<class... T> void outError(const char*, T...) {}
} inline sLog;

struct Config {
    std::unordered_map<std::string, std::string> values;
    bool reloadOk = true;

    bool Reload() { return reloadOk; }
    bool SetSource(const char*) { return true; }
    bool LoadModulesConfigs() { return true; }
    std::string GetFilename() { return "mangosd.conf"; }
    bool GetBoolDefault(const char* k, bool d) {
        auto i = values.find(k);
        return i == values.end() ? d : (i->second == "1" || i->second == "true");
    }
    int32 GetIntDefault(const char* k, int32 d) {
        auto i = values.find(k);
        return i == values.end() ? d : std::stoi(i->second);
    }
    float GetFloatDefault(const char* k, float d) {
        auto i = values.find(k);
        return i == values.end() ? d : std::stof(i->second);
    }
    std::string GetStringDefault(const char* k, const char* d) {
        auto i = values.find(k);
        return i == values.end() ? d : i->second;
    }
} inline sConfig;

struct Field {
    uint64 GetUInt64() { return 0; }
    uint32 GetUInt32() { return 0; }
    uint8 GetUInt8() { return 0; }
    int32 GetInt32() { return 0; }
};

struct QueryResult {
    Field fields[4];
    Field* Fetch() { return fields; }
    bool NextRow() { return false; }
};

struct Database {
    std::vector<std::string> writes;
    QueryResult* Query(const char*) { return nullptr; }
    template<class... T> void PExecute(const char* fmt, T...) { writes.emplace_back(fmt); }
} inline CharacterDatabase;

struct ChatHandler {
    WorldSession* session;
    std::vector<std::string> replies;

    explicit ChatHandler(WorldSession* s = nullptr) : session(s) {}
    WorldSession* GetSession() { return session; }
    void SendSysMessage(const char* s) { replies.emplace_back(s); }
    template<class... T> void PSendSysMessage(const char* s, T...) { replies.emplace_back(s); }
};

struct WorldScript {
    explicit WorldScript(const char*, std::vector<uint16> = {}) {}
    virtual ~WorldScript() = default;
    virtual void OnStartup() {}
    virtual void OnAfterConfigLoad(bool) {}
    virtual void OnUpdate(uint32) {}
};

struct PlayerScript {
    explicit PlayerScript(const char*, std::vector<uint16> = {}) {}
    virtual ~PlayerScript() = default;
    virtual void OnLogin(Player*) {}
    virtual void OnLogout(Player*) {}
};

struct Guild {
    uint32 GetId() { return 1; }
};

struct GuildScript {
    explicit GuildScript(const char*) {}
    virtual ~GuildScript() = default;
    virtual void OnAddMember(Guild*, Player*, uint8&) {}
    virtual void OnRemoveMember(Guild*, Player*, bool, bool) {}
};

struct AllCommandScript {
    explicit AllCommandScript(const char*) {}
    virtual ~AllCommandScript() = default;
    virtual bool CanExecuteCommand(ChatHandler*, const char*, const char*) { return true; }
};

inline uint32 urand(uint32 a, uint32) { return a; }
inline double rand_norm() { return 0.25; }

// -----------------------------------------------------------------------------
// TortoiseBots Native Engine Doubles
// -----------------------------------------------------------------------------
#if defined(HAVE_TORTOISEBOTS) || defined(HAVE_LEGACY_PLAYERBOTS)

struct Event {
    Player* owner = nullptr;
    explicit Event(Player* p = nullptr) : owner(p) {}
    Player* getOwner() const { return owner; }
};

struct PlayerbotAI {
    Player* player = nullptr;
    std::unordered_set<std::string> allowedActions{"rpg", "accept quest", "talk to quest giver", "choose travel target", "sell", "buy", "repair", "ah"};
    bool refuseAction = false;

    bool HasRealPlayerMaster() const { return player && player->master; }
    bool DoSpecificAction(const std::string& name, Event = Event(), bool = false) {
        if (refuseAction)
            return false;
        return allowedActions.count(name) > 0;
    }
    void* GetAiObjectContext() { return nullptr; }
};

inline PlayerbotAI* GetBotAI(Player* p) {
    static std::unordered_map<ObjectGuid, PlayerbotAI> ais;
    auto& ai = ais[p->guid];
    ai.player = p;
    return p->session.headless ? &ai : nullptr;
}

struct PlayerbotAIStorage {
    static PlayerbotAIStorage& Instance() { static PlayerbotAIStorage a; return a; }
    PlayerbotAI* GetAI(Player* p) { return ::GetBotAI(p); }
};

namespace TortoiseBots {

enum class CityId : uint8_t {
    None = 0, Stormwind = 1, Ironforge = 2, Darnassus = 3, AlahThalas = 4,
    Orgrimmar = 5, Undercity = 6, ThunderBluff = 7, Sparkwater = 8, Count = 9
};

enum class CityVisitAdmission : uint8_t {
    Accepted = 0, AlreadyActive, Busy, Protected, UnsupportedCity, UnsafeRoute, Unavailable, BudgetExhausted
};

enum class CityVisitCancelReason : uint8_t {
    None = 0, OwnerRequested = 7
};

struct CityVisitService {
    static CityVisitService& Instance() { static CityVisitService s; return s; }

    std::unordered_map<uint32, CityId> activeVisits;
    uint32 inboundCounts[9] = {0};
    uint64 nextReqId = 1;
    bool forceBusy = false;
    bool forceProtected = false;

    CityVisitAdmission RequestVisit(Player* bot, CityId cityId, uint32, const std::string&, uint32, uint64_t& outReqId) {
        if (forceBusy)
            return CityVisitAdmission::Busy;
        if (forceProtected)
            return CityVisitAdmission::Protected;
        if (!bot || !bot->IsInWorld() || !bot->IsAlive() || bot->IsInCombat())
            return CityVisitAdmission::Busy;

        uint32 guid = bot->GetGUIDLow();
        auto it = activeVisits.find(guid);
        if (it != activeVisits.end()) {
            if (it->second == cityId) {
                outReqId = 999;
                return CityVisitAdmission::AlreadyActive;
            }
            return CityVisitAdmission::Busy;
        }

        outReqId = nextReqId++;
        activeVisits[guid] = cityId;
        inboundCounts[static_cast<uint8>(cityId)]++;
        return CityVisitAdmission::Accepted;
    }

    bool CancelVisit(ObjectGuid botGuid, CityVisitCancelReason, const std::string&) {
        uint32 guid = botGuid.GetCounter();
        auto it = activeVisits.find(guid);
        if (it != activeVisits.end()) {
            inboundCounts[static_cast<uint8>(it->second)]--;
            activeVisits.erase(it);
            return true;
        }
        return false;
    }

    bool HasActiveVisit(ObjectGuid botGuid) const {
        return activeVisits.find(botGuid.GetCounter()) != activeVisits.end();
    }

    CityId GetTargetCity(ObjectGuid botGuid) const {
        auto it = activeVisits.find(botGuid.GetCounter());
        return it != activeVisits.end() ? it->second : CityId::None;
    }

    uint32 GetInboundCount(CityId cityId) const {
        return inboundCounts[static_cast<uint8>(cityId)];
    }

    uint32 GetResidentCount(CityId) const { return 0; }

    bool IsBotInCity(Player* bot, CityId cityId) const {
        if (!bot) return false;
        // Check approximate distance to canonical city centers
        float cx = 0, cy = 0;
        uint32 map = 0;
        switch (cityId) {
            case CityId::Stormwind: cx = -8810.0f; cy = 640.0f; map = 0; break;
            case CityId::Ironforge: cx = -4980.0f; cy = -940.0f; map = 0; break;
            case CityId::Orgrimmar: cx = 1570.0f; cy = -4400.0f; map = 1; break;
            default: break;
        }
        if (bot->GetMapId() != map) return false;
        float dx = bot->GetPositionX() - cx, dy = bot->GetPositionY() - cy;
        return (dx * dx + dy * dy) <= (250.0f * 250.0f);
    }

    void Clear() {
        activeVisits.clear();
        for (int i = 0; i < 9; ++i) inboundCounts[i] = 0;
        forceBusy = false; forceProtected = false;
    }
};

struct BotRecord {
    ObjectGuid masterGuid;
};

struct BotManager {
    static BotManager& Instance() { static BotManager m; return m; }
    bool IsBot(ObjectGuid g) {
        auto* p = sObjectAccessor.FindPlayer(g);
        return p && p->session.headless;
    }
    bool IsRandomBot(ObjectGuid g) {
        auto* p = sObjectAccessor.FindPlayer(g);
        return p && p->random;
    }
    BotRecord* FindBot(ObjectGuid g) {
        static BotRecord r;
        auto* p = sObjectAccessor.FindPlayer(g);
        r.masterGuid = (p && p->master) ? ObjectGuid(99) : ObjectGuid();
        return &r;
    }
};

} // namespace TortoiseBots

struct RandomBotFacade {
    static RandomBotFacade& instance() { static RandomBotFacade f; return f; }
    bool isFree = true;
    bool IsFreeBot(Player*) const { return isFree; }
};
#define sRandomBotFacade RandomBotFacade::instance()

struct RandomPlayerbotMgr {
    bool IsRandomBot(Player* p) { return p && p->random; }
} inline sRandomPlayerbotMgr;

#endif // HAVE_TORTOISEBOTS
