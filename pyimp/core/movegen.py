# movegen.py
# High-performance move generation (hot path).
# Ports the provided C++ MoveGenerator into Python with reusable buffers.

from __future__ import annotations
from dataclasses import dataclass
from collections import deque
from typing import List, Tuple, Set

from .move import Move
from .state import GameState
from .constants import (
    EMPTY,
    is_river, is_horizontal, is_stone, is_circle,
    is_piece_owner_fast,
)

# 4-neighbour directions (x, y)
DIRECTIONS: Tuple[Tuple[int, int], ...] = ((1, 0), (-1, 0), (0, 1), (0, -1))


@dataclass
class ValidTargets:
    # set of (x, y) final squares for standard "move"
    moves: Set[Tuple[int, int]]
    # list of ((own_final_x, own_final_y), (pushed_to_x, pushed_to_y))
    pushes: List[Tuple[Tuple[int, int], Tuple[int, int]]]


class MoveGenerator:
    """
    Performance choices:
      - Reuse internal buffers (visited grid, destination grid, queue, output list).
      - Use deque for BFS queue (O(1) pops).
      - Use 2D boolean grids for visited/dedup instead of Python sets of tuples.
      - Return a *copy* of the internal move buffer (safe to keep by caller).
    """

    __slots__ = (
        "_move_buffer",
        "_queue",
        "_visited",
        "_dest_grid",
        "_rows",
        "_cols",
    )

    def __init__(self) -> None:
        self._move_buffer: List[Move] = []
        self._queue: deque[Tuple[int, int]] = deque()
        self._visited: List[List[bool]] = []
        self._dest_grid: List[List[bool]] = []
        self._rows = 0
        self._cols = 0

    # ---------- public API ----------

    def generate_all_moves_optimized(self, state: GameState, player: str) -> List[Move]:
        """Optimized: iterate only player's pieces using GameState tracking."""
        self._move_buffer.clear()

        is_circle_player = (player == "circle")
        positions = (
            state.getCirclePiecePositions()
            if is_circle_player
            else state.getSquarePiecePositions()
        )

        rows, cols = state.getRows(), state.getCols()
        score_cols = state.getScoreCols()

        for (x, y) in positions:
            self._generate_for_piece(state, x, y, player, rows, cols, score_cols)

        # return a snapshot; internal buffer will be reused on next call
        return self._move_buffer[:]

    # Backward-compat alias if you prefer shorter name elsewhere
    def generate_all_moves(self, state: GameState, player: str) -> List[Move]:
        return self.generate_all_moves_optimized(state, player)

    # ---------- internal: per-piece generation ----------

    def _generate_for_piece(
        self,
        state: GameState,
        x: int,
        y: int,
        player: str,
        rows: int,
        cols: int,
        score_cols: List[int],
    ) -> None:
        piece = state.get_piece(x, y)
        vt = self._compute_valid_targets(state, x, y, player, rows, cols, score_cols)

        # movement moves
        for (tx, ty) in vt.moves:
            self._move_buffer.append(Move("move", (x, y), (tx, ty)))

        # push moves
        for ((own_tx, own_ty), (px, py)) in vt.pushes:
            self._move_buffer.append(Move("push", (x, y), (own_tx, own_ty), (px, py)))

        # flip moves
        if is_stone(piece):
            # stone -> river (both orientations)
            self._move_buffer.append(Move("flip", (x, y), (x, y), orientation="horizontal"))
            self._move_buffer.append(Move("flip", (x, y), (x, y), orientation="vertical"))
        elif is_river(piece):
            # river -> stone (orientation unused)
            self._move_buffer.append(Move("flip", (x, y), (x, y)))

        # rotate moves (only on rivers)
        if is_river(piece):
            self._move_buffer.append(Move("rotate", (x, y), (x, y)))

    # ---------- internal: targets computation ----------

    def _compute_valid_targets(
        self,
        state: GameState,
        sx: int,
        sy: int,
        player: str,
        rows: int,
        cols: int,
        score_cols: List[int],
    ) -> ValidTargets:
        moves: Set[Tuple[int, int]] = set()
        pushes: List[Tuple[Tuple[int, int], Tuple[int, int]]] = []

        if not state.in_bounds(sx, sy):
            return ValidTargets(moves, pushes)

        piece = state.get_piece(sx, sy)
        if piece == EMPTY or not is_piece_owner_fast(piece, player == "circle"):
            return ValidTargets(moves, pushes)

        is_circle_player = (player == "circle")

        for (dx, dy) in DIRECTIONS:
            tx, ty = sx + dx, sy + dy
            if not state.in_bounds(tx, ty):
                continue

            # cannot enter opponent scoring cells
            if state.isOpponentScoreCell(tx, ty, is_circle_player):
                continue

            target_piece = state.get_piece(tx, ty)

            if target_piece == EMPTY:
                # simple adjacent move
                moves.add((tx, ty))

            elif is_river(target_piece):
                # move via river flow from the entry cell (tx,ty)
                flow_dests = self._compute_river_flow(
                    state, rx=tx, ry=ty, sx=sx, sy=sy, is_circle_player=is_circle_player, river_push=False
                )
                for (fx, fy) in flow_dests:
                    moves.add((fx, fy))

            else:
                # solid piece (stone) -> potential push
                if is_stone(piece):
                    # stone pushing stone: next cell must be empty and not opp score cell
                    px, py = tx + dx, ty + dy
                    if (
                        state.in_bounds(px, py)
                        and state.get_piece(px, py) == EMPTY
                        and not state.isOpponentScoreCell(px, py, is_circle_player)
                    ):
                        pushes.append(((tx, ty), (px, py)))
                else:
                    # river pushing stone (river-push logic)
                    pushed_is_circle = is_circle(target_piece)
                    flow_dests = self._compute_river_flow(
                        state, rx=tx, ry=ty, sx=sx, sy=sy, is_circle_player=pushed_is_circle, river_push=True
                    )
                    for (fx, fy) in flow_dests:
                        if not state.isOpponentScoreCell(fx, fy, pushed_is_circle):
                            pushes.append(((tx, ty), (fx, fy)))

        return ValidTargets(moves, pushes)

    # ---------- internal: river flow ----------

    def _ensure_grids(self, rows: int, cols: int) -> None:
        if rows != self._rows or cols != self._cols:
            self._rows, self._cols = rows, cols
            self._visited = [[False] * cols for _ in range(rows)]
            self._dest_grid = [[False] * cols for _ in range(rows)]
        else:
            # reset in place
            for r in self._visited:
                r[:] = [False] * cols
            for r in self._dest_grid:
                r[:] = [False] * cols

    def _compute_river_flow(
        self,
        state: GameState,
        rx: int,
        ry: int,
        sx: int,
        sy: int,
        is_circle_player: bool,
        river_push: bool = False,
    ) -> List[Tuple[int, int]]:
        """
        BFS-based river flow:
          - Start from entry (rx,ry).
          - If river_push=True, treat entry cell (rx,ry) as if it held the *source* piece (sx,sy).
          - Propagate along river orientation; record empty endpoints, block opponent scoring cells,
            skip the original source square (sx,sy) during flow.
        """
        rows, cols = state.getRows(), state.getCols()
        self._ensure_grids(rows, cols)
        self._queue.clear()
        self._queue.append((rx, ry))

        dests: List[Tuple[int, int]] = []
        visited = self._visited
        dest_grid = self._dest_grid

        while self._queue:
            x, y = self._queue.popleft()
            if not state.in_bounds(x, y):
                continue
            if visited[y][x]:
                continue
            visited[y][x] = True

            piece = state.get_piece(x, y)

            # Special case for river push: treat entry cell as the source piece
            if river_push and x == rx and y == ry:
                piece = state.get_piece(sx, sy)

            if piece == EMPTY:
                # possible stopping destination, unless it's opponent's scoring cell
                if not state.isOpponentScoreCell(x, y, is_circle_player):
                    if not dest_grid[y][x]:
                        dest_grid[y][x] = True
                        dests.append((x, y))
                continue

            if not is_river(piece):
                # no further flow from here
                continue

            # determine river flow directions
            if is_horizontal(piece):
                dirs = ((1, 0), (-1, 0))
            else:
                dirs = ((0, 1), (0, -1))

            for (dx, dy) in dirs:
                nx, ny = x + dx, y + dy
                while state.in_bounds(nx, ny):
                    if state.isOpponentScoreCell(nx, ny, is_circle_player):
                        break
                    next_piece = state.get_piece(nx, ny)

                    if next_piece == EMPTY:
                        if not dest_grid[ny][nx]:
                            dest_grid[ny][nx] = True
                            dests.append((nx, ny))
                        nx += dx
                        ny += dy
                        continue

                    # skip original source cell during flow
                    if nx == sx and ny == sy:
                        nx += dx
                        ny += dy
                        continue

                    if is_river(next_piece):
                        # branch BFS at this river junction
                        self._queue.append((nx, ny))
                        break

                    # hit a stone (solid) -> stop this direction
                    break

        return dests

    # Public wrapper so evaluators can reuse our river BFS
    def compute_river_flow(
        self,
        state: GameState,
        rx: int,
        ry: int,
        sx: int,
        sy: int,
        is_circle_player: bool,
        river_push: bool = False,
    ) -> List[Tuple[int, int]]:
        return self._compute_river_flow(
            state, rx=rx, ry=ry, sx=sx, sy=sy, is_circle_player=is_circle_player, river_push=river_push
        )