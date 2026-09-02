# CMake configuration hook for mod-twow-bot-level-brackets

if(TORTOISE_MODULE_CMAKE_PHASE STREQUAL "POST_TARGETS")
  find_package(Boost 1.70 REQUIRED)

  if(TORTOISE_CURRENT_MODULE_LINKAGE STREQUAL "dynamic")
    GetModuleProjectName("${TORTOISE_CURRENT_MODULE}" BLB_MODULE_TARGET)
  else()
    set(BLB_MODULE_TARGET modules)
  endif()

  target_include_directories(${BLB_MODULE_TARGET}
    PUBLIC
      ${CMAKE_SOURCE_DIR}/src/modules/PlayerBots
      ${CMAKE_SOURCE_DIR}/src/modules/PlayerBots/playerbot
      ${CMAKE_SOURCE_DIR}/src/modules/PlayerBots/playerbot/strategy
      ${CMAKE_SOURCE_DIR}/src/modules/PlayerBots/playerbot/strategy/actions
      ${CMAKE_SOURCE_DIR}/src/modules/PlayerBots/playerbot/strategy/triggers
      ${CMAKE_SOURCE_DIR}/src/modules/PlayerBots/playerbot/strategy/values
      ${CMAKE_SOURCE_DIR}/src/game/MapNodes
      ${Boost_INCLUDE_DIRS}
      ${CMAKE_CURRENT_LIST_DIR}/src
  )

  # These feature gates are PRIVATE on the vendored playerbots target, so a
  # consuming module must compile its own translation units with them too.
  target_compile_definitions(${BLB_MODULE_TARGET} PRIVATE
    CMANGOS
    MANGOSBOT_ZERO
    ENABLE_PLAYERBOTS)

  target_link_libraries(${BLB_MODULE_TARGET} PUBLIC playerbots)
  unset(BLB_MODULE_TARGET)
endif()
