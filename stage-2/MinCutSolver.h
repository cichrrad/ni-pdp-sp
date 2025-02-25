#ifndef MINCUTSOLVER_H
#define MINCUTSOLVER_H

#include "Graph.h"
#include <chrono>
#include <vector>

class MinCutSolver {
public:
  MinCutSolver(const Graph &graph, int subsetSize);

  // 'naive' deprecated approach
  void solve();

  // improved
  void solveWithBetterBoundAndMultiRandom(int numRandomTries = 10);

  void printBestSolution() const;
  void solveParallelOMP(int numRandomTries, int cutoffDepth = 4);

private:
  // deprecated
  void dfs(int node);

  // DFS that uses an improved lower bound
  void dfsBetterLB(int node);

  void dfsBetterLB_omp(int node, int currentCut, int sizeX,
                       std::vector<bool> assign, int depth, int cutoffDepth);

  // Heuristic: multiple random feasible assignments
  void initMultipleRandomSolutions(int numTries);

  int improvedLowerBound(int startNode) const;

  // deprecated
  int naiveLowerBound(int startNode) const;

  // Helper: compute cut for a full assignment
  int computeCut(const std::vector<bool> &assignment) const;

  // Performance trackers
  void startTimer();
  void stopTimerAndReport(const char *label);

private:
  const Graph &graph;
  int n; // size (vertices)
  int a; // ratio of partitions

  int minCutWeight;
  std::vector<bool> assigned;      // partial solution (X..1 | Y..0)
  std::vector<bool> bestPartition; // duh
  int currentCutWeight;            // duh
  int currentSizeX;                // duh

  // Perf
  int recursiveCalls;
  std::chrono::high_resolution_clock::time_point startTime;
};

#endif // MINCUTSOLVER_H
