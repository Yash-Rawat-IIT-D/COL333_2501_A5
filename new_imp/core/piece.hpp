#pragma once

#include <cstdint>
#include <string>
#include "constants.hpp"

/*
 * ==================== PIECE ENCODING ====================
 * Fast integer encoding for pieces (cache-friendly, efficient comparisons):
 * 0 = empty
 * 1 = circle_stone
 * 2 = square_stone
 * 3 = circle_river_horizontal
 * 4 = circle_river_vertical
 * 5 = square_river_horizontal
 * 6 = square_river_vertical
 */

enum PieceType : std::uint8_t {
    EMPTY = 0,
    CIRCLE_STONE = 1,
    SQUARE_STONE = 2,
    CIRCLE_RIVER_H = 3,
    CIRCLE_RIVER_V = 4,
    SQUARE_RIVER_H = 5,
    SQUARE_RIVER_V = 6
};

/*
 * ==================== PIECE UTILITIES ====================
 * High-performance piece manipulation utilities.
 */

// Basic type checking
inline bool isEmpty(std::uint8_t piece) {
    return piece == EMPTY;
}

inline bool isCircle(std::uint8_t piece) {
    return piece == CIRCLE_STONE ||
           piece == CIRCLE_RIVER_H ||
           piece == CIRCLE_RIVER_V;
}

inline bool isSquare(std::uint8_t piece) {
    return piece == SQUARE_STONE ||
           piece == SQUARE_RIVER_H ||
           piece == SQUARE_RIVER_V;
}

inline bool isStone(std::uint8_t piece) {
    return piece == CIRCLE_STONE ||
           piece == SQUARE_STONE;
}

inline bool isRiver(std::uint8_t piece) {
    return piece >= CIRCLE_RIVER_H &&
           piece <= SQUARE_RIVER_V;
}

inline bool isHorizontal(std::uint8_t piece) {
    return piece == CIRCLE_RIVER_H ||
           piece == SQUARE_RIVER_H;
}

inline bool isVertical(std::uint8_t piece) {
    return piece == CIRCLE_RIVER_V ||
           piece == SQUARE_RIVER_V;
}

// Ownership check using player id
inline bool isPieceOwner(std::uint8_t piece, const std::string& player) {
    if (piece == EMPTY) return false;

    const bool isCirclePiece =
        (piece == CIRCLE_STONE ||
         piece == CIRCLE_RIVER_H ||
         piece == CIRCLE_RIVER_V);

    // Uses constants.hpp helpers (no raw "circle"/"square" literals)
    return is_CirclePlayer(player) ? isCirclePiece : !isCirclePiece;
}

// Faster ownership check using boolean flag (for hot paths)
inline bool isPieceOwnerFast(std::uint8_t piece, bool isCirclePlayerFlag) {
    // [empty, circle, square, circle, circle, square, square]
    static const bool CIRCLE_OWNERSHIP[7] = {
        false, true, false, true, true, false, false
    };
    return (piece < 7) && (CIRCLE_OWNERSHIP[piece] == isCirclePlayerFlag);
}

// Aliases used in different parts of the codebase
inline bool isPieceStone(std::uint8_t piece) {
    return isStone(piece);
}

inline bool isPieceRiver(std::uint8_t piece) {
    return isRiver(piece);
}

// River orientation as string (uses shared constants)
inline std::string getRiverOrientation(std::uint8_t piece) {
    if (!isRiver(piece)) return std::string();
    return isHorizontal(piece)
           ? ORIENT_HORIZONTAL
           : ORIENT_VERTICAL;
}

// Fast orientation check without string allocation
inline bool isRiverHorizontal(std::uint8_t piece) {
    return piece == CIRCLE_RIVER_H || piece == SQUARE_RIVER_H;
}

// Stone ↔ River conversion (flip move)
//
// Note: default parameter still uses the literal "horizontal" because
// C++11 does not allow using an extern std::string as a default arg.
// Internally we compare via isHorizontalOrientation(...) to avoid
// scattering the literal elsewhere.
inline std::uint8_t flipPiece(std::uint8_t piece,
                              const std::string& orientation = "horizontal") {
    if (piece == EMPTY) return EMPTY;

    const bool isHoriz = is_HorizontalOrientation(orientation);

    // Stone → River
    if (isStone(piece)) {
        if (piece == CIRCLE_STONE) {
            return isHoriz ? CIRCLE_RIVER_H : CIRCLE_RIVER_V;
        } else { // SQUARE_STONE
            return isHoriz ? SQUARE_RIVER_H : SQUARE_RIVER_V;
        }
    }

    // River → Stone
    if (isRiver(piece)) {
        return isCircle(piece) ? CIRCLE_STONE : SQUARE_STONE;
    }

    return piece; // Fallback (should not occur)
}

// Rotate river piece: horizontal ↔ vertical
inline std::uint8_t rotatePiece(std::uint8_t piece) {
    if (!isRiver(piece)) return piece;

    switch (piece) {
        case CIRCLE_RIVER_H:  return CIRCLE_RIVER_V;
        case CIRCLE_RIVER_V:  return CIRCLE_RIVER_H;
        case SQUARE_RIVER_H:  return SQUARE_RIVER_V;
        case SQUARE_RIVER_V:  return SQUARE_RIVER_H;
        default:              return piece;
    }
}

// True for circle pieces, false for square pieces (undefined for EMPTY)
inline bool getPieceOwnerFlag(std::uint8_t piece) {
    return piece == CIRCLE_STONE ||
           piece == CIRCLE_RIVER_H ||
           piece == CIRCLE_RIVER_V;
}

// Get compact type index for array-based evaluation logic
// 0 = empty, 1 = stone, 2 = river_h, 3 = river_v
inline int getPieceTypeIndex(std::uint8_t piece) {
    if (piece == EMPTY) return 0;
    if (isStone(piece)) return 1;
    if (isRiverHorizontal(piece)) return 2;
    return 3; // vertical river
}

// String → encoded piece
inline std::uint8_t encodePiece(const std::string& owner,
                                const std::string& side,
                                const std::string& orientation = "horizontal") {
    if (owner.empty()) return EMPTY;

    const bool isCircleOwner = is_CirclePlayer(owner);

    if (side == SIDE_STONE) {
        return isCircleOwner ? CIRCLE_STONE : SQUARE_STONE;
    }

    // side == river (fall back to string compare if needed)
    const bool isRiverSide = (side == SIDE_RIVER);
    if (!isRiverSide) {
        // Unknown side: treat as empty / invalid
        return EMPTY;
    }

    const bool isHoriz = is_HorizontalOrientation(orientation);
    if (isCircleOwner) {
        return isHoriz ? CIRCLE_RIVER_H : CIRCLE_RIVER_V;
    } else {
        return isHoriz ? SQUARE_RIVER_H : SQUARE_RIVER_V;
    }
}
