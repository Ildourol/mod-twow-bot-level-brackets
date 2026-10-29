#ifndef MOD_TWOW_BOT_LEVEL_BRACKETS_COMMANDS_H
#define MOD_TWOW_BOT_LEVEL_BRACKETS_COMMANDS_H

#include <string>

class ChatHandler;

namespace BotLevelBrackets
{

class BotBracketsCommands
{
public:
    static bool HandleCommand(ChatHandler* handler, const char* subCommand, const char* args);

private:
    static bool HandleStatus(ChatHandler* handler);
    static bool HandleCities(ChatHandler* handler);
    static bool HandleWorld(ChatHandler* handler);
    static bool HandleBrackets(ChatHandler* handler);
    static bool HandleActivities(ChatHandler* handler);
    static bool HandleLedger(ChatHandler* handler);
    static bool HandleStats(ChatHandler* handler);
    static bool HandleExplain(ChatHandler* handler, const char* args);
    static bool HandleReload(ChatHandler* handler);
    static bool HandleForce(ChatHandler* handler);
    static bool HandleCleanup(ChatHandler* handler);
    static bool HandleDebug(ChatHandler* handler, const char* args);
};

} // namespace BotLevelBrackets

#endif // MOD_TWOW_BOT_LEVEL_BRACKETS_COMMANDS_H
