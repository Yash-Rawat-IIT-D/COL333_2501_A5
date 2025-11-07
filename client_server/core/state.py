# state.py
# GameState: hot-path board container with O(1) access and fast apply/undo.
# Mirrors the provided C++ class semantics closely.

from __future__ import annotations
from dataclasses import dataclass
from typing import List, Tuple, Optional

from .constants import (
    EMPTY,
    CIRCLE_STONE, SQUARE_STONE,
    CIRCLE_RIVER_H, CIRCLE_RIVER_V, SQUARE_RIVER_H, SQUARE_RIVER_V,
    is_circle, is_square, is_stone, is_river, is_horizontal,
    encode_piece,
)
from .move import Move


TOP_SCORE_ROW: int = 2
WIN_COUNT: int = 4


@dataclass
class UndoInfo:
    """
    Stores enough to undo any move:
      - move: the Move that was applied
      - cells: up to 3 cells (x, y, old_piece) in the order they were touched
    """
    move: Move
    cells: List[Tuple[int, int, int]]  # (x, y, old_piece)


class GameState:
    """
    Performance notes:
      - Board as List[List[int]] (small ints).
      - Position tracking: two lists of (x,y) for circle/square pieces; O(1) scans, O(n) remove.
      - All move applications are in-place with UndoInfo (no copying in hot path).
    """

    __slots__ = (
        "board", "rows", "cols", "score_cols",
        "circle_piece_positions", "square_piece_positions",
    )

    def __init__(self, rows: int, cols: int) -> None:
        self.rows: int = rows
        self.cols: int = cols
        self.board: List[List[int]] = [[EMPTY] * cols for _ in range(rows)]

        # score_cols: 4-wide window centered (as in gameEngine.py)
        w = 4
        start = max(0, (cols - w) // 2)
        self.score_cols: List[int] = [i for i in range(start, start + w)]

        # position tracking
        self.circle_piece_positions: List[Tuple[int, int]] = []
        self.square_piece_positions: List[Tuple[int, int]] = []

        # initialize empty -> nothing to add

    # -------- convenience ----------
    def copy(self) -> "GameState":
        """Shallow copy of board & tracking. Avoid in hot-path; use apply/undo instead."""
        gs = GameState(self.rows, self.cols)
        # copy board
        for y in range(self.rows):
            row = self.board[y]
            gs.board[y] = row[:]  # copy
        gs.score_cols = self.score_cols[:]  # small list
        gs.circle_piece_positions = self.circle_piece_positions[:]  # small
        gs.square_piece_positions = self.square_piece_positions[:]
        return gs

    def get_bottom_score_row(self) -> int:
        return self.rows - 3

    # -------- load from engine ----------
    def load_from_python(self, python_board: List[List[dict]]) -> None:
        """
        python_board[y][x] is {} for empty or a dict:
          {"owner": "circle"/"square", "side": "stone"/"river", "orientation": "horizontal"/"vertical"?}
        """
        # fill board
        for y in range(self.rows):
            row_py = python_board[y] if y < len(python_board) else []
            row = self.board[y]
            for x in range(self.cols):
                if x < len(row_py) and row_py[x]:
                    cell = row_py[x]
                    owner = cell.get("owner")
                    side = cell.get("side")
                    orientation = cell.get("orientation", "horizontal")
                    row[x] = encode_piece(owner, side, orientation)
                else:
                    row[x] = EMPTY

        # rebuild positions
        self.circle_piece_positions.clear()
        self.square_piece_positions.clear()
        for y in range(self.rows):
            for x in range(self.cols):
                p = self.board[y][x]
                if p != EMPTY:
                    self._add_pos(x, y, p)

    # -------- accessors ----------
    def in_bounds(self, x: int, y: int) -> bool:
        return 0 <= x < self.cols and 0 <= y < self.rows

    def get_piece(self, x: int, y: int) -> int:
        return self.board[y][x] if self.in_bounds(x, y) else EMPTY

    def set_piece(self, x: int, y: int, piece: int) -> None:
        if not self.in_bounds(x, y):
            return
        old = self.board[y][x]
        if old == piece:
            return
        if old:
            self._rem_pos(x, y, old)
        self.board[y][x] = piece
        if piece:
            self._add_pos(x, y, piece)

    def is_empty(self, x: int, y: int) -> bool:
        return self.in_bounds(x, y) and self.board[y][x] == EMPTY

    def getRows(self) -> int:
        return self.rows

    def getCols(self) -> int:
        return self.cols

    def getScoreCols(self) -> List[int]:
        return self.score_cols

    # -------- position tracking ----------
    def _add_pos(self, x: int, y: int, piece: int) -> None:
        if piece == EMPTY:
            return
        if is_circle(piece):
            self.circle_piece_positions.append((x, y))
        elif is_square(piece):
            self.square_piece_positions.append((x, y))

    def _rem_pos(self, x: int, y: int, piece: int) -> None:
        if piece == EMPTY:
            return
        pos = (x, y)
        if is_circle(piece):
            try:
                self.circle_piece_positions.remove(pos)
            except ValueError:
                pass
        elif is_square(piece):
            try:
                self.square_piece_positions.remove(pos)
            except ValueError:
                pass

    def getCirclePiecePositions(self) -> List[Tuple[int, int]]:
        return self.circle_piece_positions

    def getSquarePiecePositions(self) -> List[Tuple[int, int]]:
        return self.square_piece_positions

    def getPlayerPiecePositions(self, isCircle: bool) -> List[Tuple[int, int]]:
        return self.circle_piece_positions if isCircle else self.square_piece_positions

    # -------- scoring cells ----------
    def isOpponentScoreCell(self, x: int, y: int, isCircle: bool) -> bool:
        if x not in self.score_cols:
            return False
        # If we are circle, opponent (square) scores at bottom
        return (y == self.get_bottom_score_row()) if isCircle else (y == TOP_SCORE_ROW)

    def isOwnScoreCell(self, x: int, y: int, isCircle: bool) -> bool:
        return self.isOpponentScoreCell(x, y, not isCircle)

    # -------- winner / counts ----------
    def getWinner(self) -> str:
        circle_count = 0
        for x in self.score_cols:
            if self.in_bounds(x, TOP_SCORE_ROW) and self.board[TOP_SCORE_ROW][x] == CIRCLE_STONE:
                circle_count += 1
        square_count = 0
        bot_row = self.get_bottom_score_row()
        for x in self.score_cols:
            if self.in_bounds(x, bot_row) and self.board[bot_row][x] == SQUARE_STONE:
                square_count += 1
        if circle_count >= WIN_COUNT:
            return "circle"
        if square_count >= WIN_COUNT:
            return "square"
        return ""

    def countScoringPieces(self, isCircle: bool) -> int:
        row = TOP_SCORE_ROW if isCircle else self.get_bottom_score_row()
        target = CIRCLE_STONE if isCircle else SQUARE_STONE
        cnt = 0
        for x in self.score_cols:
            if self.in_bounds(x, row) and self.board[row][x] == target:
                cnt += 1
        return cnt

    def countPlayerPieces(self, isCircle: bool) -> int:
        return len(self.circle_piece_positions) if isCircle else len(self.square_piece_positions)

    def countPlayerStones(self, isCircle: bool) -> int:
        cnt = 0
        vec = self.circle_piece_positions if isCircle else self.square_piece_positions
        b = self.board
        for (x, y) in vec:
            if is_stone(b[y][x]):
                cnt += 1
        return cnt

    def isPlayerPiece(self, x: int, y: int, isCircle: bool) -> bool:
        if not self.in_bounds(x, y):
            return False
        p = self.board[y][x]
        return is_circle(p) if isCircle else is_square(p)

    def getPieceOwner(self, x: int, y: int) -> str:
        if not self.in_bounds(x, y):
            return ""
        p = self.board[y][x]
        if p == EMPTY:
            return ""
        return "circle" if is_circle(p) else "square"

    def getPieceType(self, x: int, y: int) -> str:
        if not self.in_bounds(x, y):
            return ""
        p = self.board[y][x]
        if p == EMPTY:
            return ""
        return "stone" if is_stone(p) else "river"

    def getRiverOrientation(self, x: int, y: int) -> str:
        if not self.in_bounds(x, y):
            return ""
        p = self.board[y][x]
        if not is_river(p):
            return ""
        return "horizontal" if is_horizontal(p) else "vertical"

    # -------- apply / undo (in-place, fast) ----------
    def applyMove(self, move: Move) -> UndoInfo:
        ui = UndoInfo(move=move, cells=[])
        a = move.action
        fx, fy = move.from_pos
        tx, ty = move.to_pos

        if a == "move":
            self._apply_basic_move_with_undo(fx, fy, tx, ty, ui)
        elif a == "flip":
            ori = move.orientation or "horizontal"
            self._apply_flip_with_undo(fx, fy, ori, ui)
        elif a == "rotate":
            self._apply_rotate_with_undo(fx, fy, ui)
        elif a == "push":
            px, py = move.pushed_to  # type: ignore[misc]
            self._apply_push_move_with_undo(fx, fy, tx, ty, px, py, ui)
        return ui

    def undoMove(self, ui: UndoInfo) -> None:
        m = ui.move
        a = m.action
        fx, fy = m.from_pos
        tx, ty = m.to_pos

        if a == "move":
            self._undo_basic_move(fx, fy, tx, ty, ui)
        elif a == "flip":
            self._undo_flip(fx, fy, ui)
        elif a == "rotate":
            self._undo_rotate(fx, fy, ui)
        elif a == "push":
            px, py = m.pushed_to  # type: ignore[misc]
            self._undo_push_move(fx, fy, tx, ty, px, py, ui)

    # ----- MOVE -----
    def _apply_basic_move_with_undo(self, fx: int, fy: int, tx: int, ty: int, ui: UndoInfo) -> None:
        if not (self.in_bounds(fx, fy) and self.in_bounds(tx, ty)):
            return
        p_from = self.board[fy][fx]
        p_to = self.board[ty][tx]
        # record originals
        ui.cells.append((fx, fy, p_from))
        ui.cells.append((tx, ty, p_to))
        # update tracking
        if p_from:
            self._rem_pos(fx, fy, p_from)
        if p_to:
            self._rem_pos(tx, ty, p_to)
        # move
        self.board[ty][tx] = p_from
        self.board[fy][fx] = EMPTY
        if p_from:
            self._add_pos(tx, ty, p_from)

    def _undo_basic_move(self, fx: int, fy: int, tx: int, ty: int, ui: UndoInfo) -> None:
        if len(ui.cells) < 2:
            return
        original_from = ui.cells[0][2]
        original_to = ui.cells[1][2]
        # remove current placements
        cur_tx = self.board[ty][tx]
        if cur_tx:
            self._rem_pos(tx, ty, cur_tx)
        cur_fx = self.board[fy][fx]
        if cur_fx:
            self._rem_pos(fx, fy, cur_fx)
        # restore
        self.board[fy][fx] = original_from
        self.board[ty][ty if False else tx] = original_to  # keep fast path; explicit index
        if original_from:
            self._add_pos(fx, fy, original_from)
        if original_to:
            self._add_pos(tx, ty, original_to)

    # ----- PUSH -----
    def _apply_push_move_with_undo(
        self, fx: int, fy: int, tx: int, ty: int, px: int, py: int, ui: UndoInfo
    ) -> None:
        if not (self.in_bounds(fx, fy) and self.in_bounds(tx, ty) and self.in_bounds(px, py)):
            return
        our_piece = self.board[fy][fx]
        pushed_piece = self.board[ty][tx]
        displaced_piece = self.board[py][px]

        ui.cells.append((fx, fy, our_piece))
        ui.cells.append((tx, ty, pushed_piece))
        ui.cells.append((px, py, displaced_piece))

        if our_piece:
            self._rem_pos(fx, fy, our_piece)
        if pushed_piece:
            self._rem_pos(tx, ty, pushed_piece)
        if displaced_piece:
            self._rem_pos(px, py, displaced_piece)

        # move pushed piece to push destination
        self.board[py][px] = pushed_piece
        if pushed_piece:
            self._add_pos(px, py, pushed_piece)

        # our moving piece becomes stone if it was river
        if is_river(our_piece):
            final_our = CIRCLE_STONE if is_circle(our_piece) else SQUARE_STONE
        else:
            final_our = our_piece

        # place our piece into (tx,ty), clear origin
        self.board[ty][tx] = final_our
        self.board[fy][fx] = EMPTY
        if final_our:
            self._add_pos(tx, ty, final_our)

    def _undo_push_move(
        self, fx: int, fy: int, tx: int, ty: int, px: int, py: int, ui: UndoInfo
    ) -> None:
        if len(ui.cells) < 3:
            return
        orig_from = ui.cells[0][2]
        orig_to = ui.cells[1][2]
        orig_push = ui.cells[2][2]

        # remove current placements
        cur_fx = self.board[fy][fx]
        if cur_fx:
            self._rem_pos(fx, fy, cur_fx)
        cur_tx = self.board[ty][tx]
        if cur_tx:
            self._rem_pos(tx, ty, cur_tx)
        cur_px = self.board[py][px]
        if cur_px:
            self._rem_pos(px, py, cur_px)

        # restore
        self.board[fy][fx] = orig_from
        self.board[ty][tx] = orig_to
        self.board[py][px] = orig_push

        if orig_from:
            self._add_pos(fx, fy, orig_from)
        if orig_to:
            self._add_pos(tx, ty, orig_to)
        if orig_push:
            self._add_pos(px, py, orig_push)

    # ----- FLIP -----
    def _apply_flip_with_undo(self, x: int, y: int, new_orientation: str, ui: UndoInfo) -> None:
        if not self.in_bounds(x, y):
            return
        old_piece = self.board[y][x]
        if old_piece == EMPTY:
            return

        ui.cells.append((x, y, old_piece))

        is_circle_owner = is_circle(old_piece)
        if is_stone(old_piece):
            # stone -> river
            if is_circle_owner:
                new_piece = CIRCLE_RIVER_H if new_orientation == "horizontal" else CIRCLE_RIVER_V
            else:
                new_piece = SQUARE_RIVER_H if new_orientation == "horizontal" else SQUARE_RIVER_V
        else:
            # river -> stone
            new_piece = CIRCLE_STONE if is_circle_owner else SQUARE_STONE

        self._rem_pos(x, y, old_piece)
        self.board[y][x] = new_piece
        self._add_pos(x, y, new_piece)

    def _undo_flip(self, x: int, y: int, ui: UndoInfo) -> None:
        if not ui.cells:
            return
        original_piece = ui.cells[0][2]
        cur = self.board[y][x]
        if cur:
            self._rem_pos(x, y, cur)
        self.board[y][x] = original_piece
        if original_piece:
            self._add_pos(x, y, original_piece)

    # ----- ROTATE -----
    def _apply_rotate_with_undo(self, x: int, y: int, ui: UndoInfo) -> None:
        if not self.in_bounds(x, y):
            return
        old_piece = self.board[y][x]
        if not is_river(old_piece):
            return

        ui.cells.append((x, y, old_piece))

        if old_piece == CIRCLE_RIVER_H:
            new_piece = CIRCLE_RIVER_V
        elif old_piece == CIRCLE_RIVER_V:
            new_piece = CIRCLE_RIVER_H
        elif old_piece == SQUARE_RIVER_H:
            new_piece = SQUARE_RIVER_V
        else:
            new_piece = SQUARE_RIVER_H

        self._rem_pos(x, y, old_piece)
        self.board[y][x] = new_piece
        self._add_pos(x, y, new_piece)

    def _undo_rotate(self, x: int, y: int, ui: UndoInfo) -> None:
        if not ui.cells:
            return
        original_piece = ui.cells[0][2]
        cur = self.board[y][x]
        if cur:
            self._rem_pos(x, y, cur)
        self.board[y][x] = original_piece
        if original_piece:
            self._add_pos(x, y, original_piece)

    # ----- legacy one-way apply (no undo return) -----
    def applyBasicMove(self, fx: int, fy: int, tx: int, ty: int) -> None:
        ui = UndoInfo(move=Move.make_move((fx, fy), (tx, ty)), cells=[])
        self._apply_basic_move_with_undo(fx, fy, tx, ty, ui)

    def applyPushMove(self, fx: int, fy: int, tx: int, ty: int, px: int, py: int) -> None:
        ui = UndoInfo(move=Move.make_push((fx, fy), (tx, ty), (px, py)), cells=[])
        self._apply_push_move_with_undo(fx, fy, tx, ty, px, py, ui)

    def applyFlip(self, x: int, y: int, orientation: str = "horizontal") -> None:
        ui = UndoInfo(move=Move.make_flip((x, y), orientation), cells=[])
        self._apply_flip_with_undo(x, y, orientation, ui)

    def applyRotate(self, x: int, y: int) -> None:
        ui = UndoInfo(move=Move.make_rotate((x, y)), cells=[])
        self._apply_rotate_with_undo(x, y, ui)

    # ----- debug -----
    def printBoard(self) -> None:
        for y in range(self.rows):
            print(" ".join(str(self.board[y][x]) for x in range(self.cols)))
