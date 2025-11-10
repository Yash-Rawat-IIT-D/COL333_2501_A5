#pragma once

#include <vector>
#include <deque>
#include <unordered_map>
#include <random>
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <string>

#include "constants.hpp"
#include "piece.hpp"
#include "move.hpp"
#include "state.hpp"
#include "movegen.hpp"
#include "eval.hpp"

/*
 * ==================== SEARCH RESULT ====================
 */

struct SearchResult {
    float evaluation;
    Move bestMove;
    int depth_reached;
    bool timeout_occurred;
    
    SearchResult()
        : evaluation(0.0f),
          depth_reached(0),
          timeout_occurred(false) {}

    SearchResult(float eval, const Move& move, int depth = 0)
        : evaluation(eval),
          bestMove(move),
          depth_reached(depth),
          timeout_occurred(false) {}
};

/*
 * ==================== TRANSPOSITION TABLE ====================
 * (Same structure as original; TT use in MinimaxEngine remains commented out.)
 */

struct TTEntry {
    float evaluation;
    Move best_move;
    int depth;

    enum NodeType {
        EXACT = 0,
        LOWER_BOUND = 1,
        UPPER_BOUND = 2
    } node_type;
    
    TTEntry()
        : evaluation(0.0f),
          depth(-1),
          node_type(EXACT) {}

    TTEntry(float eval, const Move& move, int d, NodeType type)
        : evaluation(eval),
          best_move(move),
          depth(d),
          node_type(type) {}
};

class TranspositionTable {
private:
    static const std::size_t MAX_TABLE_SIZE = 500000;  // 500K entries max
    static const int MAX_BOARD_SIZE = 20;
    
    std::unordered_map<std::uint64_t, TTEntry> table;
    
    // Zobrist hash data
    static std::uint64_t piece_square_table[7][MAX_BOARD_SIZE][MAX_BOARD_SIZE];
    static std::uint64_t player_to_move_key;
    static bool zobrist_initialized;
    
    static void initializeZobrist();

public:
    TranspositionTable();
    
    void clear();
    void newSearch();
    
    std::uint64_t computeHash(const GameState& state, bool isCircleToMove) const;
    
    bool probe(std::uint64_t hash_key, int depth, float alpha, float beta,
               float& evaluation, Move& best_move) const;
    
    void store(std::uint64_t hash_key, float evaluation, const Move& best_move,
               int depth, float original_alpha, float beta);
    
    std::size_t size() const { return table.size(); }
};

/*
 * ==================== MINIMAX ENGINE ====================
 */

class MinimaxEngine {
private:
    BoardEvaluator* evaluator;
    MoveGenerator* moveGenerator;
    // TimeManager timeManager;
    // TranspositionTable tt;
    
    int nodes_searched;
    int max_depth_reached;

    std::deque<Move> recent_moves;
    static const int REPETITION_HISTORY = 3;

    // ========== Move history / repetition ==========
    bool isRepeatingMove(const Move& move) const;
    void addMoveToHistory(const Move& move);
    bool movesAreEqual(const Move& move1, const Move& move2) const;

    // ========== Root move ordering helper ==========
    struct MoveEvaluation {
        Move move;
        float quick_eval;
        
        MoveEvaluation(const Move& m, float eval)
            : move(m), quick_eval(eval) {}
        
        // Sort descending by quick_eval
        bool operator<(const MoveEvaluation& other) const {
            return quick_eval > other.quick_eval;
        }
    };
    
    std::vector<Move> selectTopRootMoves(GameState& position,
                                         const std::vector<Move>& allMoves,
                                         bool isCirclePlayer,
                                         int maxMoves = 32);

    // ========== Core search routines ==========
    SearchResult searchAtDepth(GameState& position, int depth,
                               bool isCirclePlayer,
                               const std::vector<Move>& rootMoves);

    float negamax(GameState& position, int depth, float alpha, float beta,
                  bool isCirclePlayer, int current_depth = 0);

    std::vector<Move> generateMovesForPosition(const GameState& position,
                                               const std::string& player);

public:
    MinimaxEngine(BoardEvaluator* eval, MoveGenerator* moveGen);
    
    Move getBestMove(GameState& position, const std::string& player,
                     int maxDepth = PLY_DEPTH_ONE);
    
    int getNodesSearched() const { return nodes_searched; }
    int getMaxDepthReached() const { return max_depth_reached; }
    
    // void clearTT() { tt.clear(); }
};
