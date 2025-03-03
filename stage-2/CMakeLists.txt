cmake_minimum_required(VERSION 3.10)
project(stage-1)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find OpenMP
find_package(OpenMP REQUIRED)

# Add executable
add_executable(mincut main.cpp Graph.cpp MinCutSolver.cpp)

target_compile_options(mincut PRIVATE -Wall -Wextra -Wpedantic -O2)

if(OpenMP_CXX_FOUND)
  target_link_libraries(mincut PRIVATE OpenMP::OpenMP_CXX)
endif()
