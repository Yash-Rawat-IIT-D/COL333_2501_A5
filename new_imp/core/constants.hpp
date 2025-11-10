#pragma once

#include <cstdint>
#include <string>
#include <cstddef>
#include <vector>

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

// ==================== BOARD SIZE ====================

enum BoardSize {
    BOARD_SMALL,
    BOARD_MEDIUM,
    BOARD_LARGE,
    BOARD_UNKNOWN
};

// Detect board size from dimensions (per A5 spec: 13x12, 15x14, 17x16 or similar)
BoardSize detectBoardSize(int rows, int cols);

// Get win count for a given board size:
//  SMALL  -> 4 //  MEDIUM -> 5 //  LARGE  -> 6
int getBoardWinCount(BoardSize size);

// Get scoring band width (number of scoring columns) for a board size:
//  SMALL  -> 4 //  MEDIUM -> 5 //  LARGE  -> 6
int getScoreColsWidth(BoardSize size);

// Compute centered scoring columns for a given board size + column count.
// For UNKNOWN, this should fall back safely (e.g., 4-wide centered band).
std::vector<int> makeScoreCols(BoardSize size, int cols);
