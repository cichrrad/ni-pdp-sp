#ifndef MINCUTSOLVER_H
#define MINCUTSOLVER_H

#include "Graph.h"
#include <chrono>
#include <vector>

class MinCutSolver {
public:
  MinCutSolver(const Graph &g, int subsetSize);

  // The improved solver that uses parallel DFS.
  void betterSolve(int numRandomTries);

  // Used to compute an initial good solution.
  void guesstimate(int numTries);

  // Print the best solution found.
  void printBestSolution() const;

  void betterSolveParallel(int numRandomTries);

private:
  std::vector<long> recursionCounts;
  const Graph &graph;
  int n; // number of vertices in the graph
  int a; // required size for partition X
  int minCutWeight;
  std::vector<bool> assigned;
  std::vector<bool> bestPartition;
  int currentCutWeight;
  int currentSizeX;
  int recursiveCalls;
  std::chrono::time_point<std::chrono::high_resolution_clock> startTime;

  int computeCut(const std::vector<bool> &assignment) const;

  void startTimer();
  void stopTimer(const char *label);

  void betterDfsParallel(int node, int currentCutWeight, int currentSizeX,
                         std::vector<bool> assigned);
  int betterLowerBoundParallel(int startNode, int currentSizeX,
                               const std::vector<bool> &assigned) const;
};

#endif // MINCUTSOLVER_H
