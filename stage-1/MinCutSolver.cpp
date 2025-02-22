#include "MinCutSolver.h"
#include <iostream>
#include <climits>
#include <cstdlib>
#include <ctime>

MinCutSolver::MinCutSolver(const Graph& g, int subsetSize)
    : graph(g),
      n(g.getNumVertices()),
      a(subsetSize),
      minCutWeight(INT_MAX),
      assigned(n, false),
      bestPartition(n, false),
      currentCutWeight(0),
      currentSizeX(0),
      recursiveCalls(0)
{
    std::srand((unsigned)std::time(nullptr));
}

// basic solve (deprecated)
void MinCutSolver::solve() {
    minCutWeight = INT_MAX;
    std::fill(assigned.begin(), assigned.end(), false);
    std::fill(bestPartition.begin(), bestPartition.end(), false);
    currentCutWeight = 0;
    currentSizeX     = 0;
    recursiveCalls   = 0;

    startTimer();
    dfs(0); // original DFS
    stopTimerAndReport("Original DFS");
}



// solve with multiple random solutions + improved LB
void MinCutSolver::solveWithBetterBoundAndMultiRandom(int numRandomTries) {
    minCutWeight = INT_MAX;
    std::fill(assigned.begin(), assigned.end(), false);
    std::fill(bestPartition.begin(), bestPartition.end(), false);
    currentCutWeight = 0;
    currentSizeX     = 0;
    recursiveCalls   = 0;

    // 1) Get a good initial solution via multiple random tries
    initMultipleRandomSolutions(numRandomTries);

    // 2) Now do a DFS that uses the improved LB
    std::fill(assigned.begin(), assigned.end(), false);
    currentCutWeight = 0;
    currentSizeX     = 0;

    startTimer();
    dfsBetterLB(0); // DFS with improved lower bound
    stopTimerAndReport("Improved LB + Multi-Random");
}



// BB-DFS with basic lower bound estimate (deprecated)
void MinCutSolver::dfs(int node) {
    recursiveCalls++;

    // Base case
    if (node == n) {
        if (currentSizeX == a) {
            if (currentCutWeight < minCutWeight) {
                minCutWeight = currentCutWeight;
                bestPartition.assign(assigned.begin(), assigned.end());
            }
        }
        return;
    }

    if (currentCutWeight + naiveLowerBound(node) >= minCutWeight) {
        return;
    }

    // If putting node in X is feasible
    if (currentSizeX < a) {
        assigned[node] = true;
        int oldCut = currentCutWeight;

        // Update partial cut
        for (int i = 0; i < node; i++) {
            if (!assigned[i]) { // crosses partition
                currentCutWeight += graph.getEdgeWeight(i, node);
            }
        }

        currentSizeX++;
        dfs(node + 1);
        currentSizeX--;

        // backtrack
        currentCutWeight = oldCut;
    }

    // node -> Y
    assigned[node] = false;
    int oldCut = currentCutWeight;

    for (int i = 0; i < node; i++) {
        if (assigned[i]) {
            currentCutWeight += graph.getEdgeWeight(i, node);
        }
    }

    dfs(node + 1);

    // backtrack
    currentCutWeight = oldCut;
}

// BB-DFS with the better lower bound estimate 
void MinCutSolver::dfsBetterLB(int node) {
    recursiveCalls++;

    // Base case
    if (node == n) {
        // must check if we ended up with exactly a in X
        if (currentSizeX == a) {
            if (currentCutWeight < minCutWeight) {
                minCutWeight = currentCutWeight;
                bestPartition.assign(assigned.begin(), assigned.end());
            }
        }
        return;
    }

    // If partial solution is already worse than best, prune
    int lb = improvedLowerBound(node);
    if (currentCutWeight + lb >= minCutWeight) {
        return;
    }

    // Try node -> X if feasible
    if (currentSizeX < a) {
        assigned[node] = true;
        int oldCut = currentCutWeight;

        // update partial cut
        for (int i = 0; i < node; i++) {
            if (!assigned[i]) {
                currentCutWeight += graph.getEdgeWeight(i, node);
            }
        }

        currentSizeX++;
        dfsBetterLB(node + 1);
        currentSizeX--;
        currentCutWeight = oldCut;
    }

    // Try node -> Y
    assigned[node] = false;
    int oldCut = currentCutWeight;

    for (int i = 0; i < node; i++) {
        if (assigned[i]) {
            currentCutWeight += graph.getEdgeWeight(i, node);
        }
    }

    dfsBetterLB(node + 1);

    // backtrack
    currentCutWeight = oldCut;
}



// To not start from nothing, try to (gu)es(s)timate initial minCutWeight
// from [numTries] random configuration
void MinCutSolver::initMultipleRandomSolutions(int numTries) {
    for (int t = 0; t < numTries; t++) {
        // 1) create random assignment
        std::vector<bool> tempAssign(n, false);

        // pick 'a' distinct vertices for X
        std::vector<int> perm(n);
        for (int i = 0; i < n; i++) {
            perm[i] = i;
        }
        // shuffle
        for (int i = 0; i < n; i++) {
            int r = i + std::rand() % (n - i);
            std::swap(perm[i], perm[r]);
        }
        // first 'a' -> X
        for (int i = 0; i < a; i++) {
            tempAssign[perm[i]] = true;
        }

        // 2) compute cut
        int cutVal = computeCut(tempAssign);

        // 3) update best global solution if better
        if (cutVal < minCutWeight) {
            minCutWeight = cutVal;
            bestPartition = tempAssign;
        }
    }
}



// more accurate LB estimate 
// for each unassigned node i, compute cost if i->X vs. i->Y | sum the MIN over i
int MinCutSolver::improvedLowerBound(int startNode) const {
    int lbSum = 0;

    // how many slots remain for X and Y?
    int remainX = a - currentSizeX;
    int remainY = (n - a) - ( (startNode) - currentSizeX ); 

    for (int i = startNode; i < n; i++) {

        int costX = 0; 
        int costY = 0;

        if (remainX > 0) {

            for (int j = 0; j < i; j++) {
                if (assigned[j] == false) { // j is in Y
                    costX += graph.getEdgeWeight(i, j);
                }
            }
        } else {
            costX = INT_MAX; // can't place i in X
        }

        if (remainY > 0) {

            for (int j = 0; j < i; j++) {
                if (assigned[j] == true) { // j is in X
                    costY += graph.getEdgeWeight(i, j);
                }
            }
        } else {
            costY = INT_MAX; // can't place i in Y
        }

        int best = (costX < costY) ? costX : costY;
        if (best == INT_MAX) {
            // This means we can't put i in X or Y feasibly -> the branch is basically infeasible
            // But for bounding, let's just say 0 because that path won't lead to a valid solution anyway.
            best = 0;
        }

        lbSum += best;
    }

    return lbSum;
}



// basic lower bound estimate (deprecated) 
int MinCutSolver::naiveLowerBound(int startNode) const {
    int lowerBound = 0;
    for (int i = startNode; i < n; i++) {
        int minEdge = INT_MAX;
        for (int j = 0; j < n; j++) {
            if (i != j && graph.getEdgeWeight(i, j) > 0) {
                if (graph.getEdgeWeight(i, j) < minEdge) {
                    minEdge = graph.getEdgeWeight(i, j);
                }
            }
        }
        if (minEdge != INT_MAX) {
            lowerBound += minEdge;
        }
    }
    return lowerBound / 2;
}



// computes cut of a given assignment
// we sum up weights of edges whose endpoints are not within same set (duh)
int MinCutSolver::computeCut(const std::vector<bool>& assignment) const {
    int cutVal = 0;
    for (int i = 0; i < n; i++) {
        for (int j = i+1; j < n; j++) {
            if (assignment[i] != assignment[j]) {
                cutVal += graph.getEdgeWeight(i, j);
            }
        }
    }
    return cutVal;
}



void MinCutSolver::startTimer() {
    startTime = std::chrono::high_resolution_clock::now();
}



void MinCutSolver::stopTimerAndReport(const char* label) {
    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = endTime - startTime;

    std::cout << label << ":\n";
    std::cout << "  Execution Time: " << elapsed.count() << " seconds\n";
    std::cout << "  Total Recursive Calls: " << recursiveCalls << "\n\n";
}



void MinCutSolver::printBestSolution() const {
    std::cout << "Best Min-Cut Weight Found: " << minCutWeight << "\n";
    std::cout << "Partition:\nX: ";
    for (int i = 0; i < n; i++) {
        if (bestPartition[i]) {
            std::cout << i << " ";
        }
    }
    std::cout << "\nY: ";
    for (int i = 0; i < n; i++) {
        if (!bestPartition[i]) {
            std::cout << i << " ";
        }
    }
    std::cout << "\n";
}
