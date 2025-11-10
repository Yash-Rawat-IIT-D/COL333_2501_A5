#include "constants.hpp"
#include <cassert>

// ==================== STRING CONSTANT DEFINITIONS ====================

// Player identifiers
const std::string PLAYER_CIRCLE = "circle";
const std::string PLAYER_SQUARE = "square";

// Piece sides
const std::string SIDE_STONE = "stone";
const std::string SIDE_RIVER = "river";

// Orientation identifiers
const std::string ORIENT_HORIZONTAL = "horizontal";
const std::string ORIENT_VERTICAL   = "vertical";

// ==================== BOARD SIZE HELPERS ====================

BoardSize detectBoardSize(int rows, int cols) {
    if ((rows == 13 && cols == 12)) {
        return BOARD_SMALL;
    }
    if ((rows == 15 && cols == 14)) {
        return BOARD_MEDIUM;
    }
    if ((rows == 17 && cols == 16)) {
        return BOARD_LARGE;
    }

    assert(false); // Unexpected board size - Should not happen per spec

    return BOARD_UNKNOWN;
}

int getBoardWinCount(BoardSize size) {
    switch (size) {
        case BOARD_SMALL:  return 4;
        case BOARD_MEDIUM: return 5;
        case BOARD_LARGE:  return 6;
        default:           return 4; // safe fallback (keeps old behavior)
    }
}

// Alias - Same value as win count
int getScoreColsWidth(BoardSize size) {
    return getBoardWinCount(size); 
}

std::vector<int> makeScoreCols(BoardSize size, int cols) {
    const int w = getScoreColsWidth(size);
    const int start = (cols > w)
        ? (cols - w) / 2   // centered band
        : 0;               // degenerate fallback

    assert(start > 0);  // start = 0 should not happen per spec

    std::vector<int> score_cols;
    score_cols.reserve((std::size_t)w);
    for (int x = start; x < start + w && x < cols; ++x) {
        score_cols.push_back(x);
    }

    // If UNKNOWN and something weird, this still returns a centered band
    // compatible with previous behavior (w=4).
    return score_cols;
}