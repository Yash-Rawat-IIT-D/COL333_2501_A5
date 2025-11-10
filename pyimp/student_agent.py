"""
StudentAgent - Strong bot for River and Stones.

This module is what `agent.py` imports:

    from student_agent import StudentAgent

Constraints:
- No dependency on agent.py or gameEngine.py.
- Logic built on our own core engine:
    - core.move.Move
    - core.state.GameState
    - core.movegen.MoveGenerator
    - core.eval.BoardEvaluator
    - core.search.MinimaxEngine
    - core.timing.TimeManager, TimeMode
"""

from __future__ import annotations

import random
from typing import Any, Dict, List, Optional, Tuple

from core.move import Move
from core.state import GameState
from core.movegen import MoveGenerator
from core.eval import BoardEvaluator
from core.search import MinimaxEngine
from core.timing import TimeManager, TimeMode



class StudentAgent:
    """
    Tournament bot implementation.

    Interface expected by agent.py:
        StudentAgent(player: str)
        choose(board, rows, cols, score_cols, current_player_time, opponent_time) -> move_dict

    Design:
    - Converts external board objects -> compact GameState.
    - Generates moves with MoveGenerator.
    - Scores positions with BoardEvaluator.
    - Searches with MinimaxEngine (negamax + alpha-beta).
    - Adapts depth/strategy based on remaining time via TimeManager.
    - Avoids moving stones out of scoring cells (safe-move filter).
    """

    def __init__(self, player: str):
        if player not in ("circle", "square"):
            raise ValueError(f"Invalid player: {player}")
        self.player: str = player
        self.opponent: str = "square" if player == "circle" else "circle"

        # Core engine components
        self.moveGen = MoveGenerator()
        self.evaluator = BoardEvaluator(self.moveGen)
        self.search = MinimaxEngine(self.evaluator, self.moveGen)
        self.timer = TimeManager()

        # Defense mode heuristic: mirror C++ logic
        # Circle tends to play offensive, Square more defensive.
        self.evaluator.setDefenseMode(player == "square")

        # Reusable GameState; recreated each choose() based on actual rows/cols.
        self._game_state: Optional[GameState] = None

    # ==========================================================
    # Public API (called by agent.py)
    # ==========================================================

    def choose(
        self,
        board: List[List[Any]],
        rows: int,
        cols: int,
        score_cols: List[int],
        current_player_time: float,
        opponent_time: float,
    ) -> Optional[Dict[str, Any]]:
        """
        Select a move for the current state.

        Args:
            board: 2D list of piece-like objects:
                   - None for empty
                   - or object/dict with fields: owner, side, orientation
            rows, cols: board dimensions
            score_cols: scoring columns (provided by framework)
            current_player_time: remaining time for *this* player (seconds)
            opponent_time: remaining time for opponent (unused here, but available)

        Returns:
            A move dict in the format expected by agent.py/gameEngine:
              {
                "action": "move" | "push" | "flip" | "rotate",
                "from": [x, y],
                "to": [nx, ny],            # for move/push
                "pushed_to": [px, py],     # for push
                "orientation": "horizontal"|"vertical"  # for flip stone->river
              }
            or None if no legal moves exist.
        """

        if rows <= 0 or cols <= 0:
            return None

        # Build internal GameState from external board
        gs = GameState(rows, cols)
        gs.load_from_python(self._normalize_board(board))
        self._game_state = gs

        # Start timing for this move
        self.timer.start_timer(float(max(0.01, current_player_time)))

        # Generate all legal moves for our side
        all_moves: List[Move] = self.moveGen.generate_all_moves_optimized(gs, self.player)
        if not all_moves:
            # No legal moves: return a harmless pass-like move
            return None

        # Filter out moves that pull stones out of scoring area
        safe_moves = self._filter_safe_moves(all_moves, gs)

        # Decide search regime from time mode
        mode = self.timer.get_time_mode()

        try:
            if mode is TimeMode.PANIC:
                # Ultra low time: fast random among safe moves
                chosen = self._select_random_move(safe_moves)

            elif mode is TimeMode.EVAL:
                # Low time: 0-ply: evaluate each safe move, pick best
                chosen = self._select_best_by_eval(safe_moves, gs)

            elif mode is TimeMode.PLY_ONE:
                # Medium low: depth-1 search
                chosen = self.search.getBestMove(gs, self.player, maxDepth=1)

            else:
                # Good time: depth-2 search (tunable)
                chosen = self.search.getBestMove(gs, self.player, maxDepth=2)

            # Safety: ensure chosen is legal; otherwise fallback
            if chosen not in all_moves:
                chosen = safe_moves[0] if safe_moves else all_moves[0]

        except Exception:
            # On any unexpected issue, fall back to a simple safe random move
            chosen = self._select_random_move(safe_moves or all_moves)

        return self._move_to_dict(chosen)

    # ==========================================================
    # Internal helpers
    # ==========================================================

    def _normalize_board(
        self,
        board: List[List[Any]],
    ) -> List[List[Dict[str, str]]]:
        """
        Convert external board representation into the format
        expected by GameState.load_from_python:

            cell = {}                               # empty
            cell = {
                "owner": "circle"/"square",
                "side": "stone"/"river",
                "orientation": "horizontal"/"vertical"
            }

        Accepts:
        - None for empty
        - dict with the above keys
        - object with attributes .owner, .side, .orientation
        """
        rows = len(board)
        cols = len(board[0]) if rows > 0 else 0

        norm: List[List[Dict[str, str]]] = [[{} for _ in range(cols)] for _ in range(rows)]

        for y in range(rows):
            row = board[y]
            for x in range(cols):
                cell = row[x]
                if not cell:
                    continue

                if isinstance(cell, dict):
                    owner = cell.get("owner")
                    side = cell.get("side")
                    if not owner or not side:
                        continue
                    ori = cell.get("orientation", "horizontal")
                else:
                    # Treat as engine piece object
                    owner = getattr(cell, "owner", None)
                    side = getattr(cell, "side", None)
                    if not owner or not side:
                        continue
                    ori = getattr(cell, "orientation", "horizontal")

                norm[y][x] = {
                    "owner": owner,
                    "side": side,
                    "orientation": ori,
                }

        return norm

    def _filter_safe_moves(self, moves: List[Move], gs: GameState) -> List[Move]:
        """
        Equivalent to C++ filterToSafeMoves:
        - If a stone is already in our scoring row and scoring column, avoid moving it out.
        """
        score_cols = gs.getScoreCols()
        is_circle = (self.player == "circle")
        scoring_row = 2 if is_circle else (gs.getRows() - 3)

        safe: List[Move] = []
        for mv in moves:
            ok = True
            if mv.action == "move":
                fx, fy = mv.from_pos
                if fy == scoring_row and fx in score_cols:
                    # This piece is scoring; don't walk it out
                    ok = False
            if ok:
                safe.append(mv)

        return safe if safe else moves

    def _select_best_by_eval(self, moves: List[Move], gs: GameState) -> Move:
        """
        0-ply evaluation over candidate moves (no recursion).
        """
        if not moves:
            return self._select_random_move(moves)

        if len(moves) == 1:
            return moves[0]

        is_circle = (self.player == "circle")
        best = moves[0]
        best_score = float("-inf")

        for mv in moves:
            ui = gs.applyMove(mv)
            score = self.evaluator.EvaluateBoard(gs, is_circle)
            gs.undoMove(ui)

            if score > best_score:
                best_score = score
                best = mv

        return best

    def _select_random_move(self, moves: List[Move]) -> Move:
        if not moves:
            # Dummy; should not normally hit if caller checks
            return Move("move", (0, 0), (0, 0))
        return random.choice(moves)

    def _move_to_dict(self, mv: Move) -> Dict[str, Any]:
        """
        Convert internal Move -> dict format expected by framework.
        Adjust this if your Move dataclass uses different field names.
        """
        data: Dict[str, Any] = {
            "action": mv.action,
            "from": [mv.from_pos[0], mv.from_pos[1]],
            "to": [mv.to_pos[0], mv.to_pos[1]],
        }

        if getattr(mv, "pushed_to", None):
            data["pushed_to"] = [mv.pushed_to[0], mv.pushed_to[1]]

        if getattr(mv, "orientation", ""):
            data["orientation"] = mv.orientation

        return data
