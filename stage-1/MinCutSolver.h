#ifndef MINCUTSOLVER_H
#define MINCUTSOLVER_H

#include "Graph.h"
#include <vector>
#include <chrono>  // For measuring execution time

class MinCutSolver {
public:
    MinCutSolver(const Graph& graph, int subsetSize);

    void solve();
    void printBestSolution() const;

private:
    void dfs(int node);
    int calculateLowerBound(int node) const;

    const Graph& graph;
    int n;
    int a;
    int minCutWeight;

    std::vector<bool> assigned;  // true = X, false = Y
    std::vector<bool> bestPartition;
    int currentCutWeight;
    int currentSizeX;

    // Performance tracking
    int recursiveCalls;
    std::chrono::high_resolution_clock::time_point startTime;
};

#endif // MINCUTSOLVER_H
