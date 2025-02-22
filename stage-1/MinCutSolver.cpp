#include "MinCutSolver.h"
#include <iostream>
#include <climits>

MinCutSolver::MinCutSolver(const Graph& g, int subsetSize)
    : graph(g), n(g.getNumVertices()), a(subsetSize), minCutWeight(INT_MAX),
      assigned(n, false), bestPartition(n, 0),
      currentCutWeight(0), currentSizeX(0), recursiveCalls(0) {}

void MinCutSolver::solve() {
    recursiveCalls = 0;
    startTime = std::chrono::high_resolution_clock::now();  // Time Start 

    dfs(0);  // Start DFS

    auto endTime = std::chrono::high_resolution_clock::now();  // Time Stop
    std::chrono::duration<double> elapsed = endTime - startTime;

    std::cout << "Execution Time: " << elapsed.count() << " seconds\n";
    std::cout << "Total Recursive Calls: " << recursiveCalls << "\n";
}

void MinCutSolver::dfs(int node) {
    recursiveCalls++; 

    if (node == n) {
        if (currentSizeX == a) {
            if (currentCutWeight < minCutWeight) {
                minCutWeight = currentCutWeight;
                bestPartition.assign(assigned.begin(), assigned.end());
            }
        }
        return;
    }

    // Prune branch if the current cut + lower bound estimate is worse than the best found cut
    if (currentCutWeight + calculateLowerBound(node) >= minCutWeight) return;

    // Try assigning the node to X
    if (currentSizeX < a) {
        assigned[node] = true;
        int oldCutWeight = currentCutWeight;

        // Update the cut weight for new assignment
        for (int i = 0; i < node; i++) {
            if (!assigned[i]) currentCutWeight += graph.getEdgeWeight(i, node);
        }

        currentSizeX++;
        dfs(node + 1);
        currentSizeX--;

        // Backtrack
        currentCutWeight = oldCutWeight;
    }

    // Try assigning the node to Y
    assigned[node] = false;
    int oldCutWeight = currentCutWeight;

    // Update the cut weight for new assignment
    for (int i = 0; i < node; i++) {
        if (assigned[i]) currentCutWeight += graph.getEdgeWeight(i, node);
    }

    dfs(node + 1);

    // Backtrack
    currentCutWeight = oldCutWeight;
}

int MinCutSolver::calculateLowerBound(int node) const {
    int lowerBound = 0;
    for (int i = node; i < n; i++) {
        int minEdge = INT_MAX;
        for (int j = 0; j < n; j++) {
            if (i != j && graph.getEdgeWeight(i, j) > 0) {
                minEdge = std::min(minEdge, graph.getEdgeWeight(i, j));
            }
        }
        if (minEdge != INT_MAX) {
            lowerBound += minEdge;
        }
    }
    return lowerBound / 2;  // Approximate remaining cut cost
}

void MinCutSolver::printBestSolution() const {
    std::cout << "Minimum cut weight: " << minCutWeight << "\n";
    std::cout << "Partition:\nX: ";
    for (int i = 0; i < n; i++) {
        if (bestPartition[i]) std::cout << i << " ";
    }
    std::cout << "\nY: ";
    for (int i = 0; i < n; i++) {
        if (!bestPartition[i]) std::cout << i << " ";
    }
    std::cout << "\n";
}
