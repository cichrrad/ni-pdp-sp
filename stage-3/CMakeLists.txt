cmake_minimum_required(VERSION 3.10)
project(stage-3)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Find OpenMP
find_package(OpenMP REQUIRED)

# Add executable
add_executable(mincut main.cpp Graph.cpp MinCutSolver.cpp)

# Enable warnings and optimization flags
target_compile_options(mincut PRIVATE -Wall -Wextra -Wpedantic -O2)

# Add OpenMP flags and libraries if available
if(OpenMP_CXX_FOUND)
  target_compile_options(mincut PRIVATE ${OpenMP_CXX_FLAGS})
  target_link_libraries(mincut PRIVATE OpenMP::OpenMP_CXX)
endif()

# Optionally, define a macro to indicate parallel execution (can be used in your code if needed)
target_compile_definitions(mincut PRIVATE PARALLEL_EXECUTION)
