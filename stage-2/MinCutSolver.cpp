#include "MinCutSolver.h"
#include <climits>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <omp.h>

// Adjust the threshold for task creation.
// Here we create new tasks only if the remaining nodes exceed this threshold.
static const int TASK_CREATION_THRESHOLD = 30;

MinCutSolver::MinCutSolver(const Graph &g, int subsetSize)
    : graph(g), n(g.getNumVertices()), a(subsetSize), minCutWeight(INT_MAX),
      assigned(n, false), bestPartition(n, false), currentCutWeight(0),
      currentSizeX(0), recursiveCalls(0) {
  std::srand((unsigned)std::time(nullptr));
}

// Computes a lower bound for the remaining nodes based on current DFS state.
int MinCutSolver::parallelBetterLowerBound(int startNode,
                                           const std::vector<bool> &assigned,
                                           int currentSizeX) const {
  int lbSum = 0;
  // Calculate remaining slots for set X and Y.
  int remainX = a - currentSizeX;
  int remainY = (n - a) - (startNode - currentSizeX);

  for (int i = startNode; i < n; i++) {
    int costX = 0;
    int costY = 0;

    if (remainX > 0) {
      for (int j = 0; j < i; j++) {
        if (!assigned[j]) { // j is in Y, so adding i to X incurs this cost.
          costX += graph.getEdgeWeight(i, j);
        }
      }
    } else {
      costX = INT_MAX; // cannot place i in X.
    }

    if (remainY > 0) {
      for (int j = 0; j < i; j++) {
        if (assigned[j]) { // j is in X, so adding i to Y incurs this cost.
          costY += graph.getEdgeWeight(i, j);
        }
      }
    } else {
      costY = INT_MAX; // cannot place i in Y.
    }

    int best = (costX < costY) ? costX : costY;
    if (best == INT_MAX)
      best = 0;
    lbSum += best;
  }
  return lbSum;
}

// Parallel DFS using OpenMP tasks with threshold control.
void MinCutSolver::parallelDfs(State state) {
// Use an atomic update for recursive call count.
#pragma omp atomic
  recursiveCalls++;

  // Base case: all nodes have been processed.
  if (state.node == n) {
    // Only update global best if we have exactly 'a' nodes in set X.
    if (state.currentSizeX == a) {
#pragma omp critical
      {
        if (state.currentCutWeight < minCutWeight) {
          minCutWeight = state.currentCutWeight;
          bestPartition = state.assigned;
        }
      }
    }
    return;
  }

  // Compute lower bound for the remaining part of the DFS.
  int lb =
      parallelBetterLowerBound(state.node, state.assigned, state.currentSizeX);
  if (state.currentCutWeight + lb >= minCutWeight)
    return;

  // Branch 1: Try assigning the current node to set X (if room available).
  if (state.currentSizeX < a) {
    State stateX = state; // Create a copy of the current state.
    stateX.assigned[state.node] = true;
    // Update cut weight: For each processed node in Y, adding current node to X
    // adds cost.
    for (int i = 0; i < stateX.node; i++) {
      if (!stateX.assigned[i]) {
        stateX.currentCutWeight += graph.getEdgeWeight(i, state.node);
      }
    }
    stateX.currentSizeX++;
    stateX.node++;
    // Spawn a new task only if enough work remains.
    if ((n - state.node) > TASK_CREATION_THRESHOLD) {
#pragma omp task firstprivate(stateX)
      { parallelDfs(stateX); }
    } else {
      parallelDfs(stateX);
    }
  }

  // Branch 2: Try assigning the current node to set Y.
  {
    State stateY = state; // Copy state.
    stateY.assigned[state.node] = false;
    // Update cut weight: For each processed node in X, adding current node to Y
    // adds cost.
    for (int i = 0; i < stateY.node; i++) {
      if (stateY.assigned[i]) {
        stateY.currentCutWeight += graph.getEdgeWeight(i, state.node);
      }
    }
    stateY.node++;
    if ((n - state.node) > TASK_CREATION_THRESHOLD) {
#pragma omp task firstprivate(stateY)
      { parallelDfs(stateY); }
    } else {
      parallelDfs(stateY);
    }
  }

// Wait for tasks spawned in this function to complete.
#pragma omp taskwait
}

// The improved solve function that runs the parallel DFS.
void MinCutSolver::betterSolve(int numRandomTries) {
  // Reset global variables.
  minCutWeight = INT_MAX;
  std::fill(assigned.begin(), assigned.end(), false);
  std::fill(bestPartition.begin(), bestPartition.end(), false);
  currentCutWeight = 0;
  currentSizeX = 0;
  recursiveCalls = 0;

  // 1) Obtain an initial solution with multiple random tries (sequentially).
  guesstimate(numRandomTries);

  // Prepare the initial DFS state.
  State initState;
  initState.node = 0;
  initState.currentSizeX = 0;
  initState.currentCutWeight = 0;
  initState.assigned.assign(n, false);

  startTimer();
// Launch the parallel region with a single initial task.
#pragma omp parallel
  {
#pragma omp single nowait
    { parallelDfs(initState); }
  }
  stopTimer("");
}

// Generates a good initial solution by trying random assignments.
void MinCutSolver::guesstimate(int numTries) {
  for (int t = 0; t < numTries; t++) {
    // Create a random assignment.
    std::vector<bool> tempAssign(n, false);
    std::vector<int> perm(n);
    for (int i = 0; i < n; i++) {
      perm[i] = i;
    }
    // Shuffle the permutation.
    for (int i = 0; i < n; i++) {
      int r = i + std::rand() % (n - i);
      std::swap(perm[i], perm[r]);
    }
    // Select the first 'a' vertices for set X.
    for (int i = 0; i < a; i++) {
      tempAssign[perm[i]] = true;
    }

    // Compute the cut value.
    int cutVal = computeCut(tempAssign);

    // Update the best global solution if this is better.
    if (cutVal < minCutWeight) {
      minCutWeight = cutVal;
      bestPartition = tempAssign;
    }
  }
}

void MinCutSolver::startTimer() {
  startTime = std::chrono::high_resolution_clock::now();
}

void MinCutSolver::stopTimer(const char *label) {
  auto endTime = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = endTime - startTime;
  std::cout << (label ? label : "") << "\n";
  std::cout << "  Execution Time: " << elapsed.count() << " seconds\n";
  std::cout << "  Total Recursive Calls: " << recursiveCalls << "\n\n";
}

void MinCutSolver::printBestSolution() const {
  std::cout << "  Best Min-Cut Weight Found: " << minCutWeight << "\n\n";
}

int MinCutSolver::computeCut(const std::vector<bool> &assignment) const {
  int cutVal = 0;
  for (int i = 0; i < n; i++) {
    for (int j = i + 1; j < n; j++) {
      if (assignment[i] != assignment[j]) {
        cutVal += graph.getEdgeWeight(i, j);
      }
    }
  }
  return cutVal;
}
