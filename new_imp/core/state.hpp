#pragma once

#include <vector>
#include <map>
#include <string>
#include <utility>
#include <cstdint>
#include <iostream>
#include <algorithm>

#include "constants.hpp"
#include "piece.hpp"
#include "move.hpp"

/*
 * ==================== GAME STATE CLASS ====================
 * Board representation, scoring logic, and move apply/undo.
 */

class GameState {
private:
    std::vector<std::vector<std::uint8_t> > board;
    int rows;
    int cols;
    std::vector<int> score_cols;

    // Position tracking for fast access
    mutable std::vector<std::pair<int,int> > circle_piece_positions;
    mutable std::vector<std::pair<int,int> > square_piece_positions;

    // New: cached board meta
    BoardSize board_size;
    int top_score_row;
    int bottom_score_row;
    int win_count;

    // Internal helpers
    void addPiecePosition(int x, int y, std::uint8_t piece);
    void removePiecePosition(int x, int y, std::uint8_t piece);
    void initializePositionTracking();



public:
    // Constructor
    GameState(int r, int c);

    // Copy constructor
    GameState(const GameState& other);

    // Assignment operator
    GameState& operator=(const GameState& other);

    // Load from Python board format
    void loadFromPython(
        const std::vector<std::vector<std::map<std::string, std::string> > >& python_board
    );

    // Accessors
    inline std::uint8_t getPiece(int x, int y) const {
        return inBounds(x, y) ? board[y][x] : EMPTY;
    }

    void setPiece(int x, int y, std::uint8_t piece);

    inline bool inBounds(int x, int y) const {
        return x >= 0 && x < cols && y >= 0 && y < rows;
    }

    inline bool isEmpty(int x, int y) const {
        return inBounds(x, y) && board[y][x] == EMPTY;
    }

    int getRows() const { return rows; }
    int getCols() const { return cols; }
    const std::vector<int>& getScoreCols() const { return score_cols; }

    int getTopScoreRow() const { return top_score_row; }
    int getBottomScoreRow() const { return bottom_score_row; }
    int getWinCount() const { return win_count; }

    // Position access
    const std::vector<std::pair<int,int> >& getCirclePiecePositions() const {
        return circle_piece_positions;
    }

    const std::vector<std::pair<int,int> >& getSquarePiecePositions() const {
        return square_piece_positions;
    }

    const std::vector<std::pair<int,int> >& getPlayerPiecePositions(bool isCircle) const;

    // Scoring area checks
    bool isOpponentScoreCell(int x, int y, bool isCircle) const;
    bool isOwnScoreCell(int x, int y, bool isCircle) const;

    // Win condition
    std::string getWinner() const;

    // Counts
    int countScoringPieces(bool isCircle) const;
    int countPlayerPieces(bool isCircle) const;
    int countPlayerStones(bool isCircle) const;

    // Piece queries
    bool isPlayerPiece(int x, int y, bool isCircle) const;
    std::string getPieceOwner(int x, int y) const;
    std::string getPieceType(int x, int y) const;
    std::string getRiverOrientation(int x, int y) const;

    // ==================== MOVE UNDO SYSTEM ====================

    struct UndoInfo {
        Move move;
        std::uint8_t captured_pieces[3];           // Max 3 pieces affected
        std::pair<int,int> captured_positions[3];
        int num_captured;

        UndoInfo();
        void addCapturedPiece(int x, int y, std::uint8_t piece);
    };

    // Apply + Undo interface
    UndoInfo applyMove(const Move& move);
    void undoMove(const UndoInfo& undo_info);

    // Low-level apply/undo
    void applyBasicMoveWithUndo(int from_x, int from_y,
                                int to_x, int to_y,
                                UndoInfo& undo_info);

    void undoBasicMove(int from_x, int from_y,
                       int to_x, int to_y,
                       const UndoInfo& undo_info);

    void applyPushMoveWithUndo(int from_x, int from_y,
                               int to_x, int to_y,
                               int push_x, int push_y,
                               UndoInfo& undo_info);

    void undoPushMove(int from_x, int from_y,
                      int to_x, int to_y,
                      int push_x, int push_y,
                      const UndoInfo& undo_info);

    void applyFlipWithUndo(int x, int y,
                           const std::string& new_orientation,
                           UndoInfo& undo_info);

    void undoFlip(int x, int y,
                  const UndoInfo& undo_info);

    void applyRotateWithUndo(int x, int y,
                             UndoInfo& undo_info);

    void undoRotate(int x, int y,
                    const UndoInfo& undo_info);

    // Convenience wrappers (compatible with old API)
    void applyBasicMove(int from_x, int from_y, int to_x, int to_y);
    void applyPushMove(int from_x, int from_y, int to_x, int to_y,
                       int push_x, int push_y);
    void applyFlip(int x, int y,
                   const std::string& new_orientation = "horizontal");
    void applyRotate(int x, int y);

    // Debug
    void printBoard() const;
};
