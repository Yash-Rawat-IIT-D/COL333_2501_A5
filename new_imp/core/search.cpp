#include "search.hpp"

// ==================== TRANSPOSITION TABLE ====================

std::uint64_t TranspositionTable::piece_square_table[7][TranspositionTable::MAX_BOARD_SIZE][TranspositionTable::MAX_BOARD_SIZE];
std::uint64_t TranspositionTable::player_to_move_key = 0;
bool TranspositionTable::zobrist_initialized = false;

void TranspositionTable::initializeZobrist() {
    if (zobrist_initialized) return;
    
    std::mt19937_64 rng(42);  // Fixed seed for reproducibility
    std::uniform_int_distribution<std::uint64_t> dist;
    
    for (int piece = 0; piece < 7; ++piece) {
        for (int y = 0; y < MAX_BOARD_SIZE; ++y) {
            for (int x = 0; x < MAX_BOARD_SIZE; ++x) {
                piece_square_table[piece][y][x] = dist(rng);
            }
        }
    }
    
    player_to_move_key = dist(rng);
    zobrist_initialized = true;
}

TranspositionTable::TranspositionTable() {
    initializeZobrist();
    table.reserve(MAX_TABLE_SIZE);
}

void TranspositionTable::clear() {
    table.clear();
}

void TranspositionTable::newSearch() {
    if (table.size() > MAX_TABLE_SIZE) {
        table.clear();
    }
}

std::uint64_t TranspositionTable::computeHash(const GameState& state,
                                              bool isCircleToMove) const {
    std::uint64_t hash = 0;
    
    for (int y = 0; y < state.getRows(); ++y) {
        for (int x = 0; x < state.getCols(); ++x) {
            std::uint8_t piece = state.getPiece(x, y);
            if (piece != EMPTY &&
                y < MAX_BOARD_SIZE && x < MAX_BOARD_SIZE) {
                hash ^= piece_square_table[piece][y][x];
            }
        }
    }
    
    if (isCircleToMove) {
        hash ^= player_to_move_key;
    }
    
    return hash;
}

bool TranspositionTable::probe(std::uint64_t hash_key, int depth,
                               float alpha, float beta,
                               float& evaluation, Move& best_move) const {
    std::unordered_map<std::uint64_t, TTEntry>::const_iterator it =
        table.find(hash_key);
    if (it == table.end() || it->second.depth < depth) {
        return false;
    }
    
    const TTEntry& entry = it->second;
    best_move = entry.best_move;
    
    switch (entry.node_type) {
        case TTEntry::EXACT:
            evaluation = entry.evaluation;
            return true;
            
        case TTEntry::LOWER_BOUND:
            if (entry.evaluation >= beta) {
                evaluation = entry.evaluation;
                return true;
            }
            break;
            
        case TTEntry::UPPER_BOUND:
            if (entry.evaluation <= alpha) {
                evaluation = entry.evaluation;
                return true;
            }
            break;
    }
    
    return false;
}

void TranspositionTable::store(std::uint64_t hash_key, float evaluation,
                               const Move& best_move, int depth,
                               float original_alpha, float beta) {
    TTEntry::NodeType node_type;
    if (evaluation <= original_alpha) {
        node_type = TTEntry::UPPER_BOUND;
    } else if (evaluation >= beta) {
        node_type = TTEntry::LOWER_BOUND;
    } else {
        node_type = TTEntry::EXACT;
    }
    
    std::unordered_map<std::uint64_t, TTEntry>::iterator it =
        table.find(hash_key);
    if (it == table.end() || it->second.depth <= depth) {
        table[hash_key] = TTEntry(evaluation, best_move, depth, node_type);
    }
}

// ==================== MINIMAX ENGINE ====================

MinimaxEngine::MinimaxEngine(BoardEvaluator* eval, MoveGenerator* moveGen)
    : evaluator(eval),
      moveGenerator(moveGen),
      nodes_searched(0),
      max_depth_reached(0) {
    // pv_moves disabled (commented in original)
}

Move MinimaxEngine::getBestMove(GameState& position,
                                const std::string& player,
                                int maxDepth) {
    // tt.newSearch(); // disabled as in original

    nodes_searched = 0;
    max_depth_reached = 0;

    bool isCirclePlayer = is_CirclePlayer(player);

    // Generate all legal root moves
    std::vector<Move> allRootMoves =
        generateMovesForPosition(position, player);

    // Select top candidates
    std::vector<Move> rootMoves =
        selectTopRootMoves(position, allRootMoves, isCirclePlayer, 48);

    if (rootMoves.empty()) {
        // No moves: return a default move
        return Move("move", std::vector<int>{0, 0}, std::vector<int>{0, 0});
    }

    if (rootMoves.size() == 1) {
        Move singleMove = rootMoves[0];
        addMoveToHistory(singleMove);
        return singleMove;
    }

    SearchResult bestResult;
    bestResult.bestMove = rootMoves[0];

    for (int depth = 1; depth <= maxDepth; ++depth) {
        SearchResult currentResult =
            searchAtDepth(position, depth, isCirclePlayer, rootMoves);

        if (!currentResult.timeout_occurred) {
            bestResult = currentResult;
            max_depth_reached = depth;
        }

        if (bestResult.evaluation > 1000000.0f) {
            break;
        }
    }

    Move finalMove = bestResult.bestMove;

    // Repetition avoidance
    if (isRepeatingMove(finalMove) && rootMoves.size() > 1) {
        for (std::size_t i = 0; i < rootMoves.size(); ++i) {
            const Move& alternative = rootMoves[i];
            if (!movesAreEqual(alternative, finalMove) &&
                !isRepeatingMove(alternative)) {
                finalMove = alternative;
                break;
            }
        }
    }

    addMoveToHistory(finalMove);
    return finalMove;
}

// ========== Move history / repetition ==========

bool MinimaxEngine::isRepeatingMove(const Move& move) const {
    if (recent_moves.size() < 2) return false;

    bool allSame = true;
    for (std::size_t i = 0; i < recent_moves.size(); ++i) {
        if (!movesAreEqual(move, recent_moves[i])) {
            allSame = false;
            break;
        }
    }
    return allSame;
}

void MinimaxEngine::addMoveToHistory(const Move& move) {
    recent_moves.push_back(move);
    while (recent_moves.size() > (std::size_t)REPETITION_HISTORY) {
        recent_moves.pop_front();
    }
}

bool MinimaxEngine::movesAreEqual(const Move& move1,
                                  const Move& move2) const {
    return move1.action == move2.action &&
           move1.from == move2.from &&
           move1.to == move2.to &&
           move1.pushed_to == move2.pushed_to &&
           move1.orientation == move2.orientation;
}

// ========== Root move ordering helper ==========

std::vector<Move> MinimaxEngine::selectTopRootMoves(
    GameState& position,
    const std::vector<Move>& allMoves,
    bool isCirclePlayer,
    int maxMoves
) {
    if ((int)allMoves.size() <= maxMoves) {
        return allMoves;
    }

    std::vector<MoveEvaluation> evaluatedMoves;
    evaluatedMoves.reserve(allMoves.size());

    for (std::size_t i = 0; i < allMoves.size(); ++i) {
        const Move& move = allMoves[i];

        GameState::UndoInfo undo_info = position.applyMove(move);

        float eval = evaluator->EvaluateBoard(position, isCirclePlayer);
        evaluatedMoves.push_back(MoveEvaluation(move, eval));

        position.undoMove(undo_info);
    }

    std::sort(evaluatedMoves.begin(), evaluatedMoves.end());

    std::vector<Move> topMoves;
    topMoves.reserve(maxMoves);

    int limit = std::min(maxMoves,
                         (int)evaluatedMoves.size());
    for (int i = 0; i < limit; ++i) {
        topMoves.push_back(evaluatedMoves[i].move);
    }

    return topMoves;
}

// ========== searchAtDepth ==========

SearchResult MinimaxEngine::searchAtDepth(
    GameState& position,
    int depth,
    bool isCirclePlayer,
    const std::vector<Move>& rootMoves
) {
    SearchResult bestResult;
    bestResult.evaluation = -10000000.0f;
    bestResult.bestMove = rootMoves[0];
    bestResult.depth_reached = depth;

    float alpha = -10000000.0f;
    float beta  = 10000000.0f;

    std::vector<Move> orderedMoves = rootMoves; // PV/TT ordering disabled

    for (std::size_t i = 0; i < orderedMoves.size(); ++i) {
        const Move& move = orderedMoves[i];

        GameState::UndoInfo undo_info = position.applyMove(move);

        float evaluation =
            -negamax(position, depth - 1,
                     -beta, -alpha,
                     !isCirclePlayer, 1);

        position.undoMove(undo_info);

        if (evaluation > bestResult.evaluation) {
            bestResult.evaluation = evaluation;
            bestResult.bestMove = move;
        }

        if (evaluation > alpha) {
            alpha = evaluation;
        }

        if (beta <= alpha) {
            break; // cutoff
        }
    }

    return bestResult;
}

// ========== negamax ==========

float MinimaxEngine::negamax(
    GameState& position,
    int depth,
    float alpha,
    float beta,
    bool isCirclePlayer,
    int current_depth
) {
    (void)current_depth; // PV/TT hooks are commented out
    nodes_searched++;

    // uint64_t position_hash = tt.computeHash(position, isCirclePlayer);
    // TT probe disabled

    if (depth == 0) {
        float evaluation = evaluator->EvaluateBoard(position, isCirclePlayer);
        // tt.store(...); // disabled
        return evaluation;
    }

    std::string currentPlayer =
        isCirclePlayer ? PLAYER_CIRCLE : PLAYER_SQUARE;

    std::vector<Move> moves =
        generateMovesForPosition(position, currentPlayer);

    if (moves.empty()) {
        float evaluation = -50.0f;
        // tt.store(...); // disabled
        return evaluation;
    }

    std::vector<Move> orderedMoves = moves; // no PV/TT ordering

    float maxEval = -10000000.0f;
    Move best_move = orderedMoves[0];
    float original_alpha = alpha;

    for (std::size_t i = 0; i < orderedMoves.size(); ++i) {
        const Move& move = orderedMoves[i];

        GameState::UndoInfo undo_info = position.applyMove(move);

        float evaluation =
            -negamax(position, depth - 1,
                     -beta, -alpha,
                     !isCirclePlayer,
                     current_depth + 1);

        position.undoMove(undo_info);

        if (evaluation > maxEval) {
            maxEval = evaluation;
            best_move = move;
        }

        if (evaluation > alpha) {
            alpha = evaluation;
        }

        if (beta <= alpha) {
            break;
        }
    }

    // tt.store(position_hash, maxEval, best_move, depth, original_alpha, beta); // disabled

    return maxEval;
}

// ========== Move generation wrapper ==========

std::vector<Move> MinimaxEngine::generateMovesForPosition(
    const GameState& position,
    const std::string& player
) {
    return moveGenerator->generateAllMovesOptimized(position, player);
}
