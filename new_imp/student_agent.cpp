// student_agent.cpp

#include <random>
#include <vector>
#include <map>
#include <string>
#include <exception>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "core/constants.hpp"
#include "core/piece.hpp"
#include "core/move.hpp"
#include "core/state.hpp"
#include "core/movegen.hpp"
#include "core/eval.hpp"
#include "core/timer.hpp"
#include "core/search.hpp"

namespace py = pybind11;

// ==================== STUDENT AGENT ENGINE ====================

class StudentAgent {
private:
    std::string side;            // "circle" or "square"
    std::random_device rd;
    std::mt19937 gen;

    MoveGenerator moveGen;       // Move generation engine
    BoardEvaluator evaluator;    // Evaluation engine
    MinimaxEngine* searchEngine; // Minimax search engine
    GameState gameState;         // Current game state

    std::vector<Move> filterToSafeMoves(const std::vector<Move>& allMoves,
                                        const GameState& gameState) {
        std::vector<Move> safeMoves;
        const std::vector<int>& score_cols = gameState.getScoreCols();

        bool isCirclePlayer = (side == PLAYER_CIRCLE);
        int target_scoring_row = isCirclePlayer ? 2 : (gameState.getRows() - 3);

        for (std::size_t i = 0; i < allMoves.size(); ++i) {
            const Move& move = allMoves[i];
            bool isSafe = true;

            if (move.action == "move" && move.from.size() >= 2) {
                int from_x = move.from[0];
                int from_y = move.from[1];

                if (from_y == target_scoring_row) {
                    for (std::size_t j = 0; j < score_cols.size(); ++j) {
                        int score_col = score_cols[j];
                        if (from_x == score_col) {
                            isSafe = false;
                            break;
                        }
                    }
                }
            }

            if (isSafe) {
                safeMoves.push_back(move);
            }
        }

        // If no safe moves found, fall back to all moves
        return safeMoves.empty() ? allMoves : safeMoves;
    }

    Move selectBestMoveByEvaluation(const std::vector<Move>& moves) {
        if (moves.empty()) {
            return Move("move", std::vector<int>{0, 0}, std::vector<int>{0, 0});
        }

        if (moves.size() == 1) {
            return moves[0];
        }

        bool isCirclePlayer = (side == PLAYER_CIRCLE);
        Move bestMove = moves[0];
        float bestScore = -1000000.0f;

        for (std::size_t i = 0; i < moves.size(); ++i) {
            const Move& move = moves[i];

            GameState::UndoInfo undo_info = gameState.applyMove(move);

            float score = evaluator.EvaluateBoard(gameState, isCirclePlayer);

            gameState.undoMove(undo_info);

            if (score > bestScore) {
                bestScore = score;
                bestMove = move;
            }
        }

        return bestMove;
    }

    Move selectRandomMove(const std::vector<Move>& moves) {
        if (moves.empty()) {
            return Move("move", std::vector<int>{0, 0}, std::vector<int>{0, 0});
        }
        std::uniform_int_distribution<> dis(0, (int)moves.size() - 1);
        return moves[(std::size_t)dis(gen)];
    }

public:
    explicit StudentAgent(std::string side_in)
        : side(std::move(side_in)),
          rd(),
          gen(rd()),
          moveGen(),
          evaluator(&moveGen),
          searchEngine(0),
          gameState(13, 12) {

        searchEngine = new MinimaxEngine(&evaluator, &moveGen);
        evaluator.setMoveGenerator(&moveGen);

        bool isCirclePlayer = (side == PLAYER_CIRCLE);
        bool defense_mode = !isCirclePlayer;
        evaluator.setDefenseMode(defense_mode);
    }

    ~StudentAgent() {
        delete searchEngine;
    }

    Move choose(
        const std::vector<std::vector<std::map<std::string, std::string> > >& board,
        int row,
        int col,
        const std::vector<int>& score_cols,
        float current_player_time,
        float opponent_time
    ) {
        (void)row;
        (void)col;
        (void)score_cols;
        (void)opponent_time;

        int rows = (int)board.size();
        int cols = rows > 0 ? (int)board[0].size() : 0;

        gameState = GameState(rows, cols);
        gameState.loadFromPython(board);

        TimeManager timeManager;
        timeManager.startTimer(static_cast<double>(current_player_time));

        TimeMode timeMode = timeManager.getTimeMode();

        const std::vector<Move> allMoves =
            moveGen.generateAllMovesOptimized(gameState, side);

        std::vector<Move> safeMoves = filterToSafeMoves(allMoves, gameState);

        try {
            if (!allMoves.empty()) {
                Move bestMove;

                if (timeMode == TimeMode::PANIC) {
                    bestMove = selectRandomMove(safeMoves);
                } else if (timeMode == TimeMode::EVAL) {
                    bestMove = selectBestMoveByEvaluation(safeMoves);
                } else if (timeMode == TimeMode::PLY_ONE) {
                    bestMove = searchEngine->getBestMove(gameState, side, PLY_DEPTH_ONE);
                } else { // TimeMode::PLY_TWO
                    bestMove = searchEngine->getBestMove(gameState, side, PLY_DEPTH_TWO);
                }

                // Ensure bestMove is a legal move from allMoves
                for (std::size_t i = 0; i < allMoves.size(); ++i) {
                    if (allMoves[i] == bestMove) {
                        return bestMove;
                    }
                }

                // Fallback: first legal move
                return allMoves[0];
            }
        }
        catch (const std::exception&) {
            // Silent fallback below
        }
        catch (...) {
            // Silent fallback below
        }

        // FALLBACKS

        if (allMoves.empty()) {
            return Move("move", std::vector<int>{0, 0}, std::vector<int>{0, 0});
        }

        return selectRandomMove(safeMoves);
    }
};

// ==================== PYBIND11 BINDINGS ====================

PYBIND11_MODULE(student_agent_module, m) {
    py::class_<Move>(m, "Move")
        .def_readonly("action", &Move::action)
        .def_readonly("from_pos", &Move::from)
        .def_readonly("to_pos", &Move::to)
        .def_readonly("pushed_to", &Move::pushed_to)
        .def_readonly("orientation", &Move::orientation);

    py::class_<StudentAgent>(m, "StudentAgent")
        .def(py::init<std::string>())
        .def("choose", &StudentAgent::choose);
}
