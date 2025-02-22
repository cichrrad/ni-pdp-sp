#ifndef MINCUTSOLVER_H
#define MINCUTSOLVER_H

#include "Graph.h"
#include <vector>
#include <chrono>

class MinCutSolver {
public:
    MinCutSolver(const Graph& graph, int subsetSize);

    // Original, simpler approach
    void solve();

    // Extended approach with:
    // - multiple random initial solutions
    // - improved lower bound
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
    int n;  // number of vertices
    int a;  // required size of subset X

    int minCutWeight;
    std::vector<bool> assigned;      // partial assignment (true = X, false = Y)
    std::vector<bool> bestPartition; // store best partition found
    int currentCutWeight;            // cost of partial assignment
    int currentSizeX;                // how many assigned to X so far

    // Perf
    int recursiveCalls;
    std::chrono::high_resolution_clock::time_point startTime;
};

#endif // MINCUTSOLVER_H
