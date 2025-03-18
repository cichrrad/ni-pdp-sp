#include "MinCutSolver.h"
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <omp.h>
#include <vector>

// --- Constructor ---
MinCutSolver::MinCutSolver(const Graph &g, int subsetSize)
    : graph(g), n(g.getNumVertices()), a(subsetSize), minCutWeight(INT_MAX),
      assigned(n, false), bestPartition(n, false), currentCutWeight(0),
      currentSizeX(0), recursiveCalls(0) {
  std::srand((unsigned)std::time(nullptr));
}

// --- Sequential DFS ---
// This DFS is executed entirely by one thread.
void MinCutSolver::dfsSequential(int node, int currentCutWeight,
                                 int currentSizeX,
                                 std::vector<bool> &assigned) {
  int tid = omp_get_thread_num();
  recursionCounts[tid]++;

  if (node == n) {
    if (currentSizeX == a) {
      if (currentCutWeight < minCutWeight) {
#pragma omp critical
        {
          if (currentCutWeight < minCutWeight) {
            minCutWeight = currentCutWeight;
            bestPartition = assigned;
          }
        }
      }
    }
    return;
  }

  int lb = parallelLB(node, currentSizeX, assigned);
  if (currentCutWeight + lb >= minCutWeight)
    return;

  // Option 1: assign node to set X if feasible.
  if (currentSizeX < a) {
    assigned[node] = true;
    int oldCut = currentCutWeight;
    for (int i = 0; i < node; i++) {
      if (!assigned[i])
        currentCutWeight += graph.getEdgeWeight(i, node);
    }
    dfsSequential(node + 1, currentCutWeight, currentSizeX + 1, assigned);
    currentCutWeight = oldCut;
    assigned[node] = false;
  }

  // Option 2: assign node to set Y.
  assigned[node] = false;
  int oldCut = currentCutWeight;
  for (int i = 0; i < node; i++) {
    if (assigned[i])
      currentCutWeight += graph.getEdgeWeight(i, node);
  }
  dfsSequential(node + 1, currentCutWeight, currentSizeX, assigned);
  currentCutWeight = oldCut;
}

// --- Generate Partial Solutions ---
// Expand the DFS tree up to a given frontierDepth. All generated partial
// solutions are stored in 'solutions'.
void MinCutSolver::generatePartialSolutions(
    int node, int currentCutWeight, int currentSizeX,
    const std::vector<bool> &assigned, int frontierDepth,
    std::vector<PartialSolution> &solutions) {
  if (node == frontierDepth) {
    PartialSolution ps;
    ps.node = node;
    ps.currentCutWeight = currentCutWeight;
    ps.currentSizeX = currentSizeX;
    ps.assigned = assigned; // copy assignment vector
    solutions.push_back(ps);
    return;
  }
  int lb = parallelLB(node, currentSizeX, assigned);
  if (currentCutWeight + lb >= minCutWeight)
    return;
  if (currentSizeX < a) {
    std::vector<bool> assignedX = assigned;
    assignedX[node] = true;
    int newCutWeight = currentCutWeight;
    for (int i = 0; i < node; i++) {
      if (!assignedX[i])
        newCutWeight += graph.getEdgeWeight(i, node);
    }
    generatePartialSolutions(node + 1, newCutWeight, currentSizeX + 1,
                             assignedX, frontierDepth, solutions);
  }
  std::vector<bool> assignedY = assigned;
  assignedY[node] = false;
  int newCutWeightY = currentCutWeight;
  for (int i = 0; i < node; i++) {
    if (assigned[i])
      newCutWeightY += graph.getEdgeWeight(i, node);
  }
  generatePartialSolutions(node + 1, newCutWeightY, currentSizeX, assignedY,
                           frontierDepth, solutions);
}

// --- Dynamic Master-Slave Entry Point (Static Partial Solutions) ---
// Instead of dynamically generating work with a lock-free queue,
// we generate all partial solutions up front (which is acceptable for n < 100)
// and then process them in parallel.
void MinCutSolver::betterSolveParallelMS(int numRandomTries,
                                         int frontierDepth) {
  // Reset state.
  minCutWeight = INT_MAX;
  std::fill(assigned.begin(), assigned.end(), false);
  std::fill(bestPartition.begin(), bestPartition.end(), false);
  currentCutWeight = 0;
  currentSizeX = 0;
  recursiveCalls = 0;

  // 1) Run the guesstimate phase.
  guesstimate(numRandomTries);

  // 2) Generate partial solutions up to the given frontier depth.
  std::vector<PartialSolution> partialSolutions;
  std::vector<bool> initAssign(n, false);
  generatePartialSolutions(0, 0, 0, initAssign, frontierDepth,
                           partialSolutions);
  std::cout << "Generated " << partialSolutions.size()
            << " partial solutions.\n";

  // 3) Initialize per-thread recursion counters.
  int maxThreads = omp_get_max_threads();
  recursionCounts.assign(maxThreads, 0);

  // 4) Start the timer.
  startTimer();

  // 5) Process each partial solution in parallel.
#pragma omp parallel for schedule(dynamic)
  for (size_t i = 0; i < partialSolutions.size(); i++) {
    std::vector<bool> localAssign = partialSolutions[i].assigned;
    dfsSequential(partialSolutions[i].node,
                  partialSolutions[i].currentCutWeight,
                  partialSolutions[i].currentSizeX, localAssign);
  }

  // 6) Stop the timer.
  stopTimer("Static Master-Slave DFS");

  // 7) Aggregate per-thread recursion counts.
  long totalRecursionCalls = 0;
  for (auto count : recursionCounts)
    totalRecursionCalls += count;
  std::cout << "  Total Recursion Calls: " << totalRecursionCalls << "\n";
}

// --- Optimized Parallel Lower Bound (LB) ---
// Uses OpenMP parallel for reduction. For small ranges, this might be
// sequential.
int MinCutSolver::parallelLB(int startNode, int currentSizeX,
                             const std::vector<bool> &assigned) const {
  int range = n - startNode;
  int lbSum = 0;
  // Use sequential computation for very small ranges.
  if (range < 100) {
    for (int i = startNode; i < n; i++) {
      int costX = 0, costY = 0;
      int remainX = a - currentSizeX;
      int remainY = (n - a) - ((startNode)-currentSizeX);
      if (remainX > 0) {
        for (int j = 0; j < i; j++) {
          if (!assigned[j])
            costX += graph.getEdgeWeight(i, j);
        }
      } else {
        costX = INT_MAX;
      }
      if (remainY > 0) {
        for (int j = 0; j < i; j++) {
          if (assigned[j])
            costY += graph.getEdgeWeight(i, j);
        }
      } else {
        costY = INT_MAX;
      }
      int best = (costX < costY) ? costX : costY;
      if (best == INT_MAX)
        best = 0;
      lbSum += best;
    }
    return lbSum;
  }
  // Otherwise, use parallel for reduction.
  int remainX = a - currentSizeX;
  int remainY = (n - a) - ((startNode)-currentSizeX);
#pragma omp parallel for reduction(+ : lbSum) schedule(dynamic)
  for (int i = startNode; i < n; i++) {
    int costX = 0, costY = 0;
    if (remainX > 0) {
      for (int j = 0; j < i; j++) {
        if (!assigned[j])
          costX += graph.getEdgeWeight(i, j);
      }
    } else {
      costX = INT_MAX;
    }
    if (remainY > 0) {
      for (int j = 0; j < i; j++) {
        if (assigned[j])
          costY += graph.getEdgeWeight(i, j);
      }
    } else {
      costY = INT_MAX;
    }
    int best = (costX < costY) ? costX : costY;
    if (best == INT_MAX)
      best = 0;
    lbSum += best;
  }
  return lbSum;
}

int MinCutSolver::computeCut(const std::vector<bool> &assignment) const {
  int cutVal = 0;
  for (int i = 0; i < n; i++) {
    for (int j = i + 1; j < n; j++) {
      if (assignment[i] != assignment[j])
        cutVal += graph.getEdgeWeight(i, j);
    }
  }
  return cutVal;
}

void MinCutSolver::startTimer() {
  startTime = std::chrono::high_resolution_clock::now();
}

void MinCutSolver::stopTimer(const char *label) {
  auto endTime = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = endTime - startTime;
  std::cout << label << "\n";
  std::cout << "  Execution Time: " << elapsed.count() << " seconds\n";
}

void MinCutSolver::guesstimate(int numTries) {
#pragma omp parallel for schedule(dynamic)
  for (int t = 0; t < numTries; t++) {
    std::vector<bool> tempAssign(n, false);
    std::vector<int> perm(n);
    for (int i = 0; i < n; i++) {
      perm[i] = i;
    }
    unsigned int seed = std::time(nullptr) + t;
    for (int i = 0; i < n; i++) {
      int r = i + rand_r(&seed) % (n - i);
      std::swap(perm[i], perm[r]);
    }
    for (int i = 0; i < a; i++) {
      tempAssign[perm[i]] = true;
    }
    int cutVal = computeCut(tempAssign);
#pragma omp critical
    {
      if (cutVal < minCutWeight) {
        minCutWeight = cutVal;
        bestPartition = tempAssign;
      }
    }
  }
}

void MinCutSolver::printBestSolution() const {
  std::cout << "  Best Min-Cut Weight Found: " << minCutWeight << "\n";
}
