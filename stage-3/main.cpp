#include "Graph.h"
#include "MinCutSolver.h"
#include <algorithm> // for std::min
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <graph_file> <subset_size>\n";
    return 1;
  }

  std::string filename = argv[1];
  int subsetSize = std::stoi(argv[2]);

  try {
    Graph g(filename);
    // Heuristic: frontierDepth = min(a , 16)
    int frontierDepth = std::min(subsetSize, 16);
    // std::cout << "Using frontierDepth = " << frontierDepth << "\n";

    MinCutSolver solver(g, subsetSize);

    // Use the static master-slave entry point (for small graphs)
    solver.betterSolveParallelMS(4, frontierDepth);
    solver.printBestSolution();

  } catch (const std::exception &e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
