#pragma once

#include <string>
#include <vector>

/*
 * ==================== MOVE REPRESENTATION ====================
 * Lightweight, POD-style struct used across the engine and bindings.
 * Kept in the global namespace for full backward compatibility.
 */

struct Move {
    std::string action;
    std::vector<int> from;
    std::vector<int> to;
    std::vector<int> pushed_to;
    std::string orientation;

    // Constructors
    Move();
    Move(std::string act,
         std::vector<int> f,
         std::vector<int> t);
    Move(std::string act,
         std::vector<int> f,
         std::vector<int> t,
         std::vector<int> pt,
         std::string ori = "");

    // Equality / inequality
    bool operator==(const Move& other) const;
    bool operator!=(const Move& other) const;
};
