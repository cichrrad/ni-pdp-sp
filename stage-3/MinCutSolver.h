#ifndef MINCUTSOLVER_H
#define MINCUTSOLVER_H

#include "Graph.h"
#include <chrono>
#include <vector>

class MinCutSolver {
public:
  MinCutSolver(const Graph &g, int subsetSize);

  // Used to compute an initial good solution.
  void guesstimate(int numTries);
  // Print the best solution found.
  void printBestSolution() const;

  void betterSolveParallel(int numRandomTries);
  // void betterSolveParallelMS(int numRandomTries, int frontierDepth);
  void betterSolveParallelMSDynamic(int numRandomTries, int frontierDepth);

private:
  // --- Structure to store a partial solution ---
  struct PartialSolution {
    int node;                   // next node index to assign
    int currentCutWeight;       // cut weight so far
    int currentSizeX;           // how many vertices are in set X so far
    std::vector<bool> assigned; // current assignment state for all vertices
  };

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

  void parallelDFS(int node, int currentCutWeight, int currentSizeX,
                   std::vector<bool> assigned);
  int parallelLB(int startNode, int currentSizeX,
                 const std::vector<bool> &assigned) const;
  void dfsSequential(int node, int currentCutWeight, int currentSizeX,
                     std::vector<bool> &assigned);
  void generatePartialSolutions(int node, int currentCutWeight,
                                int currentSizeX,
                                const std::vector<bool> &assigned,
                                int frontierDepth,
                                std::vector<PartialSolution> &solutions);
};

#endif // MINCUTSOLVER_H
