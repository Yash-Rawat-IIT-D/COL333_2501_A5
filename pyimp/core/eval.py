# eval.py
# BoardEvaluator: hot-path evaluation with lightweight caching.

from __future__ import annotations
from dataclasses import dataclass, field
from collections import deque
from typing import List, Tuple, Optional

from .constants import (
    EMPTY,
    CIRCLE_STONE, SQUARE_STONE,
    CIRCLE_RIVER_H, CIRCLE_RIVER_V, SQUARE_RIVER_H, SQUARE_RIVER_V,
    is_circle, is_square, is_stone, is_river, is_horizontal,
)
from .state import GameState
from .movegen import MoveGenerator

# Tunables
DEFENSE_BOT_DEFENSE = 4.5
OFFENSE_BOT_DEFENSE = 1.5


@dataclass
class ScoringArea:
    row: int = 0
    # use default_factory to avoid mutable-default error
    score_cols: List[int] = field(default_factory=list)


@dataclass(frozen=True)
class RiverFlowCacheEntry:
    river_x: int
    river_y: int
    source_x: int
    source_y: int
    is_circle_player: bool
    flow_destinations: Tuple[Tuple[int, int], ...]


@dataclass(frozen=True)
class RiverSimulationCacheEntry:
    river_x: int
    river_y: int
    is_circle_player: bool
    simulation_result: Tuple[Tuple[int, int], ...]


class BoardEvaluator:
    """
    Port of the C++ BoardEvaluator with Pythonic micro-optimizations.
    Uses:
      - compact piece encoding from constants.py
      - GameState / MoveGenerator from core.*
    """

    __slots__ = (
        "move_generator",
        "defense_mode",
        "weight1", "weight2",
        "weight_river_mobility", "weight_river_combos",
        "weight_board_adv", "weight_defense",
        "_circle_scoring", "_square_scoring", "_scoring_init",
        "_vis", "_rows", "_cols",
    )

    def __init__(self, moveGen: Optional[MoveGenerator] = None) -> None:
        self.move_generator: Optional[MoveGenerator] = moveGen
        self.defense_mode: bool = False

        # weights
        self.weight1: float = 50000.0
        self.weight2: float = -5.0
        self.weight_river_mobility: float = 0.15
        self.weight_river_combos: float = 0.05
        self.weight_board_adv: float = 0.15
        self.weight_defense: float = OFFENSE_BOT_DEFENSE

        # scoring areas (initialized lazily)
        self._circle_scoring: ScoringArea = ScoringArea()
        self._square_scoring: ScoringArea = ScoringArea()
        self._scoring_init: bool = False

        # reusable BFS visited matrix
        self._vis: List[List[bool]] = []
        self._rows: int = 0
        self._cols: int = 0

    # ---------- setup ----------

    def setMoveGenerator(self, moveGen: MoveGenerator) -> None:
        self.move_generator = moveGen

    def setDefenseMode(self, mode: bool) -> None:
        self.defense_mode = mode
        self.weight_defense = DEFENSE_BOT_DEFENSE if mode else OFFENSE_BOT_DEFENSE

    def _ensure_scoring_areas(self, s: GameState) -> None:
        if self._scoring_init:
            return
        rows = s.getRows()
        score_cols = s.getScoreCols()
        self._circle_scoring = ScoringArea(row=2, score_cols=score_cols[:])
        self._square_scoring = ScoringArea(row=rows - 3, score_cols=score_cols[:])
        self._scoring_init = True

    def _ensure_vis(self, rows: int, cols: int) -> None:
        if rows != self._rows or cols != self._cols:
            self._rows, self._cols = rows, cols
            self._vis = [[False] * cols for _ in range(rows)]
        else:
            # fast reset
            for r in self._vis:
                r[:] = [False] * cols

    # ---------- top-level evaluation ----------

    def EvaluateBoard(self, s: GameState, isCirclePlayer: bool) -> float:
        self._ensure_scoring_areas(s)

        score1 = self.computeBasicEvaluation(s, isCirclePlayer)
        score2 = self.evaluatePosition(s, isCirclePlayer)
        score_mob = self.evaluateMobility(s, isCirclePlayer)

        sim_cache: List[RiverSimulationCacheEntry] = []
        river_mobility = self.evaluateRiverMobility(s, isCirclePlayer, sim_cache)
        river_combos = self.evaluateRiverCombos(s, isCirclePlayer, sim_cache)

        base_score = score1 * self.weight1 + score2 * self.weight2
        river_total = (
            river_mobility * self.weight_river_mobility
            + river_combos * self.weight_river_combos
        )
        board_adv_score = (
            self.evaluateBoardAdvancement(s, isCirclePlayer) * self.weight_board_adv
        )
        defense_score = self.evaluateDefense(s, isCirclePlayer) * self.weight_defense

        return base_score + river_total + board_adv_score + defense_score

    # ---------- components ----------

    def computeBasicEvaluation(self, s: GameState, isCirclePlayer: bool) -> float:
        score = 0.0
        ps = self.countStonesInScoringArea(s, isCirclePlayer)
        os = self.countStonesInScoringArea(s, not isCirclePlayer)
        pr = self.countRiversInScoringArea(s, isCirclePlayer)
        or_ = self.countRiversInScoringArea(s, not isCirclePlayer)
        score += ps * 2.0 - os * 2.0
        score += pr * 1.0 - or_ * 1.0
        return score

    def evaluatePosition(self, s: GameState, isCirclePlayer: bool) -> float:
        dists = self.getmoveDistancesFromScoringArea(s, isCirclePlayer)
        return float(sum(dists[:4])) if dists else 0.0

    def evaluateMobility(self, s: GameState, isCirclePlayer: bool) -> float:
        return float(self.countStonesAdjacentToRivers(s, isCirclePlayer))

    # ---------- defense ----------

    def evaluateDefense(self, s: GameState, isCirclePlayer: bool) -> float:
        score = 0.0
        score_cols = s.getScoreCols()
        opp_row = (
            self._square_scoring.row if isCirclePlayer else self._circle_scoring.row
        )
        for sc in score_cols:
            score += self._evaluateColumnDefense(s, sc, opp_row, isCirclePlayer)
        return score

    def _evaluateColumnDefense(
        self, s: GameState, score_col: int, opp_row: int, isCirclePlayer: bool
    ) -> float:
        val = 0.0
        val += self._evaluateDefensePosition(
            s, score_col, opp_row - 1, isCirclePlayer, "vertical"
        )
        val += self._evaluateDefensePosition(
            s, score_col, opp_row + 1, isCirclePlayer, "vertical"
        )

        cols = s.getScoreCols()
        if score_col == cols[0]:
            val += self._evaluateDefensePosition(
                s, score_col - 1, opp_row, isCirclePlayer, "horizontal"
            )
        if score_col == cols[-1]:
            val += self._evaluateDefensePosition(
                s, score_col + 1, opp_row, isCirclePlayer, "horizontal"
            )
        return val

    def _evaluateDefensePosition(
        self, s: GameState, x: int, y: int, isCirclePlayer: bool, pos_type: str
    ) -> float:
        if not s.in_bounds(x, y):
            return 0.0
        p = s.get_piece(x, y)
        if p == EMPTY:
            return 0.0

        is_ours = is_circle(p) if isCirclePlayer else is_square(p)
        if not is_ours:
            return -60.0

        if is_stone(p):
            return 5.0 if pos_type == "vertical" else 12.0

        if is_river(p):
            horiz = is_horizontal(p)
            if pos_type == "vertical":
                return 5.0 if horiz else -5.0
            else:
                return 15.0 if not horiz else -30.0

        return 0.0

    # ---------- advancement ----------

    def evaluateBoardAdvancement(self, s: GameState, isCirclePlayer: bool) -> float:
        adv = 0.0
        positions = s.getPlayerPiecePositions(isCirclePlayer)
        score_cols = s.getScoreCols()
        rows = s.getRows()
        target_row = 2 if isCirclePlayer else (rows - 3)

        pieces_in_ring = [0, 0, 0, 0, 0]
        stones_in_ring = [0, 0, 0, 0, 0]

        for (x, y) in positions:
            p = s.get_piece(x, y)
            md = 100
            for sc in score_cols:
                d = abs(x - sc) + abs(y - target_row)
                if d < md:
                    md = d
            if md > 4:
                continue
            ring = md
            pieces_in_ring[ring] += 1
            if is_stone(p):
                stones_in_ring[ring] += 1

        adv += pieces_in_ring[1] * 60.0 + stones_in_ring[1] * 120.0
        adv += pieces_in_ring[2] * 8.0 + stones_in_ring[2] * 15.0
        adv += pieces_in_ring[3] * 3.0 + stones_in_ring[3] * 5.0
        adv += pieces_in_ring[4] * 1.0 + stones_in_ring[4] * 1.0

        col_bonus = 0.0
        for (x, y) in positions:
            if x in score_cols:
                p = s.get_piece(x, y)
                vdist = abs(y - target_row)
                if vdist == 1:
                    col_bonus += 15.0 if is_stone(p) else 6.0
                elif vdist == 2:
                    col_bonus += 8.0 if is_stone(p) else 3.0
                elif vdist <= 4:
                    col_bonus += 3.0 if is_stone(p) else 1.0
        adv += col_bonus
        return adv

    # ---------- river combos / mobility ----------

    def evaluateRiverCombos(
        self, s: GameState, isCirclePlayer: bool, cache: List[RiverSimulationCacheEntry]
    ) -> float:
        positions = s.getPlayerPiecePositions(isCirclePlayer)
        rivers: List[Tuple[int, int]] = [
            (x, y) for (x, y) in positions if is_river(s.get_piece(x, y))
        ]
        combo = 0.0
        n = len(rivers)
        for i in range(n):
            for j in range(i + 1, n):
                combo += self._evaluateRiverConnectionWithCache(
                    s, rivers[i], rivers[j], isCirclePlayer, cache
                )
        return combo

    def _getCachedRiverSimulation(
        self,
        s: GameState,
        rx: int,
        ry: int,
        isCirclePlayer: bool,
        cache: List[RiverSimulationCacheEntry],
    ) -> List[Tuple[int, int]]:
        for entry in cache:
            if (
                entry.river_x == rx
                and entry.river_y == ry
                and entry.is_circle_player == isCirclePlayer
            ):
                return list(entry.simulation_result)
        res = self._simulateRiverFlow(s, rx, ry)
        cache.append(
            RiverSimulationCacheEntry(rx, ry, isCirclePlayer, tuple(res))
        )
        return res

    def countReachableOpponentPositionsWithCache(
        self,
        s: GameState,
        rx: int,
        ry: int,
        isCirclePlayer: bool,
        cache: List[RiverSimulationCacheEntry],
    ) -> float:
        weighted = 0.0
        rows = s.getRows()
        half = rows // 2
        opp_start = 0 if isCirclePlayer else half
        opp_end = half if isCirclePlayer else rows

        score_cols = s.getScoreCols()
        goal_row = 2 if isCirclePlayer else (rows - 3)

        dests = self._getCachedRiverSimulation(s, rx, ry, isCirclePlayer, cache)
        for (dx, dy) in dests:
            if opp_start <= dy < opp_end:
                md = 100
                for sc in score_cols:
                    d = abs(dx - sc) + abs(dy - goal_row)
                    if d < md:
                        md = d
                weighted += max(1.0, 4.0 - md)
        return weighted

    def countReachableScoringPositionsWithCache(
        self,
        s: GameState,
        rx: int,
        ry: int,
        isCirclePlayer: bool,
        cache: List[RiverSimulationCacheEntry],
    ) -> float:
        weighted = 0.0
        score_cols = s.getScoreCols()
        target_row = 2 if isCirclePlayer else (s.getRows() - 3)

        dests = self._getCachedRiverSimulation(s, rx, ry, isCirclePlayer, cache)
        for (dx, dy) in dests:
            md = 100
            for sc in score_cols:
                d = abs(dx - sc) + abs(dy - target_row)
                if d < md:
                    md = d
            if md <= 3:
                weighted += max(1.0, 4.0 - md)
                if dy == target_row and dx in score_cols:
                    weighted += 2.0
        return weighted

    def _simulateRiverFlow(self, s: GameState, x0: int, y0: int) -> List[Tuple[int, int]]:
        out: List[Tuple[int, int]] = []
        p = s.get_piece(x0, y0)
        if not is_river(p):
            return out
        dirs = ((1, 0), (-1, 0)) if is_horizontal(p) else ((0, 1), (0, -1))
        for (dx, dy) in dirs:
            x, y = x0 + dx, y0 + dy
            while s.in_bounds(x, y):
                q = s.get_piece(x, y)
                if q == EMPTY:
                    out.append((x, y))
                    x += dx
                    y += dy
                elif is_river(q):
                    x += dx
                    y += dy
                else:
                    break
        return out

    def _evaluateRiverConnectionWithCache(
        self,
        s: GameState,
        r1: Tuple[int, int],
        r2: Tuple[int, int],
        isCirclePlayer: bool,
        cache: List[RiverSimulationCacheEntry],
    ) -> float:
        x1, y1 = r1
        x2, y2 = r2
        d1 = self._getCachedRiverSimulation(s, x1, y1, isCirclePlayer, cache)
        d2 = self._getCachedRiverSimulation(s, x2, y2, isCirclePlayer, cache)

        score = 0.0
        if (x2, y2) in d1:
            score += 1.0

        shared = 0
        for (a, b) in d1:
            for (c, d) in d2:
                if abs(a - c) + abs(b - d) <= 2:
                    shared += 1
        score += shared * 0.2
        return score

    def evaluateRiverMobility(
        self, s: GameState, isCirclePlayer: bool, cache: List[RiverSimulationCacheEntry]
    ) -> float:
        score = 0.0
        positions = s.getPlayerPiecePositions(isCirclePlayer)
        for (x, y) in positions:
            p = s.get_piece(x, y)
            if is_river(p):
                opp_reach = self.countReachableOpponentPositionsWithCache(
                    s, x, y, isCirclePlayer, cache
                )
                scoring_reach = self.countReachableScoringPositionsWithCache(
                    s, x, y, isCirclePlayer, cache
                )
                score += opp_reach * 0.15
                score += scoring_reach * 0.85
        return score

    # ---------- scoring helpers ----------

    def countStonesInScoringArea(self, s: GameState, isCirclePlayer: bool) -> int:
        area = self._circle_scoring if isCirclePlayer else self._square_scoring
        row = area.row
        cnt = 0
        for col in area.score_cols:
            if (
                s.in_bounds(col, row)
                and s.isPlayerPiece(col, row, isCirclePlayer)
                and s.getPieceType(col, row) == "stone"
            ):
                cnt += 1
        return cnt

    def countRiversInScoringArea(self, s: GameState, isCirclePlayer: bool) -> int:
        area = self._circle_scoring if isCirclePlayer else self._square_scoring
        row = area.row
        cnt = 0
        for col in area.score_cols:
            if (
                s.in_bounds(col, row)
                and s.isPlayerPiece(col, row, isCirclePlayer)
                and s.getPieceType(col, row) == "river"
            ):
                cnt += 1
        return cnt

    # ---------- distance from scoring ----------

    def getmoveDistancesFromScoringArea(self, s: GameState, isCirclePlayer: bool) -> List[int]:
        dists: List[int] = []
        positions = s.getPlayerPiecePositions(isCirclePlayer)
        flow_cache: List[RiverFlowCacheEntry] = []

        for (x, y) in positions:
            d = self._distance_from_piece_with_cache(
                s, isCirclePlayer, (x, y), flow_cache
            )
            if d != 0:
                dists.append(d)

        dists.sort()
        return dists

    def _distance_from_piece_with_cache(
        self,
        s: GameState,
        isCirclePlayer: bool,
        piece: Tuple[int, int],
        flow_cache: List[RiverFlowCacheEntry],
    ) -> int:
        px, py = piece
        if s.isOwnScoreCell(px, py, isCirclePlayer):
            return 0

        rows, cols = s.getRows(), s.getCols()
        self._ensure_vis(rows, cols)

        q: deque[Tuple[int, int, int]] = deque()
        q.append((px, py, 0))
        self._vis[py][px] = True

        DIRS = ((0, 1), (0, -1), (1, 0), (-1, 0))

        while q:
            cx, cy, cd = q.popleft()
            for (dx, dy) in DIRS:
                nx, ny = cx + dx, cy + dy
                if not s.in_bounds(nx, ny):
                    continue
                if self._vis[ny][nx]:
                    continue

                np = s.get_piece(nx, ny)

                if np == EMPTY:
                    if s.isOwnScoreCell(nx, ny, isCirclePlayer):
                        return cd + 1
                    self._vis[ny][nx] = True
                    q.append((nx, ny, cd + 1))

                elif is_river(np):
                    self._vis[ny][nx] = True
                    dests = self._getCachedRiverFlow(
                        s, nx, ny, cx, cy, isCirclePlayer, flow_cache
                    )
                    for (fx, fy) in dests:
                        if not self._vis[fy][fx]:
                            if s.isOwnScoreCell(fx, fy, isCirclePlayer):
                                return cd + 1
                            self._vis[fy][fx] = True
                            q.append((fx, fy, cd + 1))

        return 100  # unreachable

    def _getCachedRiverFlow(
        self,
        s: GameState,
        rx: int,
        ry: int,
        sx: int,
        sy: int,
        isCirclePlayer: bool,
        flow_cache: List[RiverFlowCacheEntry],
    ) -> List[Tuple[int, int]]:
        for e in flow_cache:
            if (
                e.river_x == rx
                and e.river_y == ry
                and e.source_x == sx
                and e.source_y == sy
                and e.is_circle_player == isCirclePlayer
            ):
                return list(e.flow_destinations)

        if self.move_generator is None:
            return []

        result = self.move_generator.compute_river_flow(
            s, rx, ry, sx, sy, isCirclePlayer, river_push=False
        )
        flow_cache.append(
            RiverFlowCacheEntry(rx, ry, sx, sy, isCirclePlayer, tuple(result))
        )
        return result

    # ---------- mobility helper ----------

    def countStonesAdjacentToRivers(self, s: GameState, isCirclePlayer: bool) -> int:
        cnt = 0
        positions = s.getPlayerPiecePositions(isCirclePlayer)
        for (x, y) in positions:
            p = s.get_piece(x, y)
            if not is_stone(p):
                continue
            for (dx, dy) in ((0, -1), (0, 1), (-1, 0), (1, 0)):
                nx, ny = x + dx, y + dy
                if s.in_bounds(nx, ny) and is_river(s.get_piece(nx, ny)):
                    cnt += 1
                    break
        return cnt
