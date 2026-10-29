#ifndef BRACKETS_COMMON_H
#define BRACKETS_COMMON_H

#include "Common.h"
#include <string>
#include <vector>
#include <cstdint>

namespace Brackets
{
    enum Domain : uint8
    {
        DOMAIN_NONE = 0,
        DOMAIN_CITY = 1,
        DOMAIN_WORLD = 2
    };

    inline const char* DomainToString(Domain d)
    {
        switch (d)
        {
            case DOMAIN_CITY:  return "City";
            case DOMAIN_WORLD: return "World";
            default:           return "None";
        }
    }

    enum IntentState : uint8
    {
        INTENT_STATE_NONE = 0,
        INTENT_STATE_PROPOSED = 1,
        INTENT_STATE_PLANNING = 2,
        INTENT_STATE_TRAVELING = 3,
        INTENT_STATE_ARRIVED = 4,
        INTENT_STATE_ACTIVITY_OBSERVED = 5,
        INTENT_STATE_RELEASED = 6,
        INTENT_STATE_DEFERRED = 7,
        INTENT_STATE_REJECTED = 8,
        INTENT_STATE_FAILED = 9,
        INTENT_STATE_EXPIRED = 10,
        INTENT_STATE_CANCELLED = 11
    };

    inline const char* IntentStateToString(IntentState s)
    {
        switch (s)
        {
            case INTENT_STATE_PROPOSED:          return "Proposed";
            case INTENT_STATE_PLANNING:          return "Planning";
            case INTENT_STATE_TRAVELING:         return "Traveling";
            case INTENT_STATE_ARRIVED:           return "Arrived";
            case INTENT_STATE_ACTIVITY_OBSERVED: return "ActivityObserved";
            case INTENT_STATE_RELEASED:          return "Released";
            case INTENT_STATE_DEFERRED:          return "Deferred";
            case INTENT_STATE_REJECTED:          return "Rejected";
            case INTENT_STATE_FAILED:            return "Failed";
            case INTENT_STATE_EXPIRED:           return "Expired";
            case INTENT_STATE_CANCELLED:         return "Cancelled";
            default:                             return "None";
        }
    }

    inline bool IsTerminalState(IntentState s)
    {
        return s >= INTENT_STATE_RELEASED;
    }

    enum TerminalReason : uint8
    {
        REASON_NONE = 0,
        REASON_COMPLETED,
        REASON_BUSY_NATIVE_WORK,
        REASON_PROTECTED_FRIEND,
        REASON_PROTECTED_GUILD,
        REASON_PROTECTED_NAME,
        REASON_MASTER_CLAIMED,
        REASON_IN_GROUP,
        REASON_IN_INSTANCE,
        REASON_IN_QUEUE,
        REASON_IN_COMBAT,
        REASON_DEAD,
        REASON_SEARCH_EMPTY,
        REASON_ROUTE_UNKNOWN,
        REASON_ROUTE_UNSAFE,
        REASON_ROUTE_UNSUPPORTED_BEFORE_START,
        REASON_DEADLINE_EXPIRED,
        REASON_NATIVE_PREEMPTED,
        REASON_TARGET_REPLACED,
        REASON_LANDING_UNAVAILABLE,
        REASON_FALLBACK_DISABLED,
        REASON_RATE_LIMITED,
        REASON_CONFIG_RELOAD,
        REASON_MODULE_DISABLED,
        REASON_LEVEL_OUTGROWN,
        REASON_STALLED,
        REASON_FAILED,
        REASON_EARLY_DEPARTURE
    };

    inline const char* TerminalReasonToString(TerminalReason r)
    {
        switch (r)
        {
            case REASON_COMPLETED:                     return "Completed";
            case REASON_BUSY_NATIVE_WORK:              return "BusyNativeWork";
            case REASON_PROTECTED_FRIEND:              return "ProtectedFriend";
            case REASON_PROTECTED_GUILD:               return "ProtectedGuild";
            case REASON_PROTECTED_NAME:                return "ProtectedName";
            case REASON_MASTER_CLAIMED:                return "MasterClaimed";
            case REASON_IN_GROUP:                      return "InGroup";
            case REASON_IN_INSTANCE:                   return "InInstance";
            case REASON_IN_QUEUE:                      return "InQueue";
            case REASON_IN_COMBAT:                     return "InCombat";
            case REASON_DEAD:                          return "Dead";
            case REASON_SEARCH_EMPTY:                  return "SearchEmpty";
            case REASON_ROUTE_UNKNOWN:                 return "RouteUnknown";
            case REASON_ROUTE_UNSAFE:                  return "RouteUnsafe";
            case REASON_ROUTE_UNSUPPORTED_BEFORE_START:return "NativeRouteUnsupportedBeforeStart";
            case REASON_DEADLINE_EXPIRED:              return "DeadlineExpired";
            case REASON_NATIVE_PREEMPTED:              return "NativePreempted";
            case REASON_TARGET_REPLACED:               return "TargetReplaced";
            case REASON_LANDING_UNAVAILABLE:           return "LandingUnavailable";
            case REASON_FALLBACK_DISABLED:             return "FallbackDisabled";
            case REASON_RATE_LIMITED:                  return "RateLimited";
            case REASON_CONFIG_RELOAD:                 return "ConfigReload";
            case REASON_MODULE_DISABLED:               return "ModuleDisabled";
            case REASON_LEVEL_OUTGROWN:                return "LevelOutgrown";
            case REASON_STALLED:                       return "Stalled";
            case REASON_FAILED:                        return "Failed";
            case REASON_EARLY_DEPARTURE:               return "EarlyDeparture";
            default:                                   return "None";
        }
    }

    enum CityId : uint8
    {
        CITY_STORMWIND = 0,
        CITY_IRONFORGE = 1,
        CITY_DARNASSUS = 2,
        CITY_ALAH_THALAS = 3,
        CITY_ORGRIMMAR = 4,
        CITY_UNDERCITY = 5,
        CITY_THUNDER_BLUFF = 6,
        CITY_SPARKWATER = 7,
        CITY_COUNT = 8
    };

    enum DebugMode : uint8
    {
        DEBUG_MODE_OFF = 0,
        DEBUG_MODE_SUMMARY = 1,
        DEBUG_MODE_TRACE = 2,
        DEBUG_MODE_DRY_RUN = 3
    };

    enum LevelPolicyMode : uint8
    {
        LEVEL_POLICY_OBSERVE_NATIVE = 0,
        LEVEL_POLICY_ADVISORY = 1
    };
}

#endif // BRACKETS_COMMON_H
