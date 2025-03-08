#ifndef MINCUTSOLVER_H
#define MINCUTSOLVER_H

#include "Graph.h"
#include <chrono>
#include <vector>

// Structure to hold DFS state for parallel DFS.
struct State {
  int node;                   // current node index in DFS
  int currentSizeX;           // number of nodes assigned to set X so far
  int currentCutWeight;       // accumulated cut weight
  std::vector<bool> assigned; // assignment: true if node is in X, false if in Y
};

class MinCutSolver {
public:
  MinCutSolver(const Graph &g, int subsetSize);

  // The improved solver that uses parallel DFS.
  void betterSolve(int numRandomTries);

  // Used to compute an initial good solution.
  void guesstimate(int numTries);

  // Print the best solution found.
  void printBestSolution() const;

  // Computes the cut value for a given partition.
  int computeCut(const std::vector<bool> &assignment) const;
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

  // Helper functions for the parallel DFS.
  int parallelBetterLowerBound(int startNode, const std::vector<bool> &assigned,
                               int currentSizeX) const;
  void parallelDfs(State state);
  void startTimer();
  void stopTimer(const char *label);
  void betterDfsParallel(int node, int currentCutWeight, int currentSizeX,
                         std::vector<bool> assigned);
  int betterLowerBoundParallel(int startNode, int currentSizeX,
                               const std::vector<bool> &assigned) const;
};

#endif // MINCUTSOLVER_H
