# CMakeLists.txt for xr_coop module
# Cooperative multiplayer mod for XRay Engine 1.6

# xr_coop source files
set(XR_COOP_SOURCES
    coop_protocol.h
    coop_net_entity.h
    coop_net_entity.cpp
    coop_session_manager.h
    coop_session_manager.cpp
    coop_player_state.h
    coop_player_state.cpp
    coop_damage_handler.h
    coop_damage_handler.cpp
    coop_ai_sync.h
    coop_ai_sync.cpp
    coop_debug_overlay.h
    coop_debug_overlay.cpp
    game_sv_coop.h
    game_sv_coop.cpp
    game_cl_coop.h
    game_cl_coop.cpp
)

# Prefix paths with current directory
list(TRANSFORM XR_COOP_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

# Export to parent scope for inclusion in xrGame
set(XR_COOP_SOURCES ${XR_COOP_SOURCES} PARENT_SCOPE)
