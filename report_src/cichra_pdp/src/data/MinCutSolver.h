#ifndef MINCUTSOLVER_H
#define MINCUTSOLVER_H

#include "Graph.h"
#include <chrono>
#include <vector>

class MinCutSolver {
public:
  MinCutSolver(const Graph &g, int subsetSize);

  // Compute an initial good solution.
  void guesstimate(int numTries);
  // Print the best solution found.
  void printBestSolution() const;

  // Original parallel method.
  void betterSolveParallel(int numRandomTries);
  // Dynamic master–slave with a fixed frontier.
  void betterSolveParallelMS(int numRandomTries, int frontierDepth);
  // Dynamic master–slave with a lock-free queue and adaptive ordering.
  void betterSolveParallelMSDynamic(int numRandomTries, int frontierDepth);

private:
  // Structure to store a partial solution.
  struct PartialSolution {
    int node;                   // Next node index to assign.
    int currentCutWeight;       // Cut weight so far.
    int currentSizeX;           // Number of vertices in partition X so far.
    std::vector<bool> assigned; // Current assignment.
  };

  std::vector<long> recursionCounts;
  const Graph &graph;
  int n; // Number of vertices.
  int a; // Required size for partition X.
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

  void dfsSequential(int node, int currentCutWeight, int currentSizeX,
                     std::vector<bool> &assigned);
  int parallelLB(int startNode, int currentSizeX,
                 const std::vector<bool> &assigned) const;
  void generatePartialSolutions(int node, int currentCutWeight,
                                int currentSizeX,
                                const std::vector<bool> &assigned,
                                int frontierDepth,
                                std::vector<PartialSolution> &solutions);
};

#endif // MINCUTSOLVER_H
