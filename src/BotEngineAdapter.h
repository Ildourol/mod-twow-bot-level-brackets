#ifndef BOT_ENGINE_ADAPTER_H
#define BOT_ENGINE_ADAPTER_H

#include "Common.h"
#include "Objects/Player.h"
#include "ObjectMgr.h"
#include "World.h"
#include "Group/Group.h"
#include "Chat/Chat.h"
#include "Log.h"

#include <string>
#include <vector>
#include <sstream>

// -----------------------------------------------------------------------------
// ENGINE SELECTION & CONDITIONAL HEADERS
// -----------------------------------------------------------------------------
// Priority 1: TortoiseBots (Native Modular Companion Engine for tortoise-wow)
#if defined(HAVE_TORTOISEBOTS)

#include "ai/playerbot/playerbot.h"
#include "ai/playerbot/PlayerbotAI.h"
#include "ai/playerbot/PlayerbotAIConfig.h"
#include "ai/playerbot/PlayerbotFactory.h"
#include "ai/playerbot/ChatHelper.h"
#include "runtime/BotManager.h"
#include "runtime/PlayerbotAIStorage.h"

// Priority 2: Legacy PlayerBots (CMaNGOS / shyalya vendored in older cores)
#elif defined(HAVE_LEGACY_PLAYERBOTS)

#include "botpch.h"
#include "playerbot/playerbot.h"
#include "playerbot/PlayerbotAI.h"
#include "playerbot/PlayerbotAIConfig.h"
#include "playerbot/RandomPlayerbotMgr.h"
#include "playerbot/PlayerbotFactory.h"
#include "playerbot/ChatHelper.h"

// Priority 3: Generic Headless Session Mode (Native tortoise-wow core fallback)
#else

#define HAVE_GENERIC_HEADLESS_BOTS 1

#endif

/**
 * Unified Bot Engine Abstraction Layer.
 *
 * Provides a uniform, zero-overhead compile-time adapter across:
 * 1. TortoiseBots (Native decoupled module for Turtle WoW custom vMaNGOS)
 * 2. Legacy PlayerBots (Vendored monolithic engine in older vMaNGOS forks)
 * 3. Generic Headless Session Mode (Pure tortoise-wow core with zero bot headers)
 */
class PlayerbotAI;

class BotEngineAdapter
{
public:
    enum EngineKind
    {
        ENGINE_TORTOISEBOTS,
        ENGINE_LEGACY_PLAYERBOTS,
        ENGINE_GENERIC_HEADLESS
    };

    /**
     * Returns the compiled engine type.
     */
    static EngineKind GetEngineKind()
    {
#if defined(HAVE_TORTOISEBOTS)
        return ENGINE_TORTOISEBOTS;
#elif defined(HAVE_LEGACY_PLAYERBOTS)
        return ENGINE_LEGACY_PLAYERBOTS;
#else
        return ENGINE_GENERIC_HEADLESS;
#endif
    }

    /**
     * Returns a human-readable name of the active engine.
     */
    static char const* GetEngineName()
    {
#if defined(HAVE_TORTOISEBOTS)
        return "TortoiseBots (Native Modular)";
#elif defined(HAVE_LEGACY_PLAYERBOTS)
        return "Legacy PlayerBots (CMaNGOS Vendored)";
#else
        return "Generic Headless Session Mode (Native Core)";
#endif
    }

    /**
     * Returns true if the given player is any bot entity.
     */
    static bool IsBot(Player* player)
    {
        if (!player)
            return false;

        // Primary generic invariant: headless session transport
        if (player->GetSession() && player->GetSession()->IsHeadless())
            return true;

#if defined(HAVE_TORTOISEBOTS)
        return TortoiseBots::BotManager::Instance().IsBot(player->GetObjectGuid());
#elif defined(HAVE_LEGACY_PLAYERBOTS)
        return ::GetBotAI(player) != nullptr;
#else
        return false;
#endif
    }

    /**
     * Returns true if the bot belongs to the autonomous random pool.
     */
    static bool IsRandomBot(Player* player)
    {
        if (!player)
            return false;

#if defined(HAVE_TORTOISEBOTS)
        return TortoiseBots::BotManager::Instance().IsRandomBot(player->GetObjectGuid());
#elif defined(HAVE_LEGACY_PLAYERBOTS)
        return sRandomPlayerbotMgr.IsRandomBot(player);
#else
        return player->GetSession() && player->GetSession()->IsHeadless();
#endif
    }

    /**
     * Resolves the Player entity managed by the given PlayerbotAI instance.
     */
    static Player* GetAiPlayer(PlayerbotAI* ai)
    {
        if (!ai)
            return nullptr;

#if defined(HAVE_TORTOISEBOTS) || defined(HAVE_LEGACY_PLAYERBOTS)
        typedef Player* (PlayerbotAI::*AiGetBotMemFn)();
        AiGetBotMemFn pfn = &PlayerbotAI::GetBot;
        return (ai->*pfn)();
#else
        return nullptr;
#endif
    }

    /**
     * Checks if the bot is actively controlled or bound to a human master.
     */
    static bool BotHasRealPlayerMaster(Player* bot)
    {
        if (!bot)
            return false;

#if defined(HAVE_TORTOISEBOTS)
        PlayerbotAI* ai = PlayerbotAIStorage::Instance().GetAI(bot);
        if (ai && ai->HasRealPlayerMaster())
            return true;

        TortoiseBots::BotRecord* rec = TortoiseBots::BotManager::Instance().FindBot(bot->GetObjectGuid());
        if (rec && !rec->masterGuid.IsEmpty())
            return true;

        return false;
#elif defined(HAVE_LEGACY_PLAYERBOTS)
        PlayerbotAI* ai = ::GetBotAI(bot);
        return ai && ai->HasRealPlayerMaster();
#else
        if (Group* group = bot->GetGroup())
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (member && member->IsInWorld() && member->GetSession() && !member->GetSession()->IsHeadless())
                    return true;
            }
        }
        return false;
#endif
    }

    /**
     * Returns the minimum level configured for random bots.
     */
    static uint32 GetConfigMinLevel()
    {
#if defined(HAVE_TORTOISEBOTS)
        return sPlayerbotAIConfig.randombotStartingLevel > 0 ? sPlayerbotAIConfig.randombotStartingLevel : 1;
#elif defined(HAVE_LEGACY_PLAYERBOTS)
        return sPlayerbotAIConfig.randomBotMinLevel > 0 ? sPlayerbotAIConfig.randomBotMinLevel : 1;
#else
        return 1;
#endif
    }

    /**
     * Returns the maximum level configured for random bots.
     */
    static uint32 GetConfigMaxLevel()
    {
#if defined(HAVE_TORTOISEBOTS) || defined(HAVE_LEGACY_PLAYERBOTS)
        return sPlayerbotAIConfig.randomBotMaxLevel;
#else
        uint32 maxCfgLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
        return maxCfgLevel > 0 ? maxCfgLevel : 60;
#endif
    }

    /**
     * Formats class ID into human-readable name for logging.
     */
    static std::string GetClassName(uint8 classId)
    {
#if defined(HAVE_TORTOISEBOTS) || defined(HAVE_LEGACY_PLAYERBOTS)
        return ChatHelper::formatClass(classId);
#else
        switch (classId)
        {
            case CLASS_WARRIOR: return "Warrior";
            case CLASS_PALADIN: return "Paladin";
            case CLASS_HUNTER:  return "Hunter";
            case CLASS_ROGUE:   return "Rogue";
            case CLASS_PRIEST:  return "Priest";
            case CLASS_SHAMAN:  return "Shaman";
            case CLASS_MAGE:    return "Mage";
            case CLASS_WARLOCK: return "Warlock";
            case CLASS_DRUID:   return "Druid";
            default:            return "Unknown";
        }
#endif
    }

    /**
     * Safely dismounts and teleports a bot to a specified coordinate.
     * Used exclusively by restricted city relocation fallback.
     */
    static bool TeleportBot(Player* bot, uint32 mapId, float x, float y, float z, float o)
    {
        if (!bot || !bot->IsInWorld() || !bot->GetSession() || bot->GetSession()->isLogingOut())
            return false;

        if (bot->IsMounted())
            bot->Unmount();

        return bot->TeleportTo(mapId, x, y, z, o);
    }

    /**
     * Retrieves the PlayerbotAI instance for a bot player entity, if available.
     */
#if defined(HAVE_TORTOISEBOTS) || defined(HAVE_LEGACY_PLAYERBOTS)
    static PlayerbotAI* GetBotAI(Player* bot)
    {
        if (!bot)
            return nullptr;
#if defined(HAVE_TORTOISEBOTS)
        return PlayerbotAIStorage::Instance().GetAI(bot);
#elif defined(GET_PLAYERBOT_AI)
        return GET_PLAYERBOT_AI(bot);
#else
        return ::GetBotAI(bot);
#endif
    }

    static bool HasStrategy(Player* bot, const std::string& name)
    {
        if (!bot)
            return false;
        PlayerbotAI* ai = GetBotAI(bot);
        return ai && ai->HasStrategy(name, BotState::BOT_STATE_NON_COMBAT);
    }

#else
    static void* GetBotAI(Player* /*bot*/) { return nullptr; }
    static bool HasStrategy(Player* /*bot*/, const std::string& /*name*/) { return false; }
#endif
};

#endif // BOT_ENGINE_ADAPTER_H
