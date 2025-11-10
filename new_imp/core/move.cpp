#include "move.hpp"
#include <utility>  // for std::move

Move::Move() = default;

Move::Move(std::string act,
           std::vector<int> f,
           std::vector<int> t)
    : action(std::move(act)),
      from(std::move(f)),
      to(std::move(t)) {}

Move::Move(std::string act,
           std::vector<int> f,
           std::vector<int> t,
           std::vector<int> pt,
           std::string ori)
    : action(std::move(act)),
      from(std::move(f)),
      to(std::move(t)),
      pushed_to(std::move(pt)),
      orientation(std::move(ori)) {}

bool Move::operator==(const Move& other) const {
    return action      == other.action &&
           from        == other.from &&
           to          == other.to &&
           pushed_to   == other.pushed_to &&
           orientation == other.orientation;
}

bool Move::operator!=(const Move& other) const {
    return !(*this == other);
}
