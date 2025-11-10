# timing.py
# TimeMode + TimeManager (hot path): monotonic timer, minimal allocations.

from __future__ import annotations
from enum import Enum
from typing import Optional
import time


class TimeMode(Enum):
    EVAL = 0      # evaluation-only (no tree search)
    PLY_ONE = 1   # depth-1 only
    PLY_TWO = 2   # depth-2 or more
    PANIC = 3     # absolute minimum thinking (fastest move)


class TimeManager:
    """
    Lightweight per-move time manager.
    Use:
        tm = TimeManager()
        tm.start_timer(move_time_limit_sec)
        mode = tm.get_time_mode()
        # optionally: depth = tm.get_recommended_depth(default_depth)

    Design:
      - Uses time.perf_counter() (monotonic, high-res).
      - __slots__ to avoid per-instance dict.
      - Simple threshold policy matching the provided C++ snippet.
    """

    __slots__ = (
        "_start",
        "_limit",
        "switch_threshold",
        "eval_threshold",
        "panic_threshold",
    )

    def __init__(
        self,
        switch_threshold: float = 15.0,  # switch to PLY_ONE at <= 15s
        eval_threshold: float = 10.0,    # switch to EVAL at <= 10s
        panic_threshold: float = 4.0,    # switch to PANIC at <= 4s
    ) -> None:
        self._start: float = 0.0
        self._limit: float = 0.0
        self.switch_threshold: float = switch_threshold
        self.eval_threshold: float = eval_threshold
        self.panic_threshold: float = panic_threshold

    # ---------- lifecycle ----------

    def start_timer(self, time_limit_sec: float) -> None:
        """Start (or restart) the per-move timer with a time budget in seconds."""
        self._start = time.perf_counter()
        self._limit = float(time_limit_sec)

    # ---------- queries (hot path) ----------

    def get_elapsed_time(self) -> float:
        """Seconds elapsed since start_timer()."""
        if self._start == 0.0:
            return 0.0
        return time.perf_counter() - self._start

    def get_remaining_time(self) -> float:
        """Seconds remaining in current budget; never negative."""
        rem = self._limit - self.get_elapsed_time()
        return rem if rem > 0.0 else 0.0

    def get_time_mode(self) -> TimeMode:
        """
        Map remaining time -> mode (mirrors C++ logic):
            <= panic_threshold  -> PANIC
            <= eval_threshold   -> EVAL
            <= switch_threshold -> PLY_ONE
            else                -> PLY_TWO
        """
        remaining = self.get_remaining_time()
        if remaining <= self.panic_threshold:
            return TimeMode.PANIC
        elif remaining <= self.eval_threshold:
            return TimeMode.EVAL
        elif remaining <= self.switch_threshold:
            return TimeMode.PLY_ONE
        else:
            return TimeMode.PLY_TWO

    # ---------- convenience (optional) ----------

    def get_recommended_depth(self, default_depth: int) -> int:
        """
        Heuristic depth suggestion derived from TimeMode.
        - PANIC:     1
        - EVAL:      0 (evaluation only, skip search)
        - PLY_ONE:   1
        - PLY_TWO:   default_depth
        """
        mode = self.get_time_mode()
        if mode is TimeMode.PANIC:
            return 1
        if mode is TimeMode.EVAL:
            return 0
        if mode is TimeMode.PLY_ONE:
            return 1
        return max(1, int(default_depth))

    def should_panic(self) -> bool:
        """Cheap predicate for hot loops."""
        return self.get_remaining_time() <= self.panic_threshold

    def update_thresholds(self, *, switch: Optional[float] = None,
                          eval_only: Optional[float] = None,
                          panic: Optional[float] = None) -> None:
        """Adjust thresholds on the fly (useful for tuning)."""
        if switch is not None:
            self.switch_threshold = float(switch)
        if eval_only is not None:
            self.eval_threshold = float(eval_only)
        if panic is not None:
            self.panic_threshold = float(panic)
