# search.py
# MinimaxEngine: alpha-beta negamax with root ordering & repetition avoidance.

from __future__ import annotations
from dataclasses import dataclass
from typing import List, Tuple
from collections import deque

from .move import Move
from .state import GameState
from .eval import BoardEvaluator
from .movegen import MoveGenerator


# Match the C++-style defaults used in the snippet
PLY_DEPTH_ONE = 1


@dataclass
class SearchResult:
    evaluation: float = -1e10
    bestMove: Move = Move("move", (0, 0), (0, 0))
    depth_reached: int = 0
    timeout_occurred: bool = False  # kept for future TimeManager integration


class MinimaxEngine:
    """
    Hot-path search engine.
    - Negamax + alpha-beta pruning
    - Root move ordering by quick eval (drop-in heuristic)
    - Iterative deepening loop (depth 1..maxDepth)
    - Repetition-avoidance (last-k same move filter)
    """

    __slots__ = (
        "evaluator",
        "moveGenerator",
        "nodes_searched",
        "max_depth_reached",
        "recent_moves",
        "REPETITION_HISTORY",
    )

    def __init__(self, eval: BoardEvaluator, moveGen: MoveGenerator) -> None:
        self.evaluator: BoardEvaluator = eval
        self.moveGenerator: MoveGenerator = moveGen

        # wire the evaluator with move generator if not already
        try:
            # eval may not expose setter if already injected earlier
            self.evaluator.setMoveGenerator(self.moveGenerator)  # type: ignore[attr-defined]
        except Exception:
            pass

        self.nodes_searched: int = 0
        self.max_depth_reached: int = 0

        # repetition memory (tiny)
        self.REPETITION_HISTORY: int = 3
        self.recent_moves: deque[Move] = deque(maxlen=self.REPETITION_HISTORY)

    # ---------------- public API ----------------

    def getBestMove(
        self,
        position: GameState,
        player: str,
        maxDepth: int = PLY_DEPTH_ONE,
    ) -> Move:
        # reset stats
        self.nodes_searched = 0
        self.max_depth_reached = 0

        isCirclePlayer = (player == "circle")

        allRootMoves = self.generateMovesForPosition(position, player)
        rootMoves = self._selectTopRootMoves(position, allRootMoves, isCirclePlayer, maxMoves=48)

        if not rootMoves:
            # no legal moves — return a default "do-nothing" move
            return Move("move", (0, 0), (0, 0))

        if len(rootMoves) == 1:
            only = rootMoves[0]
            self._addMoveToHistory(only)
            return only

        bestResult = SearchResult(bestMove=rootMoves[0])

        # Iterative deepening: 1..maxDepth
        for depth in range(1, maxDepth + 1):
            current = self._searchAtDepth(position, depth, isCirclePlayer, rootMoves)
            if not current.timeout_occurred:
                bestResult = current
                self.max_depth_reached = depth

            # very high eval cutoff (mate-like)
            if bestResult.evaluation > 1_000_000.0:
                break

        finalMove = bestResult.bestMove

        # repetition avoidance: if the chosen move equals the last two, pick first alternative
        if self._isRepeatingMove(finalMove) and len(rootMoves) > 1:
            for alt in rootMoves:
                if alt != finalMove and not self._isRepeatingMove(alt):
                    finalMove = alt
                    break

        self._addMoveToHistory(finalMove)
        return finalMove

    def getNodesSearched(self) -> int:
        return self.nodes_searched

    def getMaxDepthReached(self) -> int:
        return self.max_depth_reached

    # ---------------- repetition helpers ----------------

    def _isRepeatingMove(self, move: Move) -> bool:
        # need at least 2 previous moves to detect a 3-in-a-row pattern
        if len(self.recent_moves) < self.REPETITION_HISTORY - 1:
            return False
        # check if all recent moves equal this move
        for m in self.recent_moves:
            if m != move:
                return False
        return True

    def _addMoveToHistory(self, move: Move) -> None:
        self.recent_moves.append(move)

    # ---------------- root selection & ordering ----------------

    def _selectTopRootMoves(
        self,
        position: GameState,
        allMoves: List[Move],
        isCirclePlayer: bool,
        maxMoves: int = 32,
    ) -> List[Move]:
        # if already few moves, skip ordering
        if len(allMoves) <= maxMoves:
            return allMoves

        # quick evaluate each move at depth 0 (stand-pat after apply)
        evals: List[tuple[float, Move]] = []
        append_eval = evals.append
        evaluator = self.evaluator

        for mv in allMoves:
            ui = position.applyMove(mv)
            score = evaluator.EvaluateBoard(position, isCirclePlayer)
            append_eval((score, mv))
            position.undoMove(ui)

        # sort descending by eval
        evals.sort(key=lambda p: p[0], reverse=True)

        # take top-K (copy only moves)
        top = [mv for _, mv in evals[:maxMoves]]
        return top

    # ---------------- core search ----------------

    def _searchAtDepth(
        self,
        position: GameState,
        depth: int,
        isCirclePlayer: bool,
        rootMoves: List[Move],
    ) -> SearchResult:
        best = SearchResult(evaluation=-1e10, bestMove=rootMoves[0], depth_reached=depth)

        alpha = -1e10
        beta = 1e10

        # No PV/TT ordering at root for simplicity (plug-in later if needed)
        ordered = rootMoves

        for mv in ordered:
            ui = position.applyMove(mv)
            self.nodes_searched += 1

            ev = -self._negamax(position, depth - 1, -beta, -alpha, not isCirclePlayer, 1)

            position.undoMove(ui)

            if ev > best.evaluation:
                best.evaluation = ev
                best.bestMove = mv

            if ev > alpha:
                alpha = ev

            if beta <= alpha:
                # alpha-beta cutoff
                break

        return best

    def _negamax(
        self,
        position: GameState,
        depth: int,
        alpha: float,
        beta: float,
        isCirclePlayer: bool,
        ply: int,
    ) -> float:
        self.nodes_searched += 1

        # leaf
        if depth == 0:
            return self.evaluator.EvaluateBoard(position, isCirclePlayer)

        # generate moves
        current_player = "circle" if isCirclePlayer else "square"
        moves = self.generateMovesForPosition(position, current_player)

        if not moves:
            # no moves: bad position
            return -50.0

        # (Future: add internal move ordering here if needed)
        best = -1e10
        a = alpha

        for mv in moves:
            ui = position.applyMove(mv)

            val = -self._negamax(position, depth - 1, -beta, -a, not isCirclePlayer, ply + 1)

            position.undoMove(ui)

            if val > best:
                best = val

            if val > a:
                a = val

            if a >= beta:
                break  # cutoff

        return best

    # ---------------- movegen wrapper ----------------

    def generateMovesForPosition(self, position: GameState, player: str) -> List[Move]:
        # Use the optimized generator that scans only player pieces
        return self.moveGenerator.generate_all_moves_optimized(position, player)
