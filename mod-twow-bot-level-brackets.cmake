# CMake configuration hook for mod-twow-bot-level-brackets (Option 1 Architecture)

if(TORTOISE_MODULE_CMAKE_PHASE STREQUAL "POST_TARGETS")

  if(TORTOISE_CURRENT_MODULE_LINKAGE STREQUAL "dynamic")
    GetModuleProjectName("${TORTOISE_CURRENT_MODULE}" BLB_MODULE_TARGET)
  else()
    set(BLB_MODULE_TARGET modules)
  endif()

  # ---------------------------------------------------------------------------
  # 1. Check for TortoiseBots (Native Modular Companion Engine for tortoise-wow)
  # ---------------------------------------------------------------------------
  set(TORTOISEBOTS_FOUND FALSE)
  if(NOT DEFINED MODULE_TORTOISEBOTS OR NOT "${MODULE_TORTOISEBOTS}" STREQUAL "disabled")
    set(TORTOISEBOTS_SEARCH_PATHS
      "${CMAKE_SOURCE_DIR}/modules/TortoiseBots"
      "${CMAKE_SOURCE_DIR}/../TortoiseBots"
      "${CMAKE_CURRENT_LIST_DIR}/../TortoiseBots"
    )
    foreach(SEARCH_PATH ${TORTOISEBOTS_SEARCH_PATHS})
      if(EXISTS "${SEARCH_PATH}/runtime/BotManager.h")
        set(TORTOISEBOTS_ROOT "${SEARCH_PATH}")
        set(TORTOISEBOTS_FOUND TRUE)
        break()
      endif()
    endforeach()
  endif()

  # ---------------------------------------------------------------------------
  # 2. Check for Legacy PlayerBots (vendored in older cores)
  # ---------------------------------------------------------------------------
  set(LEGACY_PLAYERBOTS_FOUND FALSE)
  if(NOT DEFINED MODULE_PLAYERBOTS OR NOT "${MODULE_PLAYERBOTS}" STREQUAL "disabled")
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/modules/PlayerBots/playerbot/RandomPlayerbotMgr.h")
      set(LEGACY_PLAYERBOTS_ROOT "${CMAKE_SOURCE_DIR}/src/modules/PlayerBots")
      set(LEGACY_PLAYERBOTS_FOUND TRUE)
    endif()
  endif()

  # ---------------------------------------------------------------------------
  # 3. Configure includes and preprocessor definitions based on discovered engine
  # ---------------------------------------------------------------------------
  if(TORTOISEBOTS_FOUND)
    message(STATUS "mod-twow-bot-level-brackets: Discovered TortoiseBots at ${TORTOISEBOTS_ROOT}")
    target_compile_definitions(${BLB_MODULE_TARGET} PRIVATE
      HAVE_TORTOISEBOTS=1
      MANGOSBOT_ZERO=1
      CMANGOS=1
    )
    target_include_directories(${BLB_MODULE_TARGET}
      PUBLIC
        "${TORTOISEBOTS_ROOT}"
        "${TORTOISEBOTS_ROOT}/ai"
        "${TORTOISEBOTS_ROOT}/ai/playerbot"
        "${TORTOISEBOTS_ROOT}/ai/playerbot/strategy"
        "${TORTOISEBOTS_ROOT}/ai/playerbot/strategy/actions"
        "${TORTOISEBOTS_ROOT}/ai/playerbot/strategy/triggers"
        "${TORTOISEBOTS_ROOT}/ai/playerbot/strategy/values"
        "${TORTOISEBOTS_ROOT}/host"
        "${TORTOISEBOTS_ROOT}/runtime"
        "${CMAKE_SOURCE_DIR}/src/game/MapNodes"
        "${CMAKE_CURRENT_LIST_DIR}/src"
        "${CMAKE_CURRENT_LIST_DIR}/src/compat"
        "${CMAKE_CURRENT_LIST_DIR}/src/policy"
        "${CMAKE_CURRENT_LIST_DIR}/src/world"
        "${CMAKE_CURRENT_LIST_DIR}/src/scheduling"
        "${CMAKE_CURRENT_LIST_DIR}/src/activities"
        "${CMAKE_CURRENT_LIST_DIR}/src/social"
        "${CMAKE_CURRENT_LIST_DIR}/src/commands"
        "${CMAKE_CURRENT_LIST_DIR}/src/debug"
    )
  elseif(LEGACY_PLAYERBOTS_FOUND)
    message(STATUS "mod-twow-bot-level-brackets: Discovered Legacy PlayerBots at ${LEGACY_PLAYERBOTS_ROOT}")
    find_package(Boost 1.70 REQUIRED)
    target_compile_definitions(${BLB_MODULE_TARGET} PRIVATE
      HAVE_LEGACY_PLAYERBOTS=1
      CMANGOS=1
      MANGOSBOT_ZERO=1
      ENABLE_PLAYERBOTS=1
    )
    target_include_directories(${BLB_MODULE_TARGET}
      PUBLIC
        "${LEGACY_PLAYERBOTS_ROOT}"
        "${LEGACY_PLAYERBOTS_ROOT}/playerbot"
        "${LEGACY_PLAYERBOTS_ROOT}/playerbot/strategy"
        "${LEGACY_PLAYERBOTS_ROOT}/playerbot/strategy/actions"
        "${LEGACY_PLAYERBOTS_ROOT}/playerbot/strategy/triggers"
        "${LEGACY_PLAYERBOTS_ROOT}/playerbot/strategy/values"
        "${CMAKE_SOURCE_DIR}/src/game/MapNodes"
        ${Boost_INCLUDE_DIRS}
        "${CMAKE_CURRENT_LIST_DIR}/src"
        "${CMAKE_CURRENT_LIST_DIR}/src/compat"
        "${CMAKE_CURRENT_LIST_DIR}/src/policy"
        "${CMAKE_CURRENT_LIST_DIR}/src/world"
        "${CMAKE_CURRENT_LIST_DIR}/src/scheduling"
        "${CMAKE_CURRENT_LIST_DIR}/src/activities"
        "${CMAKE_CURRENT_LIST_DIR}/src/social"
        "${CMAKE_CURRENT_LIST_DIR}/src/commands"
        "${CMAKE_CURRENT_LIST_DIR}/src/debug"
    )
  else()
    message(STATUS "mod-twow-bot-level-brackets: No PlayerBots engine found; enabling Generic Headless Session Mode.")
    target_compile_definitions(${BLB_MODULE_TARGET} PRIVATE
      HAVE_GENERIC_HEADLESS_BOTS=1
    )
    target_include_directories(${BLB_MODULE_TARGET}
      PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/src"
        "${CMAKE_CURRENT_LIST_DIR}/src/compat"
        "${CMAKE_CURRENT_LIST_DIR}/src/policy"
        "${CMAKE_CURRENT_LIST_DIR}/src/world"
        "${CMAKE_CURRENT_LIST_DIR}/src/scheduling"
        "${CMAKE_CURRENT_LIST_DIR}/src/activities"
        "${CMAKE_CURRENT_LIST_DIR}/src/social"
        "${CMAKE_CURRENT_LIST_DIR}/src/commands"
        "${CMAKE_CURRENT_LIST_DIR}/src/debug"
    )
  endif()

  unset(BLB_MODULE_TARGET)
endif()
