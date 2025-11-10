cmake_minimum_required(VERSION 3.12)
project(student_agent_module)

# Require C++17 (still compatible with C++11-level features)
set(CMAKE_CXX_STANDARD 17)

find_package(pybind11 REQUIRED)

# Include path for headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/core)

# Build the Python extension module
pybind11_add_module(student_agent_module
    student_agent.cpp

    # Core source files
    core/constants.cpp
    core/move.cpp
    core/state.cpp
    core/movegen.cpp
    core/eval.cpp
    core/timer.cpp
    core/search.cpp
)

# Optional: optimization and warnings
target_compile_options(student_agent_module PRIVATE
    -Wall -Wextra -Wno-unused-parameter -O3
)

# Optional: show structure during build
message(STATUS "Building student_agent_module with sources:")
message(STATUS "  ${CMAKE_CURRENT_SOURCE_DIR}/student_agent.cpp")
message(STATUS "  core/ (engine files)")