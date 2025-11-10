#pragma once

#include <vector>
#include <set>
#include <utility>
#include <cstdint>
#include <string>

#include "constants.hpp"
#include "piece.hpp"
#include "move.hpp"
#include "state.hpp"

/*
 * ==================== MOVE GENERATION ENGINE ====================
 * Behavior-preserving split of the original MoveGenerator.
 */

class MoveGenerator {
private:
    // Pre-allocated containers for performance
    mutable std::vector<Move> move_buffer;
    mutable std::vector<std::pair<int,int> > bfs_queue;
    mutable std::vector<std::vector<bool> > visited_grid;

    // Direction constants (defined in movegen.cpp)
    static const std::vector<std::pair<int,int> > DIRECTIONS;

    inline bool inBounds(int x, int y, int rows, int cols) const {
        return x >= 0 && x < cols && y >= 0 && y < rows;
    }

public:
    MoveGenerator();

    // ==================== PHASE 2: RIVER FLOW ENGINE ====================

    // BFS-based river flow computation (mirrors original logic)
    std::vector<std::pair<int,int> > computeRiverFlow(
        const GameState& state,
        int rx, int ry,
        int sx, int sy,
        bool isCirclePlayer,
        bool river_push = false
    ) const;

    // ==================== PHASE 3: MOVE GENERATION / ORDERING ====================

    // Compute valid targets for a piece (moves + pushes)
    struct ValidTargets {
        std::set<std::pair<int,int> > moves;
        std::vector<std::pair<std::pair<int,int>, std::pair<int,int> > > pushes;
        // ((own_final), (pushed_to))
    };

    // Generate all moves for current player using tracked positions
    std::vector<Move> generateAllMovesOptimized(
        const GameState& state,
        const std::string& player
    );

    // Compute valid targets for a single piece (mirrors original compute_valid_targets)
    ValidTargets computeValidTargets(
        const GameState& state,
        int sx, int sy,
        const std::string& player,
        int rows, int cols,
        const std::vector<int>& score_cols
    ) const;

    // Generate all moves for a single piece (mirrors original implementation)
    void generateMovesForPiece(
        const GameState& state,
        int x, int y,
        const std::string& player,
        int rows, int cols,
        const std::vector<int>& score_cols
    );
};
