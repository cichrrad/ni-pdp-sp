cmake_minimum_required(VERSION 3.10)
project(stage-1)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add executable
add_executable(mincut main.cpp Graph.cpp MinCutSolver.cpp)

target_compile_options(mincut PRIVATE -Wall -Wextra -Wpedantic -Ofast)
