# move.py
# Lightweight, immutable Move representation (C++ struct port)
# Note: avoid dataclass(slots=True) for wider Pylance/typeshed compatibility.

from dataclasses import dataclass
from typing import Optional, Tuple, Literal, Dict, Any, cast

Action = Literal["move", "push", "flip", "rotate"]
Orientation = Literal["horizontal", "vertical"]


@dataclass(frozen=True)
class Move:
    """
    C++ struct Move:
      action: string
      from:   vector<int> size=2
      to:     vector<int> size=2
      pushed_to: optional vector<int> size=2
      orientation: optional string ("horizontal"/"vertical")
    """
    action: Action
    from_pos: Tuple[int, int]
    to_pos: Tuple[int, int]
    pushed_to: Optional[Tuple[int, int]] = None
    orientation: Optional[Orientation] = None

    # ---- Convenience constructors (parity with C++ helpers) ----
    @classmethod
    def make_move(cls, f: Tuple[int, int], t: Tuple[int, int]) -> "Move":
        return cls("move", f, t)

    @classmethod
    def make_push(cls, f: Tuple[int, int], t: Tuple[int, int], pushed_to: Tuple[int, int]) -> "Move":
        return cls("push", f, t, pushed_to=pushed_to)

    @classmethod
    def make_flip(cls, at: Tuple[int, int], orientation: Orientation) -> "Move":
        # flip happens in-place: from == to == at
        return cls("flip", at, at, orientation=orientation)

    @classmethod
    def make_rotate(cls, at: Tuple[int, int]) -> "Move":
        # rotate happens in-place: from == to == at
        return cls("rotate", at, at)

    # ---- Engine I/O helpers ----
    def to_engine_dict(self) -> Dict[str, Any]:
        """Map to the engine's expected dict format."""
        d: Dict[str, Any] = {
            "action": self.action,
            "from": [self.from_pos[0], self.from_pos[1]],
            "to":   [self.to_pos[0], self.to_pos[1]],
        }
        if self.pushed_to is not None:
            d["pushed_to"] = [self.pushed_to[0], self.pushed_to[1]]
        if self.orientation is not None:
            d["orientation"] = self.orientation
        return d

    @staticmethod
    def from_engine_dict(d: Dict[str, Any]) -> "Move":
        """Construct from engine dict (defensive conversions)."""
        action = cast(Action, d["action"])
        f = cast(Tuple[int, int], tuple(d["from"]))
        t = cast(Tuple[int, int], tuple(d["to"]))
        pt_raw = d.get("pushed_to")
        pt = cast(Optional[Tuple[int, int]], tuple(pt_raw) if pt_raw is not None else None)
        ori = cast(Optional[Orientation], d.get("orientation"))
        return Move(action, f, t, pt, ori)
