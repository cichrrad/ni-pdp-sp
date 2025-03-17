#include "MinCutSolver.h"
#include <climits>
#include <condition_variable>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <mutex>
#include <omp.h>
#include <queue>
#include <thread>
#include <vector>

// --- Existing constructor ---
MinCutSolver::MinCutSolver(const Graph &g, int subsetSize)
    : graph(g), n(g.getNumVertices()), a(subsetSize), minCutWeight(INT_MAX),
      assigned(n, false), bestPartition(n, false), currentCutWeight(0),
      currentSizeX(0), recursiveCalls(0) {
  std::srand((unsigned)std::time(nullptr));
}

// --- Sequential DFS function to complete the search from a given partial state
// --- This DFS is executed entirely by one thread (no further OpenMP tasks).
void MinCutSolver::dfsSequential(int node, int currentCutWeight,
                                 int currentSizeX,
                                 std::vector<bool> &assigned) {
  // Increment recursion counter for the current thread.
  int tid = omp_get_thread_num();
  recursionCounts[tid]++;

  // Base case: all nodes assigned.
  if (node == n) {
    if (currentSizeX == a) {
      // Update global best solution (using a critical section)
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

  // Compute lower bound for the current partial solution.
  int lb = parallelLB(node, currentSizeX, assigned);
  if (currentCutWeight + lb >= minCutWeight)
    return;

  // Option 1: assign node to set X if feasible.
  if (currentSizeX < a) {
    assigned[node] = true;
    int oldCut = currentCutWeight;
    for (int i = 0; i < node; i++) {
      if (!assigned[i]) {
        currentCutWeight += graph.getEdgeWeight(i, node);
      }
    }
    dfsSequential(node + 1, currentCutWeight, currentSizeX + 1, assigned);
    // Backtrack.
    currentCutWeight = oldCut;
    assigned[node] = false;
  }

  // Option 2: assign node to set Y.
  assigned[node] = false;
  int oldCut = currentCutWeight;
  for (int i = 0; i < node; i++) {
    if (assigned[i]) {
      currentCutWeight += graph.getEdgeWeight(i, node);
    }
  }
  dfsSequential(node + 1, currentCutWeight, currentSizeX, assigned);
  currentCutWeight = oldCut;
}

// --- Master function: recursively generate partial solutions up to a given
// frontier depth --- The master thread expands the DFS tree until 'node' equals
// frontierDepth. All generated partial solutions are stored in the vector
// 'solutions'.
void MinCutSolver::generatePartialSolutions(
    int node, int currentCutWeight, int currentSizeX,
    const std::vector<bool> &assigned, int frontierDepth,
    std::vector<PartialSolution> &solutions) {
  // Stop expanding if we have reached the frontier.
  if (node == frontierDepth) {
    PartialSolution ps;
    ps.node = node;
    ps.currentCutWeight = currentCutWeight;
    ps.currentSizeX = currentSizeX;
    ps.assigned = assigned; // copy assignment vector
    solutions.push_back(ps);
    return;
  }

  // Pruning using lower bound.
  int lb = parallelLB(node, currentSizeX, assigned);
  if (currentCutWeight + lb >= minCutWeight)
    return;

  // Option 1: assign node to set X if feasible.
  if (currentSizeX < a) {
    std::vector<bool> assignedX = assigned;
    assignedX[node] = true;
    int newCutWeight = currentCutWeight;
    for (int i = 0; i < node; i++) {
      if (!assignedX[i]) {
        newCutWeight += graph.getEdgeWeight(i, node);
      }
    }
    generatePartialSolutions(node + 1, newCutWeight, currentSizeX + 1,
                             assignedX, frontierDepth, solutions);
  }

  // Option 2: assign node to set Y.
  std::vector<bool> assignedY = assigned;
  assignedY[node] = false;
  int newCutWeightY = currentCutWeight;
  for (int i = 0; i < node; i++) {
    if (assigned[i]) {
      newCutWeightY += graph.getEdgeWeight(i, node);
    }
  }
  generatePartialSolutions(node + 1, newCutWeightY, currentSizeX, assignedY,
                           frontierDepth, solutions);
}

// --- New dynamic master–slave entry point using a shared work queue ---
// This version uses a dedicated producer thread that generates partial
// solutions and pushes them into a shared, mutex-protected queue. Worker
// threads (inside an OpenMP parallel region) then pop work from the queue and
// process it.
void MinCutSolver::betterSolveParallelMSDynamic(int numRandomTries,
                                                int frontierDepth) {
  // Reset state.
  minCutWeight = INT_MAX;
  std::fill(assigned.begin(), assigned.end(), false);
  std::fill(bestPartition.begin(), bestPartition.end(), false);
  currentCutWeight = 0;
  currentSizeX = 0;
  recursiveCalls = 0;

  // 1) Run the guesstimate phase to get a good initial bound.
  guesstimate(numRandomTries);

  // Set up the dynamic work queue and synchronization primitives.
  std::queue<PartialSolution> workQueue;
  std::mutex queueMutex;
  std::condition_variable queueCV;
  bool producerDone = false;

  // Producer function: generate all partial solutions up to the given frontier
  // depth and push them into the shared work queue.
  auto producer = [&]() {
    std::vector<bool> initAssign(n, false);
    std::vector<PartialSolution> solutions;
    generatePartialSolutions(0, 0, 0, initAssign, frontierDepth, solutions);
    {
      std::lock_guard<std::mutex> lock(queueMutex);
      for (auto &sol : solutions) {
        workQueue.push(sol);
      }
    }
    queueCV.notify_all();
    producerDone = true;
  };

  // Launch the producer thread.
  std::thread producerThread(producer);

  // Initialize per-thread recursion counters.
  int maxThreads = omp_get_max_threads();
  recursionCounts.assign(maxThreads, 0);

  // Start the timer.
  startTimer();

  // Worker region: each worker thread repeatedly extracts a partial solution
  // from the work queue and processes it sequentially.
#pragma omp parallel
  {
    while (true) {
      PartialSolution ps;
      {
        std::unique_lock<std::mutex> lock(queueMutex);
        // Wait until the work queue is nonempty or the producer is done.
        queueCV.wait(lock,
                     [&]() { return !workQueue.empty() || producerDone; });
        if (workQueue.empty()) {
          // If the queue is empty and the producer is done, exit the loop.
          break;
        }
        ps = workQueue.front();
        workQueue.pop();
      }
      // Process the partial solution.
      std::vector<bool> localAssign = ps.assigned;
      dfsSequential(ps.node, ps.currentCutWeight, ps.currentSizeX, localAssign);
    }
  }

  // Stop the timer.
  stopTimer("Dynamic Master-Slave DFS");

  // Wait for the producer thread to finish.
  producerThread.join();

  // Aggregate per-thread recursion counts.
  long totalRecursionCalls = 0;
  for (auto count : recursionCounts) {
    totalRecursionCalls += count;
  }
  std::cout << "  Total Recursion Calls: " << totalRecursionCalls << "\n";
}

// --- Existing lower bound function (unchanged) ---
int MinCutSolver::parallelLB(int startNode, int currentSizeX,
                             const std::vector<bool> &assigned) const {
  int lbSum = 0;
  int remainX = a - currentSizeX;
  int remainY = (n - a) - ((startNode)-currentSizeX);
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
      costX = INT_MAX;
    }
    if (remainY > 0) {
      for (int j = 0; j < i; j++) {
        if (assigned[j]) { // j is in X
          costY += graph.getEdgeWeight(i, j);
        }
      }
    } else {
      costY = INT_MAX;
    }
    int best = (costX < costY) ? costX : costY;
    if (best == INT_MAX) {
      best = 0;
    }
    lbSum += best;
  }
  return lbSum;
}

// --- Existing functions: computeCut, startTimer, stopTimer, guesstimate ---

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
  for (int t = 0; t < numTries; t++) {
    std::vector<bool> tempAssign(n, false);
    std::vector<int> perm(n);
    for (int i = 0; i < n; i++) {
      perm[i] = i;
    }
    for (int i = 0; i < n; i++) {
      int r = i + std::rand() % (n - i);
      std::swap(perm[i], perm[r]);
    }
    for (int i = 0; i < a; i++) {
      tempAssign[perm[i]] = true;
    }
    int cutVal = computeCut(tempAssign);
    if (cutVal < minCutWeight) {
      minCutWeight = cutVal;
      bestPartition = tempAssign;
    }
  }
}

void MinCutSolver::printBestSolution() const {
  std::cout << "  Best Min-Cut Weight Found: " << minCutWeight << "\n";
}
