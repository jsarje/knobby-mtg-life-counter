// session_state.cpp
// Owns the singleton runtime state objects for the application.
// All state is declared here so there is exactly one writable owner per value.

#include "session_state.h"

// Global definitions – constructed before setup() runs.
SettingsState        g_settings_state;
MultiplayerGameState g_multiplayer_game_state;
