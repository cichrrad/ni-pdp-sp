#include "MinCutSolver.h"
#include <climits>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <omp.h> // ADD THIS

// We'll use a mutex or critical sections to update shared data:
#pragma omp declare reduction(min_int                                          \
:int : omp_out = std::min(omp_out, omp_in)) initializer(omp_priv = INT_MAX)

// constructor unchanged
MinCutSolver::MinCutSolver(const Graph &g, int subsetSize)
    : graph(g), n(g.getNumVertices()), a(subsetSize), minCutWeight(INT_MAX),
      assigned(n, false), bestPartition(n, false), currentCutWeight(0),
      currentSizeX(0), recursiveCalls(0) {
  std::srand((unsigned)std::time(nullptr));
}

// -----------------------------------------------------------------------
// Example new method that does parallel DFS with OpenMP tasks
// -----------------------------------------------------------------------
void MinCutSolver::solveParallelOMP(int numRandomTries, int cutoffDepth) {
  // 1) As before, get a good initial solution
  minCutWeight = INT_MAX;
  recursiveCalls = 0;
  initMultipleRandomSolutions(numRandomTries);

  // 2) Now run parallel version of DFS with tasks
  std::fill(assigned.begin(), assigned.end(), false);
  currentCutWeight = 0;
  currentSizeX = 0;

  startTimer();

// We'll run the parallel region around a single "master" invocation
// that spawns tasks
#pragma omp parallel
  {
// Only one thread enters "single"
#pragma omp single
    {
      // pass partial state by value/copy
      dfsBetterLB_omp(0, /* currentCut = */ 0,
                      /* sizeX = */ 0, assigned, /* copy of assigned vector */
                      /* depth= */ 0, cutoffDepth);
    }
  }

  stopTimerAndReport("Parallel OMP DFS");
}

void MinCutSolver::printBestSolution() const {
  std::cout << "  Best Min-Cut Weight Found: " << minCutWeight << "\n";
  // std::cout << "Partition:\nX: ";
  // for (int i = 0; i < n; i++) {
  //     if (bestPartition[i]) {
  //         std::cout << i << " ";
  //     }
  // }
  // std::cout << "\nY: ";
  // for (int i = 0; i < n; i++) {
  //     if (!bestPartition[i]) {
  //         std::cout << i << " ";
  //     }
  // }
  std::cout << "\n";
}

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

void MinCutSolver::startTimer() {
  startTime = std::chrono::high_resolution_clock::now();
}

int MinCutSolver::improvedLowerBound(int startNode) const {
  int lbSum = 0;

  // how many slots remain for X and Y?
  int remainX = a - currentSizeX;
  int remainY = (n - a) - ((startNode)-currentSizeX);

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
      // This means we can't put i in X or Y feasibly -> the branch is basically
      // infeasible But for bounding, let's just say 0 because that path won't
      // lead to a valid solution anyway.
      best = 0;
    }

    lbSum += best;
  }

  return lbSum;
}

void MinCutSolver::stopTimerAndReport(const char *label) {
  auto endTime = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = endTime - startTime;

  std::cout << label << "\n";
  std::cout << "  Execution Time: " << elapsed.count() << " seconds\n";
  std::cout << "  Total Recursive Calls: " << recursiveCalls << "\n\n";
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

// This version is similar to dfsBetterLB, but each call can spawn tasks
// for the next level if depth < cutoffDepth
void MinCutSolver::dfsBetterLB_omp(int node, int currentCut, int sizeX,
                                   std::vector<bool> assign, int depth,
                                   int cutoffDepth) {
// increment recursion calls atomically
#pragma omp atomic
  recursiveCalls++;

  // base case
  if (node == n) {
    if (sizeX == a) {
      // potential update of minCutWeight + bestPartition
      // must be done in a critical section or atomic
      if (currentCut < minCutWeight) {
#pragma omp critical
        {
          if (currentCut < minCutWeight) {
            minCutWeight = currentCut;
            bestPartition = assign;
          }
        }
      }
    }
    return;
  }

  // bounding
  int lb = improvedLowerBound(
      node); // same usage, but watch out for 'assign' usage inside it
  if (currentCut + lb >= minCutWeight) {
    return;
  }

  // We'll create up to 2 tasks (one for "node -> X" and one for "node -> Y")
  // if we haven't hit cutoffDepth, otherwise do sequential calls.

  bool spawnTasks = (depth < cutoffDepth);

  // Try node -> X
  if (sizeX < a) {
    // copy partial state for "node -> X"
    std::vector<bool> assignX = assign;
    int cutX = currentCut;
    int sizeX2 = sizeX;

    assignX[node] = true;
    // update partial cut
    for (int i = 0; i < node; i++) {
      if (!assignX[i]) {
        cutX += graph.getEdgeWeight(i, node);
      }
    }
    sizeX2++;

    if (spawnTasks) {
#pragma omp task firstprivate(assignX, cutX, sizeX2)                           \
    shared(minCutWeight, bestPartition)
      {
        dfsBetterLB_omp(node + 1, cutX, sizeX2, assignX, depth + 1,
                        cutoffDepth);
      }
    } else {
      // do sequential call
      dfsBetterLB_omp(node + 1, cutX, sizeX2, assignX, depth + 1, cutoffDepth);
    }
  }

  // Try node -> Y
  {
    std::vector<bool> assignY = assign;
    int cutY = currentCut;
    int sizeY2 = sizeX; // sizeX remains the same if we put node in Y

    assignY[node] = false;
    for (int i = 0; i < node; i++) {
      if (assignY[i]) {
        cutY += graph.getEdgeWeight(i, node);
      }
    }

    if (spawnTasks) {
#pragma omp task firstprivate(assignY, cutY, sizeY2)                           \
    shared(minCutWeight, bestPartition)
      {
        dfsBetterLB_omp(node + 1, cutY, sizeY2, assignY, depth + 1,
                        cutoffDepth);
      }
    } else {
      // sequential
      dfsBetterLB_omp(node + 1, cutY, sizeY2, assignY, depth + 1, cutoffDepth);
    }
  }

  // Wait for child tasks at this node to finish before returning
  // This ensures correctness at each branching level
  if (spawnTasks) {
#pragma omp taskwait
  }
}
