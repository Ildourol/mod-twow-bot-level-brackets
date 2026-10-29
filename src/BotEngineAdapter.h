#ifndef BOT_ENGINE_ADAPTER_H
#define BOT_ENGINE_ADAPTER_H

#include "compat/NativeTaskAdapter.h"

/**
 * Backward compatibility header forwarding to BotLevelBrackets::NativeTaskAdapter.
 */
class BotEngineAdapter
{
public:
    enum EngineKind
    {
        ENGINE_TORTOISEBOTS = 0,
        ENGINE_LEGACY_PLAYERBOTS = 1,
        ENGINE_GENERIC_HEADLESS = 2
    };

    static EngineKind GetEngineKind()
    {
        return static_cast<EngineKind>(BotLevelBrackets::NativeTaskAdapter::GetEngineKind());
    }

    static const char* GetEngineName()
    {
        return BotLevelBrackets::NativeTaskAdapter::GetEngineName();
    }

    static bool IsBot(Player* player)
    {
        return BotLevelBrackets::NativeTaskAdapter::IsBot(player);
    }

    static bool IsRandomBot(Player* player)
    {
        return BotLevelBrackets::NativeTaskAdapter::IsRandomBot(player);
    }

    static bool HasRealPlayerMaster(Player* player)
    {
        return BotLevelBrackets::NativeTaskAdapter::HasRealPlayerMaster(player);
    }

    static bool IsFreeBot(Player* player)
    {
        return BotLevelBrackets::NativeTaskAdapter::IsFreeBot(player);
    }
};

#endif // BOT_ENGINE_ADAPTER_H
