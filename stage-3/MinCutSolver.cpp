#include "MinCutSolver.h"
#include <climits>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <omp.h>
#include <vector>
// Define a threshold below which tasks are spawned
#define TASK_DEPTH 6

// Constructor: Initializes state and seeds the random number generator.
MinCutSolver::MinCutSolver(const Graph &g, int subsetSize)
    : graph(g), n(g.getNumVertices()), a(subsetSize), minCutWeight(INT_MAX),
      assigned(n, false), bestPartition(n, false), currentCutWeight(0),
      currentSizeX(0), recursiveCalls(0) {
  std::srand((unsigned)std::time(nullptr));
}

// New parallel DFS function that carries the state as parameters.
// Also increments a per-thread recursion counter.
void MinCutSolver::parallelDFS(int node, int currentCutWeight, int currentSizeX,
                               std::vector<bool> assigned) {
  // Increment the counter for the current thread
  int tid = omp_get_thread_num();
  recursionCounts[tid]++;

  // Base case: all nodes have been assigned
  if (node == n) {
    if (currentSizeX == a) {
      // Critical section to update the global best solution
#pragma omp critical
      {
        if (currentCutWeight < minCutWeight) {
          minCutWeight = currentCutWeight;
          bestPartition = assigned;
        }
      }
    }
    return;
  }

  // Compute lower bound for the current partial solution.
  int lb = parallelLB(node, currentSizeX, assigned);
  // Prune if the lower bound plus the current cut weight exceeds the best known
  // solution.
  if (currentCutWeight + lb >= minCutWeight) {
    return;
  }

  // Spawn tasks at the upper levels of the DFS; switch to sequential recursion
  // after TASK_DEPTH.
  if (node < TASK_DEPTH) {
    // Option 1: assign node to set X if feasible
    if (currentSizeX < a) {
      std::vector<bool> assignedX = assigned;
      assignedX[node] = true;
      int newCutWeight = currentCutWeight;
      // Update newCutWeight for assigning node to X: add weights from all nodes
      // in Y
      for (int i = 0; i < node; i++) {
        if (!assignedX[i]) {
          newCutWeight += graph.getEdgeWeight(i, node);
        }
      }
#pragma omp task firstprivate(node, newCutWeight, currentSizeX, assignedX)
      { parallelDFS(node + 1, newCutWeight, currentSizeX + 1, assignedX); }
    }

    // Option 2: assign node to set Y
    std::vector<bool> assignedY = assigned;
    assignedY[node] = false;
    int newCutWeightY = currentCutWeight;
    // Update newCutWeight for assigning node to Y: add weights from all nodes
    // in X
    for (int i = 0; i < node; i++) {
      if (assigned[i]) {
        newCutWeightY += graph.getEdgeWeight(i, node);
      }
    }
#pragma omp task firstprivate(node, newCutWeightY, currentSizeX, assignedY)
    { parallelDFS(node + 1, newCutWeightY, currentSizeX, assignedY); }

    // Wait for both tasks to finish before returning.
#pragma omp taskwait
  } else {
    // Sequential recursion beyond the threshold

    // Option 1: assign node to set X if feasible
    if (currentSizeX < a) {
      assigned[node] = true;
      int oldCut = currentCutWeight;
      for (int i = 0; i < node; i++) {
        if (!assigned[i]) {
          currentCutWeight += graph.getEdgeWeight(i, node);
        }
      }
      parallelDFS(node + 1, currentCutWeight, currentSizeX + 1, assigned);
      // Backtrack
      currentCutWeight = oldCut;
    }

    // Option 2: assign node to set Y
    assigned[node] = false;
    int oldCut = currentCutWeight;
    for (int i = 0; i < node; i++) {
      if (assigned[i]) {
        currentCutWeight += graph.getEdgeWeight(i, node);
      }
    }
    parallelDFS(node + 1, currentCutWeight, currentSizeX, assigned);
    currentCutWeight = oldCut;
  }
}

// Data-parallel lower bound calculation.
// Parallelizes the outer loop over unassigned nodes.
int MinCutSolver::parallelLB(int startNode, int currentSizeX,
                             const std::vector<bool> &assigned) const {
  int remainX = a - currentSizeX;
  int remainY = (n - a) - ((startNode)-currentSizeX);
  int lbSum = 0;

// Parallel loop with reduction to sum up the lower bound contributions.
#pragma omp parallel for reduction(+ : lbSum) schedule(dynamic)
  for (int i = startNode; i < n; i++) {
    int costX = 0;
    int costY = 0;

    if (remainX > 0) {
      for (int j = 0; j < i; j++) {
        if (!assigned[j]) { // j is in Y
          costX += graph.getEdgeWeight(i, j);
        }
      }
    } else {
      costX = INT_MAX; // cannot place i in X
    }

    if (remainY > 0) {
      for (int j = 0; j < i; j++) {
        if (assigned[j]) { // j is in X
          costY += graph.getEdgeWeight(i, j);
        }
      }
    } else {
      costY = INT_MAX; // cannot place i in Y
    }

    int best = (costX < costY) ? costX : costY;
    if (best == INT_MAX) {
      best = 0;
    }
    lbSum += best;
  }

  return lbSum;
}

// Data-parallel version of computeCut.
// Computes the total cut weight over all vertex pairs in parallel.
int MinCutSolver::computeCut(const std::vector<bool> &assignment) const {
  int cutVal = 0;
#pragma omp parallel for collapse(2) reduction(+ : cutVal) schedule(dynamic)
  for (int i = 0; i < n; i++) {
    for (int j = i + 1; j < n; j++) {
      if (assignment[i] != assignment[j]) {
        cutVal += graph.getEdgeWeight(i, j);
      }
    }
  }
  return cutVal;
}

// A new entry point for the parallel solution.
// It first runs the data-parallel guesstimate phase,
// then starts the task-parallel DFS.
void MinCutSolver::betterSolveParallel(int numRandomTries) {
  // Reset state
  minCutWeight = INT_MAX;
  std::fill(assigned.begin(), assigned.end(), false);
  std::fill(bestPartition.begin(), bestPartition.end(), false);
  currentCutWeight = 0;
  currentSizeX = 0;
  recursiveCalls = 0;

// Data-parallel guesstimate phase:
// Try numRandomTries random configurations in parallel.
#pragma omp parallel for schedule(dynamic)
  for (int t = 0; t < numRandomTries; t++) {
    std::vector<bool> tempAssign(n, false);
    std::vector<int> perm(n);
    for (int i = 0; i < n; i++) {
      perm[i] = i;
    }
    // Shuffle permutation (each iteration has its own seed based on t)
    unsigned int seed = std::time(nullptr) + t;
    for (int i = 0; i < n; i++) {
      int r = i + rand_r(&seed) % (n - i);
      std::swap(perm[i], perm[r]);
    }
    // Assign first a vertices to set X.
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

  // Initialize per-thread recursion counters.
  int maxThreads = omp_get_max_threads();
  recursionCounts.assign(maxThreads, 0);

  startTimer();
#pragma omp parallel
  {
#pragma omp single nowait
    {
      // Launch the task-parallel DFS starting from node 0.
      parallelDFS(0, 0, 0, assigned);
    }
  }
  stopTimer("");

  // Aggregate per-thread recursion counts.
  long totalRecursionCalls = 0;
  for (auto count : recursionCounts) {
    totalRecursionCalls += count;
  }
  std::cout << "  Total Recursion Calls: " << totalRecursionCalls << "\n";
}

// Print the best solution found.
void MinCutSolver::printBestSolution() const {
  std::cout << "  Best Min-Cut Weight Found: " << minCutWeight << "\n";
}

// Starts the timer for measuring execution duration.
void MinCutSolver::startTimer() {
  startTime = std::chrono::high_resolution_clock::now();
}

// Stops the timer, printing the elapsed time.
void MinCutSolver::stopTimer(const char *label) {
  auto endTime = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = endTime - startTime;

  std::cout << label << "\n";
  std::cout << "  Execution Time: " << elapsed.count() << " seconds\n";
}

// Data-parallel guesstimate phase to obtain an initial minCutWeight.
void MinCutSolver::guesstimate(int numTries) {
  // Already integrated into betterSolveParallel with a parallel loop.
  // Optionally, you can call this function separately if needed.
  // In this version, betterSolveParallel runs guesstimate in parallel.
}
