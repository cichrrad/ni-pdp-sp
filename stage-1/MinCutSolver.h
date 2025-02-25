#ifndef MINCUTSOLVER_H
#define MINCUTSOLVER_H

#include "Graph.h"
#include <vector>
#include <chrono>

class MinCutSolver {
public:
    MinCutSolver(const Graph& graph, int subsetSize);

    // 'naive' deprecated approach
    void solve();

    // improved
    void solveWithBetterBoundAndMultiRandom(int numRandomTries = 10);

    void printBestSolution() const;

private:
    
    //deprecated
    void dfs(int node);

    // DFS that uses an improved lower bound
    void dfsBetterLB(int node);

    // Heuristic: multiple random feasible assignments
    void initMultipleRandomSolutions(int numTries);

    int improvedLowerBound(int startNode) const;

    // deprecated
    int naiveLowerBound(int startNode) const;

    // Helper: compute cut for a full assignment
    int computeCut(const std::vector<bool>& assignment) const;

    // Performance trackers
    void startTimer();
    void stopTimerAndReport(const char* label);

private:
    const Graph& graph;
    int n;  // size (vertices)
    int a;  // ratio of partitions 

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
