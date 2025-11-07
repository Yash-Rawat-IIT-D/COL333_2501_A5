# constants.py
# Fast integer encoding for pieces (cache-friendly, efficient comparisons)

from typing import Final, Literal, Optional

# ----- Piece Encoding (match C++ exactly) -----
EMPTY: Final[int] = 0
CIRCLE_STONE: Final[int] = 1
SQUARE_STONE: Final[int] = 2
CIRCLE_RIVER_H: Final[int] = 3
CIRCLE_RIVER_V: Final[int] = 4
SQUARE_RIVER_H: Final[int] = 5
SQUARE_RIVER_V: Final[int] = 6

# Orientation literals (for readability and fewer typos)
Orientation = Literal["horizontal", "vertical"]

# Small immutable tuples/sets for fast membership (no per-call allocations)
_STONES: Final[tuple[int, ...]] = (CIRCLE_STONE, SQUARE_STONE)
_RIVERS: Final[tuple[int, ...]] = (
    CIRCLE_RIVER_H, CIRCLE_RIVER_V, SQUARE_RIVER_H, SQUARE_RIVER_V
)
_RIVER_H: Final[tuple[int, ...]] = (CIRCLE_RIVER_H, SQUARE_RIVER_H)
_RIVER_V: Final[tuple[int, ...]] = (CIRCLE_RIVER_V, SQUARE_RIVER_V)
_CIRCLE_OWNERSHIP: Final[tuple[bool, ...]] = (
    False,  # 0 empty
    True,   # 1 circle_stone
    False,  # 2 square_stone
    True,   # 3 circle_river_h
    True,   # 4 circle_river_v
    False,  # 5 square_river_h
    False,  # 6 square_river_v
)

# ----- Basic type checks (fast) -----
def is_empty(piece: int) -> bool:
    return piece == EMPTY

def is_circle(piece: int) -> bool:
    # Circle pieces: 1,3,4
    return piece in (CIRCLE_STONE, CIRCLE_RIVER_H, CIRCLE_RIVER_V)

def is_square(piece: int) -> bool:
    # Square pieces: 2,5,6
    return piece in (SQUARE_STONE, SQUARE_RIVER_H, SQUARE_RIVER_V)

def is_stone(piece: int) -> bool:
    return piece in _STONES

def is_river(piece: int) -> bool:
    return piece in _RIVERS

def is_horizontal(piece: int) -> bool:
    return piece in _RIVER_H

def is_vertical(piece: int) -> bool:
    return piece in _RIVER_V

# ----- Ownership checks -----
def is_piece_owner(piece: int, player: str) -> bool:
    """String API: player is 'circle' or 'square'."""
    if piece == EMPTY:
        return False
    is_circle_piece = (piece in (CIRCLE_STONE, CIRCLE_RIVER_H, CIRCLE_RIVER_V))
    return (player == "circle") == is_circle_piece

def is_piece_owner_fast(piece: int, is_circle_player: bool) -> bool:
    """Branchless LUT version for hot paths."""
    return (0 <= piece < 7) and (_CIRCLE_OWNERSHIP[piece] == is_circle_player)

# C++-style aliases (optional; useful if you want identical names)
isPieceOwner = is_piece_owner
isPieceOwnerFast = is_piece_owner_fast

# ----- Extra fast checks (C++ parity names) -----
def isPieceStone(piece: int) -> bool:
    return piece in _STONES

def isPieceRiver(piece: int) -> bool:
    return piece in _RIVERS

def isRiverHorizontal(piece: int) -> bool:
    return piece in _RIVER_H

# ----- Orientation helpers -----
def get_river_orientation(piece: int) -> str:
    """Return 'horizontal'/'vertical' or '' if not a river (string form kept for parity)."""
    if piece in _RIVER_H:
        return "horizontal"
    if piece in _RIVER_V:
        return "vertical"
    return ""

# C++-style alias
getRiverOrientation = get_river_orientation

# ----- Flip / Rotate -----
def flip_piece(piece: int, orientation: Orientation = "horizontal") -> int:
    """Stone<->River conversion. Orientation used only when flipping Stone->River."""
    if piece == EMPTY:
        return EMPTY
    if piece in _STONES:
        if piece == CIRCLE_STONE:
            return CIRCLE_RIVER_H if orientation == "horizontal" else CIRCLE_RIVER_V
        else:
            return SQUARE_RIVER_H if orientation == "horizontal" else SQUARE_RIVER_V
    if piece in _RIVERS:
        return CIRCLE_STONE if is_circle(piece) else SQUARE_STONE
    return piece  # should not happen

def rotate_piece(piece: int) -> int:
    """Rotate river 90° (horizontal <-> vertical). Non-river unchanged."""
    if piece == CIRCLE_RIVER_H:
        return CIRCLE_RIVER_V
    if piece == CIRCLE_RIVER_V:
        return CIRCLE_RIVER_H
    if piece == SQUARE_RIVER_H:
        return SQUARE_RIVER_V
    if piece == SQUARE_RIVER_V:
        return SQUARE_RIVER_H
    return piece

# C++-style aliases
flipPiece = flip_piece
rotatePiece = rotate_piece

# ----- Owner flag / type index -----
def get_piece_owner_flag(piece: int) -> bool:
    """True if circle, False if square; EMPTY -> False."""
    return piece in (CIRCLE_STONE, CIRCLE_RIVER_H, CIRCLE_RIVER_V)

def get_piece_type_index(piece: int) -> int:
    """
    0 = empty
    1 = stone
    2 = river_h
    3 = river_v
    """
    if piece == EMPTY:
        return 0
    if piece in _STONES:
        return 1
    if piece in _RIVER_H:
        return 2
    return 3

# C++-style aliases
getPieceOwnerFlag = get_piece_owner_flag
getPieceTypeIndex = get_piece_type_index

# ----- Encode piece from engine strings -----
def encode_piece(
    owner: Optional[str],
    side: Optional[str],
    orientation: Orientation = "horizontal",
) -> int:
    """
    Map {owner, side, orientation} -> encoded int.
    owner in {'circle','square'} or None/'' for empty.
    side in {'stone','river'}.
    """
    if not owner or not side:
        return EMPTY
    is_circle_owner = (owner == "circle")
    if side == "stone":
        return CIRCLE_STONE if is_circle_owner else SQUARE_STONE
    # river
    if is_circle_owner:
        return CIRCLE_RIVER_H if orientation == "horizontal" else CIRCLE_RIVER_V
    else:
        return SQUARE_RIVER_H if orientation == "horizontal" else SQUARE_RIVER_V

# C++-style alias
encodePiece = encode_piece

__all__ = [
    # constants
    "EMPTY",
    "CIRCLE_STONE", "SQUARE_STONE",
    "CIRCLE_RIVER_H", "CIRCLE_RIVER_V",
    "SQUARE_RIVER_H", "SQUARE_RIVER_V",
    # checks
    "is_empty", "is_circle", "is_square", "is_stone", "is_river",
    "is_horizontal", "is_vertical",
    "is_piece_owner", "is_piece_owner_fast",
    "isPieceOwner", "isPieceOwnerFast",
    "isPieceStone", "isPieceRiver", "isRiverHorizontal",
    "get_river_orientation", "getRiverOrientation",
    "flip_piece", "rotate_piece", "flipPiece", "rotatePiece",
    "get_piece_owner_flag", "get_piece_type_index",
    "getPieceOwnerFlag", "getPieceTypeIndex",
    "encode_piece", "encodePiece",
    "Orientation",
]
