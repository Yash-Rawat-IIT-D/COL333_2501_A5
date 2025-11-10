#include "state.hpp"
#include "constants.hpp"
#include "piece.hpp"

// ==================== INTERNAL HELPERS ====================

void GameState::addPiecePosition(int x, int y, std::uint8_t piece) {
    if (piece == EMPTY) return;
    const std::pair<int,int> pos(x, y);
    if (isCircle(piece)) {
        circle_piece_positions.push_back(pos);
    } else if (isSquare(piece)) {
        square_piece_positions.push_back(pos);
    }
}

void GameState::removePiecePosition(int x, int y, std::uint8_t piece) {
    if (piece == EMPTY) return;
    const std::pair<int,int> pos(x, y);

    if (isCircle(piece)) {
        std::vector<std::pair<int,int> >::iterator it =
            std::find(circle_piece_positions.begin(),
                      circle_piece_positions.end(), pos);
        if (it != circle_piece_positions.end()) {
            circle_piece_positions.erase(it);
        }
    } else if (isSquare(piece)) {
        std::vector<std::pair<int,int> >::iterator it =
            std::find(square_piece_positions.begin(),
                      square_piece_positions.end(), pos);
        if (it != square_piece_positions.end()) {
            square_piece_positions.erase(it);
        }
    }
}

void GameState::initializePositionTracking() {
    circle_piece_positions.clear();
    square_piece_positions.clear();

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            std::uint8_t piece = board[y][x];
            if (piece != EMPTY) {
                addPiecePosition(x, y, piece);
            }
        }
    }
}

// ==================== CTORS / ASSIGNMENT ====================

GameState::GameState(int r, int c)
    : rows(r),
      cols(c),
      board_size(BOARD_UNKNOWN),
      top_score_row(2),          // fixed by spec
      bottom_score_row(r - 3),   // fixed by spec
      win_count(4)               // default; updated below
{
    board.assign(rows, std::vector<std::uint8_t>(cols, EMPTY));

    // Determine board profile
    board_size = detectBoardSize(rows, cols);
    win_count = getBoardWinCount(board_size);

    // Build scoring columns using central band from constants
    score_cols = makeScoreCols(board_size, cols);

    initializePositionTracking();
}

GameState::GameState(const GameState& other)
    : board(other.board),
      rows(other.rows),
      cols(other.cols),
      score_cols(other.score_cols),
      circle_piece_positions(other.circle_piece_positions),
      square_piece_positions(other.square_piece_positions),
      board_size(other.board_size),
      top_score_row(other.top_score_row),
      bottom_score_row(other.bottom_score_row),
      win_count(other.win_count)
{
}

GameState& GameState::operator=(const GameState& other) {
    if (this != &other) {
        board = other.board;
        rows = other.rows;
        cols = other.cols;

        score_cols = other.score_cols;
        circle_piece_positions = other.circle_piece_positions;
        square_piece_positions = other.square_piece_positions;

        board_size = other.board_size;
        top_score_row = other.top_score_row;
        bottom_score_row = other.bottom_score_row;
        win_count = other.win_count;
    }
    return *this;
}
// ==================== LOADING / ACCESS ====================

void GameState::loadFromPython(
    const std::vector<std::vector<std::map<std::string, std::string> > >& python_board
) {
    for (int y = 0; y < rows && y < (int)python_board.size(); ++y) {
        for (int x = 0; x < cols && x < (int)python_board[y].size(); ++x) {
            const std::map<std::string, std::string>& cell = python_board[y][x];

            if (cell.empty()) {
                board[y][x] = EMPTY;
            } else {
                const std::string& owner = cell.at("owner");
                const std::string& side = cell.at("side");
                const std::string orientation =
                    cell.count("orientation")
                    ? cell.at("orientation")
                    : std::string("horizontal");

                board[y][x] = encodePiece(owner, side, orientation);
            }
        }
    }

    initializePositionTracking();
}

void GameState::setPiece(int x, int y, std::uint8_t piece) {
    if (!inBounds(x, y)) return;

    std::uint8_t oldPiece = board[y][x];
    removePiecePosition(x, y, oldPiece);

    board[y][x] = piece;

    addPiecePosition(x, y, piece);
}

const std::vector<std::pair<int,int> >&
GameState::getPlayerPiecePositions(bool isCirclePlayer) const {
    return isCirclePlayer ? circle_piece_positions : square_piece_positions;
}

// ==================== SCORING HELPERS ====================

bool GameState::isOpponentScoreCell(int x, int y, bool isCirclePlayer) const {
    if (std::find(score_cols.begin(), score_cols.end(), x) == score_cols.end()) {
        return false;
    }

    if (isCirclePlayer) {
        // Circle scores at bottom row
        return y == bottom_score_row;
    } else {
        // Square scores at top row
        return y == top_score_row;
    }
}

bool GameState::isOwnScoreCell(int x, int y, bool isCirclePlayer) const {
    // Own score = opponent's score if roles swapped
    return isOpponentScoreCell(x, y, !isCirclePlayer);
}

// ==================== WIN / COUNTING ====================

std::string GameState::getWinner() const {
    int circle_count = 0;
    int square_count = 0;

    // Circle's scoring area (top)
    for (std::size_t i = 0; i < score_cols.size(); ++i) {
        int x = score_cols[i];
        if (inBounds(x, top_score_row)) {
            std::uint8_t piece = board[top_score_row][x];
            if (piece == CIRCLE_STONE) {
                ++circle_count;
            }
        }
    }

    // Square's scoring area (bottom)
    const int bot_row = bottom_score_row;
    for (std::size_t i = 0; i < score_cols.size(); ++i) {
        int x = score_cols[i];
        if (inBounds(x, bot_row)) {
            std::uint8_t piece = board[bot_row][x];
            if (piece == SQUARE_STONE) {
                ++square_count;
            }
        }
    }

    if (circle_count >= win_count) return PLAYER_CIRCLE;
    if (square_count >= win_count) return PLAYER_SQUARE;
    return std::string();
}

int GameState::countScoringPieces(bool isCirclePlayer) const {
    int count = 0;
    const int target_row = isCirclePlayer ? top_score_row : bottom_score_row;
    const std::uint8_t target_piece =
        isCirclePlayer ? CIRCLE_STONE : SQUARE_STONE;

    for (std::size_t i = 0; i < score_cols.size(); ++i) {
        int x = score_cols[i];
        if (inBounds(x, target_row) && board[target_row][x] == target_piece) {
            ++count;
        }
    }
    return count;
}

int GameState::countPlayerPieces(bool isCirclePlayer) const {
    return (int)(isCirclePlayer
        ? circle_piece_positions.size()
        : square_piece_positions.size());
}

int GameState::countPlayerStones(bool isCirclePlayer) const {
    int count = 0;
    const std::vector<std::pair<int,int> >& positions =
        isCirclePlayer ? circle_piece_positions : square_piece_positions;

    for (std::size_t i = 0; i < positions.size(); ++i) {
        const std::pair<int,int>& pos = positions[i];
        std::uint8_t piece = board[pos.second][pos.first];
        if (isStone(piece)) {
            ++count;
        }
    }
    return count;
}

// ==================== PIECE QUERIES ====================

bool GameState::isPlayerPiece(int x, int y, bool isCirclePlayer) const {
    if (!inBounds(x, y)) return false;
    std::uint8_t piece = board[y][x];
    return isCirclePlayer ? isCircle(piece) : isSquare(piece);
}

std::string GameState::getPieceOwner(int x, int y) const {
    if (!inBounds(x, y) || board[y][x] == EMPTY) return std::string();

    return isCircle(board[y][x])
        ? PLAYER_CIRCLE
        : PLAYER_SQUARE;
}

std::string GameState::getPieceType(int x, int y) const {
    if (!inBounds(x, y) || board[y][x] == EMPTY) return std::string();

    return isStone(board[y][x])
        ? SIDE_STONE
        : SIDE_RIVER;
}

std::string GameState::getRiverOrientation(int x, int y) const {
    if (!inBounds(x, y) || !isRiver(board[y][x])) return std::string();

    return isHorizontal(board[y][x])
        ? ORIENT_HORIZONTAL
        : ORIENT_VERTICAL;
}

// ==================== UNDO INFO ====================

GameState::UndoInfo::UndoInfo()
    : num_captured(0) {
    captured_pieces[0] = captured_pieces[1] = captured_pieces[2] = EMPTY;
}

void GameState::UndoInfo::addCapturedPiece(int x, int y, std::uint8_t piece) {
    if (num_captured < 3) {
        captured_positions[num_captured] = std::make_pair(x, y);
        captured_pieces[num_captured] = piece;
        ++num_captured;
    }
}

// ==================== APPLY / UNDO (HIGH-LEVEL) ====================

GameState::UndoInfo GameState::applyMove(const Move& move) {
    UndoInfo undo_info;
    undo_info.move = move;

    if (move.action == "move") {
        applyBasicMoveWithUndo(
            move.from[0], move.from[1],
            move.to[0],   move.to[1],
            undo_info
        );
    } else if (move.action == "flip") {
        applyFlipWithUndo(
            move.from[0], move.from[1],
            move.orientation,
            undo_info
        );
    } else if (move.action == "rotate") {
        applyRotateWithUndo(
            move.from[0], move.from[1],
            undo_info
        );
    } else if (move.action == "push") {
        applyPushMoveWithUndo(
            move.from[0],       move.from[1],
            move.to[0],         move.to[1],
            move.pushed_to[0],  move.pushed_to[1],
            undo_info
        );
    }

    return undo_info;
}

void GameState::undoMove(const UndoInfo& undo_info) {
    const Move& move = undo_info.move;

    if (move.action == "move") {
        undoBasicMove(
            move.from[0], move.from[1],
            move.to[0],   move.to[1],
            undo_info
        );
    } else if (move.action == "flip") {
        undoFlip(
            move.from[0], move.from[1],
            undo_info
        );
    } else if (move.action == "rotate") {
        undoRotate(
            move.from[0], move.from[1],
            undo_info
        );
    } else if (move.action == "push") {
        undoPushMove(
            move.from[0],       move.from[1],
            move.to[0],         move.to[1],
            move.pushed_to[0],  move.pushed_to[1],
            undo_info
        );
    }
}

// ==================== APPLY / UNDO (LOW-LEVEL) ====================

void GameState::applyBasicMoveWithUndo(int from_x, int from_y,
                                       int to_x, int to_y,
                                       UndoInfo& undo_info) {
    if (!inBounds(from_x, from_y) || !inBounds(to_x, to_y)) return;

    std::uint8_t piece = board[from_y][from_x];
    std::uint8_t displaced_piece = board[to_y][to_x];

    undo_info.addCapturedPiece(from_x, from_y, piece);
    undo_info.addCapturedPiece(to_x, to_y, displaced_piece);

    removePiecePosition(from_x, from_y, piece);
    removePiecePosition(to_x, to_y, displaced_piece);

    board[to_y][to_x] = piece;
    board[from_y][from_x] = EMPTY;

    addPiecePosition(to_x, to_y, piece);
}

void GameState::undoBasicMove(int from_x, int from_y,
                              int to_x, int to_y,
                              const UndoInfo& undo_info) {
    if (undo_info.num_captured < 2) return;

    std::uint8_t original_from_piece = undo_info.captured_pieces[0];
    std::uint8_t original_to_piece   = undo_info.captured_pieces[1];

    removePiecePosition(to_x, to_y, board[to_y][to_x]);
    removePiecePosition(from_x, from_y, board[from_y][from_x]);

    board[from_y][from_x] = original_from_piece;
    board[to_y][to_x]     = original_to_piece;

    addPiecePosition(from_x, from_y, original_from_piece);
    addPiecePosition(to_x, to_y, original_to_piece);
}

void GameState::applyPushMoveWithUndo(int from_x, int from_y,
                                      int to_x, int to_y,
                                      int push_x, int push_y,
                                      UndoInfo& undo_info) {
    if (!inBounds(from_x, from_y) ||
        !inBounds(to_x, to_y)     ||
        !inBounds(push_x, push_y)) {
        return;
    }

    std::uint8_t our_piece       = board[from_y][from_x];
    std::uint8_t pushed_piece    = board[to_y][to_x];
    std::uint8_t displaced_piece = board[push_y][push_x];

    undo_info.addCapturedPiece(from_x, from_y, our_piece);
    undo_info.addCapturedPiece(to_x, to_y, pushed_piece);
    undo_info.addCapturedPiece(push_x, push_y, displaced_piece);

    removePiecePosition(from_x, from_y, our_piece);
    removePiecePosition(to_x, to_y, pushed_piece);
    removePiecePosition(push_x, push_y, displaced_piece);

    // Move pushed piece
    board[push_y][push_x] = pushed_piece;

    // Our piece becomes stone if it was a river
    std::uint8_t final_our_piece =
        isRiver(our_piece)
            ? (isCircle(our_piece) ? CIRCLE_STONE : SQUARE_STONE)
            : our_piece;

    board[to_y][to_x]     = final_our_piece;
    board[from_y][from_x] = EMPTY;

    addPiecePosition(to_x, to_y, final_our_piece);
    addPiecePosition(push_x, push_y, pushed_piece);
}

void GameState::undoPushMove(int from_x, int from_y,
                             int to_x, int to_y,
                             int push_x, int push_y,
                             const UndoInfo& undo_info) {
    if (undo_info.num_captured < 3) return;

    std::uint8_t original_from_piece = undo_info.captured_pieces[0];
    std::uint8_t original_to_piece   = undo_info.captured_pieces[1];
    std::uint8_t original_push_piece = undo_info.captured_pieces[2];

    removePiecePosition(from_x, from_y, board[from_y][from_x]);
    removePiecePosition(to_x, to_y, board[to_y][to_x]);
    removePiecePosition(push_x, push_y, board[push_y][push_x]);

    board[from_y][from_x] = original_from_piece;
    board[to_y][to_x]     = original_to_piece;
    board[push_y][push_x] = original_push_piece;

    addPiecePosition(from_x, from_y, original_from_piece);
    addPiecePosition(to_x, to_y, original_to_piece);
    addPiecePosition(push_x, push_y, original_push_piece);
}

void GameState::applyFlipWithUndo(int x, int y,
                                  const std::string& new_orientation,
                                  UndoInfo& undo_info) {
    if (!inBounds(x, y)) return;

    std::uint8_t old_piece = board[y][x];
    if (old_piece == EMPTY) return;

    undo_info.addCapturedPiece(x, y, old_piece);

    bool isCircleOwner = isCircle(old_piece);
    std::uint8_t new_piece;

    if (isStone(old_piece)) {
        bool isHoriz = (new_orientation == "horizontal");
        if (isCircleOwner) {
            new_piece = isHoriz ? CIRCLE_RIVER_H : CIRCLE_RIVER_V;
        } else {
            new_piece = isHoriz ? SQUARE_RIVER_H : SQUARE_RIVER_V;
        }
    } else {
        // River -> Stone
        new_piece = isCircleOwner ? CIRCLE_STONE : SQUARE_STONE;
    }

    removePiecePosition(x, y, old_piece);
    board[y][x] = new_piece;
    addPiecePosition(x, y, new_piece);
}

void GameState::undoFlip(int x, int y,
                         const UndoInfo& undo_info) {
    if (undo_info.num_captured < 1) return;

    std::uint8_t original_piece = undo_info.captured_pieces[0];

    removePiecePosition(x, y, board[y][x]);
    board[y][x] = original_piece;
    addPiecePosition(x, y, original_piece);
}

void GameState::applyRotateWithUndo(int x, int y,
                                    UndoInfo& undo_info) {
    if (!inBounds(x, y)) return;

    std::uint8_t old_piece = board[y][x];
    if (!isRiver(old_piece)) return;

    undo_info.addCapturedPiece(x, y, old_piece);

    std::uint8_t new_piece;
    if (old_piece == CIRCLE_RIVER_H) {
        new_piece = CIRCLE_RIVER_V;
    } else if (old_piece == CIRCLE_RIVER_V) {
        new_piece = CIRCLE_RIVER_H;
    } else if (old_piece == SQUARE_RIVER_H) {
        new_piece = SQUARE_RIVER_V;
    } else if (old_piece == SQUARE_RIVER_V) {
        new_piece = SQUARE_RIVER_H;
    } else {
        return;
    }

    removePiecePosition(x, y, old_piece);
    board[y][x] = new_piece;
    addPiecePosition(x, y, new_piece);
}

void GameState::undoRotate(int x, int y,
                           const UndoInfo& undo_info) {
    if (undo_info.num_captured < 1) return;

    std::uint8_t original_piece = undo_info.captured_pieces[0];

    removePiecePosition(x, y, board[y][x]);
    board[y][x] = original_piece;
    addPiecePosition(x, y, original_piece);
}

// ==================== WRAPPER METHODS ====================

void GameState::applyBasicMove(int from_x, int from_y,
                               int to_x, int to_y) {
    UndoInfo undo_info;
    applyBasicMoveWithUndo(from_x, from_y, to_x, to_y, undo_info);
}

void GameState::applyPushMove(int from_x, int from_y,
                              int to_x, int to_y,
                              int push_x, int push_y) {
    UndoInfo undo_info;
    applyPushMoveWithUndo(from_x, from_y, to_x, to_y, push_x, push_y, undo_info);
}

void GameState::applyFlip(int x, int y,
                          const std::string& new_orientation) {
    UndoInfo undo_info;
    applyFlipWithUndo(x, y, new_orientation, undo_info);
}

void GameState::applyRotate(int x, int y) {
    UndoInfo undo_info;
    applyRotateWithUndo(x, y, undo_info);
}

// ==================== DEBUG ====================

void GameState::printBoard() const {
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            std::cout << (int)board[y][x] << " ";
        }
        std::cout << std::endl;
    }
}
