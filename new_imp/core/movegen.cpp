#include "movegen.hpp"
#include <algorithm> // std::fill

// Static directions (4-neighborhood)
const std::vector<std::pair<int,int> > MoveGenerator::DIRECTIONS = {
    std::make_pair(1, 0),
    std::make_pair(-1, 0),
    std::make_pair(0, 1),
    std::make_pair(0, -1)
};

MoveGenerator::MoveGenerator() {
    move_buffer.reserve(200);
    bfs_queue.reserve(100);
    // visited_grid is resized lazily per board size
}

// ==================== RIVER FLOW ENGINE ====================

std::vector<std::pair<int,int> > MoveGenerator::computeRiverFlow(
    const GameState& state,
    int rx, int ry,
    int sx, int sy,
    bool isCirclePlayer,
    bool river_push
) const {
    int rows = state.getRows();
    int cols = state.getCols();

    // Reset BFS queue
    bfs_queue.clear();
    bfs_queue.push_back(std::make_pair(rx, ry));

    // Prepare visited grid
    if ((int)visited_grid.size() != rows) {
        visited_grid.assign(rows, std::vector<bool>(cols, false));
    } else {
        for (int y = 0; y < rows; ++y) {
            std::fill(visited_grid[y].begin(), visited_grid[y].end(), false);
        }
    }

    // Local static destinations grid (as in original: reused, cleared)
    static std::vector<std::vector<bool> > destinations_grid;
    if ((int)destinations_grid.size() != rows) {
        destinations_grid.assign(rows, std::vector<bool>(cols, false));
    } else {
        for (int y = 0; y < rows; ++y) {
            std::fill(destinations_grid[y].begin(), destinations_grid[y].end(), false);
        }
    }

    std::vector<std::pair<int,int> > unique_destinations;
    unique_destinations.reserve(64);

    // Original behavior: pop from front via erase(begin)
    while (!bfs_queue.empty()) {
        std::pair<int,int> front = bfs_queue.front();
        int x = front.first;
        int y = front.second;
        bfs_queue.erase(bfs_queue.begin());

        if (!state.inBounds(x, y)) continue;
        if (visited_grid[y][x]) continue;
        visited_grid[y][x] = true;

        std::uint8_t piece = state.getPiece(x, y);

        // River push: treat entry as source piece
        if (river_push && x == rx && y == ry) {
            piece = state.getPiece(sx, sy);
        }

        // Empty cell: possible destination (unless opponent scoring)
        if (piece == EMPTY) {
            if (!state.isOpponentScoreCell(x, y, isCirclePlayer)) {
                if (!destinations_grid[y][x]) {
                    destinations_grid[y][x] = true;
                    unique_destinations.push_back(std::make_pair(x, y));
                }
            }
            continue;
        }

        // Non-river blocks flow
        if (!isRiver(piece)) continue;

        // Determine flow directions
        std::vector<std::pair<int,int> > flow_dirs;
        if (isHorizontal(piece)) {
            flow_dirs.push_back(std::make_pair(1, 0));
            flow_dirs.push_back(std::make_pair(-1, 0));
        } else {
            flow_dirs.push_back(std::make_pair(0, 1));
            flow_dirs.push_back(std::make_pair(0, -1));
        }

        // Follow each direction
        for (std::size_t i = 0; i < flow_dirs.size(); ++i) {
            int dx = flow_dirs[i].first;
            int dy = flow_dirs[i].second;

            int nx = x + dx;
            int ny = y + dy;

            while (state.inBounds(nx, ny)) {
                if (state.isOpponentScoreCell(nx, ny, isCirclePlayer)) break;

                std::uint8_t next_piece = state.getPiece(nx, ny);

                if (next_piece == EMPTY) {
                    if (!destinations_grid[ny][nx]) {
                        destinations_grid[ny][nx] = true;
                        unique_destinations.push_back(std::make_pair(nx, ny));
                    }
                    nx += dx;
                    ny += dy;
                    continue;
                }

                // Skip the moving source piece
                if (nx == sx && ny == sy) {
                    nx += dx;
                    ny += dy;
                    continue;
                }

                // Chain into another river
                if (::isRiver(next_piece)) {
                    bfs_queue.push_back(std::make_pair(nx, ny));
                    break;
                }

                // Solid block
                break;
            }
        }
    }

    return unique_destinations;
}

// ==================== MOVE GENERATION (ALL PIECES) ====================

std::vector<Move> MoveGenerator::generateAllMovesOptimized(
    const GameState& state,
    const std::string& player
) {
    move_buffer.clear();

    bool isCircle = is_CirclePlayer(player);
    const std::vector<std::pair<int,int> >& player_positions =
        isCircle ? state.getCirclePiecePositions()
                 : state.getSquarePiecePositions();

    int rows = state.getRows();
    int cols = state.getCols();
    const std::vector<int>& score_cols = state.getScoreCols();

    for (std::size_t i = 0; i < player_positions.size(); ++i) {
        int x = player_positions[i].first;
        int y = player_positions[i].second;
        generateMovesForPiece(state, x, y, player, rows, cols, score_cols);
    }

    return move_buffer;
}

// ==================== VALID TARGETS (SINGLE PIECE) ====================

MoveGenerator::ValidTargets MoveGenerator::computeValidTargets(
    const GameState& state,
    int sx, int sy,
    const std::string& player,
    int rows, int cols,
    const std::vector<int>& score_cols
) const {
    (void)rows;
    (void)cols;
    (void)score_cols;

    ValidTargets targets;
    if (!state.inBounds(sx, sy)) return targets;

    std::uint8_t piece = state.getPiece(sx, sy);
    if (piece == EMPTY) return targets;

    bool isCirclePlayerFlag = is_CirclePlayer(player);
    if (!isPieceOwnerFast(piece, isCirclePlayerFlag)) return targets;

    // Check all four directions
    for (std::size_t i = 0; i < DIRECTIONS.size(); ++i) {
        int dx = DIRECTIONS[i].first;
        int dy = DIRECTIONS[i].second;

        int tx = sx + dx;
        int ty = sy + dy;
        if (!state.inBounds(tx, ty)) continue;

        // Block movement into opponent scoring cells
        if (state.isOpponentScoreCell(tx, ty, isCirclePlayerFlag)) continue;

        std::uint8_t target_piece = state.getPiece(tx, ty);

        if (target_piece == EMPTY) {
            // Simple move
            targets.moves.insert(std::make_pair(tx, ty));
        }
        else if (::isRiver(target_piece)) {
            // Flow moves via river
            std::vector<std::pair<int,int> > flow_destinations =
                computeRiverFlow(state, tx, ty, sx, sy, isCirclePlayerFlag, false);
            for (std::size_t j = 0; j < flow_destinations.size(); ++j) {
                targets.moves.insert(flow_destinations[j]);
            }
        }
        else {
            // Stone: possible push
            if (::isStone(piece)) {
                // Stone pushing stone
                int px = tx + dx;
                int py = ty + dy;
                if (state.inBounds(px, py) &&
                    state.getPiece(px, py) == EMPTY &&
                    !state.isOpponentScoreCell(px, py, isCirclePlayerFlag)) {
                    targets.pushes.push_back(
                        std::make_pair(
                            std::make_pair(tx, ty),
                            std::make_pair(px, py)
                        )
                    );
                }
            } else {
                // River pushing stone (river-push logic)
                bool pushedIsCircle = ::isCircle(target_piece);
                std::vector<std::pair<int,int> > flow_destinations =
                    computeRiverFlow(state, tx, ty, sx, sy, pushedIsCircle, true);

                for (std::size_t j = 0; j < flow_destinations.size(); ++j) {
                    const std::pair<int,int>& dest = flow_destinations[j];
                    if (!state.isOpponentScoreCell(dest.first, dest.second, pushedIsCircle)) {
                        targets.pushes.push_back(
                            std::make_pair(
                                std::make_pair(tx, ty),
                                dest
                            )
                        );
                    }
                }
            }
        }
    }

    return targets;
}

// ==================== GENERATE MOVES FOR SINGLE PIECE ====================

void MoveGenerator::generateMovesForPiece(
    const GameState& state,
    int x, int y,
    const std::string& player,
    int rows, int cols,
    const std::vector<int>& score_cols
) {
    (void)rows;
    (void)cols;
    (void)score_cols;

    std::uint8_t piece = state.getPiece(x, y);

    // Generate movement/push moves using valid targets
    ValidTargets targets = computeValidTargets(
        state, x, y, player,
        state.getRows(), state.getCols(), state.getScoreCols()
    );

    // ----- Movement moves -----
    for (std::set<std::pair<int,int> >::const_iterator it = targets.moves.begin();
         it != targets.moves.end(); ++it) {
        const std::pair<int,int>& move_pos = *it;
        move_buffer.emplace_back(
            "move",
            std::vector<int>{x, y},
            std::vector<int>{move_pos.first, move_pos.second}
        );
    }

    // ----- Push moves -----
    for (std::size_t i = 0; i < targets.pushes.size(); ++i) {
        const std::pair<std::pair<int,int>, std::pair<int,int> >& push_pair = targets.pushes[i];
        const std::pair<int,int>& own_final = push_pair.first;
        const std::pair<int,int>& pushed_to = push_pair.second;

        move_buffer.emplace_back(
            "push",
            std::vector<int>{x, y},
            std::vector<int>{own_final.first, own_final.second},
            std::vector<int>{pushed_to.first, pushed_to.second}
        );
    }

    // ----- Flip moves -----
    if (::isStone(piece)) {
        // Stone -> River (try both orientations)
        move_buffer.emplace_back(
            "flip",
            std::vector<int>{x, y},
            std::vector<int>{x, y},
            std::vector<int>{},
            ORIENT_HORIZONTAL
        );
        move_buffer.emplace_back(
            "flip",
            std::vector<int>{x, y},
            std::vector<int>{x, y},
            std::vector<int>{},
            ORIENT_VERTICAL
        );
    } else if (::isRiver(piece)) {
        // River -> Stone
        move_buffer.emplace_back(
            "flip",
            std::vector<int>{x, y},
            std::vector<int>{x, y},
            std::vector<int>{},
            std::string()
        );
    }

    // ----- Rotate moves (only for rivers) -----
    if (::isRiver(piece)) {
        move_buffer.emplace_back(
            "rotate",
            std::vector<int>{x, y},
            std::vector<int>{x, y},
            std::vector<int>{},
            std::string()
        );
    }
}
