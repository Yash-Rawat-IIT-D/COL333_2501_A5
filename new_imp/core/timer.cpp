// timer.cpp
#include "timer.hpp"

TimeManager::TimeManager()
    : start_time(),
      total_time_limit(0.0),
      switch_threshold(15.0),
      eval_threshold(10.0),
      panic_threshold(4.0) {}

void TimeManager::startTimer(double time_limit) {
    start_time = std::chrono::steady_clock::now();
    total_time_limit = time_limit;
}

double TimeManager::getElapsedTime() const {
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - start_time).count();
}

double TimeManager::getRemainingTime() const {
    return total_time_limit - getElapsedTime();
}

TimeMode TimeManager::getTimeMode() const {
    double remaining = getRemainingTime();

    if (remaining <= panic_threshold) {
        return TimeMode::PANIC;      // Panic mode
    } else if (remaining <= eval_threshold) {
        return TimeMode::EVAL;       // Evaluation-only / very shallow
    } else if (remaining <= switch_threshold) {
        return TimeMode::PLY_ONE;    // Depth 1 only
    } else {
        return TimeMode::PLY_TWO;    // Depth 2 or more
    }
}
