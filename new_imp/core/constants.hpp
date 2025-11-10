#pragma once

#include <cstdint>
#include <string>

/*
 * ==================== GLOBAL CONSTANTS ====================
 * C++11-compatible version (no inline variables, no string_view).
 */

// Forward declaration for PieceType
enum PieceType : std::uint8_t;

// ==================== SEARCH DEPTH CONSTANTS ====================
static const int MAX_DEPTH      = 2;
static const int PLY_DEPTH_ONE  = 1;
static const int PLY_DEPTH_TWO  = 2;

// ==================== EVALUATION / DEFENSE WEIGHTS ====================
static const float DEFENSE_BOT_DEFENSE  = 4.5f;
static const float OFFENSE_BOT_DEFENSE  = 0.1f;

// ==================== STRING CONSTANTS (defined in constants.cpp) ====================
extern const std::string PLAYER_CIRCLE;
extern const std::string PLAYER_SQUARE;

extern const std::string SIDE_STONE;
extern const std::string SIDE_RIVER;

extern const std::string ORIENT_HORIZONTAL;
extern const std::string ORIENT_VERTICAL;

// ==================== BASIC CHECKS ====================
inline bool is_CirclePlayer(const std::string& player) {
    return player == PLAYER_CIRCLE;
}
inline bool is_SquarePlayer(const std::string& player) {
    return player == PLAYER_SQUARE;
}
inline bool is_HorizontalOrientation(const std::string& orientation) {
    return orientation == ORIENT_HORIZONTAL;
}
inline bool is_VerticalOrientation(const std::string& orientation) {
    return orientation == ORIENT_VERTICAL;
}
