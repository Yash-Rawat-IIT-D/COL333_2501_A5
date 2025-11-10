// timer.hpp
#pragma once

#include <chrono>

/*
 * ==================== TIME MODE ENUM ====================
 * Mirror of original enum (global namespace).
 */
enum class TimeMode {
    EVAL,
    PLY_ONE,
    PLY_TWO,
    PANIC
};

/*
 * ==================== TIME MANAGER ====================
 * Behavior-preserving split from original implementation.
 */

class TimeManager {
private:
    std::chrono::steady_clock::time_point start_time;
    double total_time_limit;
    double switch_threshold;  // Switch to depth 1 at 15s remaining
    double eval_threshold;    // Switch to eval mode at 10s remaining
    double panic_threshold;   // Switch to panic mode at 4s remaining

public:
    TimeManager();

    void startTimer(double time_limit);
    double getElapsedTime() const;
    double getRemainingTime() const;

    TimeMode getTimeMode() const;
};
