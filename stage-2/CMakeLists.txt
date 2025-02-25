cmake_minimum_required(VERSION 3.10)
project(stage-2)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find OpenMP
find_package(OpenMP REQUIRED)

# Add the executable
add_executable(mincut main.cpp Graph.cpp MinCutSolver.cpp)

# Add desired compiler flags
target_compile_options(mincut PRIVATE -Wall -Wextra -Wpedantic -O2)

# Link OpenMP libraries
target_link_libraries(mincut PRIVATE OpenMP::OpenMP_CXX)
