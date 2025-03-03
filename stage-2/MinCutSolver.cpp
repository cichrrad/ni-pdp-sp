#include "MinCutSolver.h"
#include <climits>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <omp.h>
#include <queue>

MinCutSolver::MinCutSolver(const Graph &g, int subsetSize)
    : graph(g), n(g.getNumVertices()), a(subsetSize), minCutWeight(INT_MAX),
      assigned(n, false), bestPartition(n, false), currentCutWeight(0),
      currentSizeX(0), recursiveCalls(0) {
  std::srand((unsigned)std::time(nullptr));
}

// basic solve (deprecated)
void MinCutSolver::solve() {
  minCutWeight = INT_MAX;
  std::fill(assigned.begin(), assigned.end(), false);
  std::fill(bestPartition.begin(), bestPartition.end(), false);
  currentCutWeight = 0;
  currentSizeX = 0;
  recursiveCalls = 0;

  startTimer();
  dfs(0); // plain DFS
  stopTimer("plain DFS");
}

// solve with multiple random solutions + improved LB using master-slave
// parallelism
void MinCutSolver::betterSolve(int numRandomTries) {
  minCutWeight = INT_MAX;
  std::fill(assigned.begin(), assigned.end(), false);
  std::fill(bestPartition.begin(), bestPartition.end(), false);
  currentCutWeight = 0;
  currentSizeX = 0;
  recursiveCalls = 0;

  // 1) Get a good initial solution via multiple random tries.
  guesstimate(numRandomTries);

  startTimer();

  // 2) Build an initial task queue using the master thread.
  // We'll expand the DFS tree to a fixed depth D.
  const int D = 4;
  std::queue<State> taskQueue;
  // Create the initial state.
  State init;
  init.node = 0;
  init.currentSizeX = 0;
  init.currentCutWeight = 0;
  init.assigned = std::vector<bool>(n, false);
  taskQueue.push(init);

  // Expand states until reaching depth D.
  std::queue<State> initialTasks;
  while (!taskQueue.empty()) {
    State s = taskQueue.front();
    taskQueue.pop();
    if (s.node < D) {
      // Branch: assign node -> X if feasible.
      if (s.currentSizeX < a) {
        State s1 = s;
        s1.assigned[s.node] = true;
        for (int i = 0; i < s.node; i++) {
          if (!s1.assigned[i]) {
            s1.currentCutWeight += graph.getEdgeWeight(i, s.node);
          }
        }
        s1.currentSizeX++;
        s1.node = s.node + 1;
        taskQueue.push(s1);
      }
      // Branch: assign node -> Y.
      State s2 = s;
      s2.assigned[s.node] = false;
      for (int i = 0; i < s.node; i++) {
        if (s2.assigned[i]) {
          s2.currentCutWeight += graph.getEdgeWeight(i, s.node);
        }
      }
      s2.node = s.node + 1;
      taskQueue.push(s2);
    } else {
      // Once depth D is reached, add the state to the initial tasks.
      initialTasks.push(s);
    }
  }

// 3) Process tasks in parallel (slave workers).
#pragma omp parallel
  {
    while (true) {
      State currentTask;
      bool gotTask = false;
// Critical section to safely pop a task.
#pragma omp critical
      {
        if (!initialTasks.empty()) {
          currentTask = initialTasks.front();
          initialTasks.pop();
          gotTask = true;
        }
      }
      if (!gotTask)
        break; // No more tasks: exit loop.
      processState(currentTask);
    }
  }

  stopTimer("");
}

// Sequential DFS with improved lower bound that processes a given state.
void MinCutSolver::processState(State s) {
// Update global recursive call count atomically.
#pragma omp atomic
  recursiveCalls++;

  // Base case: if we've assigned all nodes.
  if (s.node == n) {
    if (s.currentSizeX == a) {
// Update global best if a better cut is found.
#pragma omp critical
      {
        if (s.currentCutWeight < minCutWeight) {
          minCutWeight = s.currentCutWeight;
          bestPartition = s.assigned;
        }
      }
    }
    return;
  }

  // Compute a lower bound for the current partial solution.
  int lb = betterLowerBoundState(s, s.node);
  if (s.currentCutWeight + lb >= minCutWeight)
    return;

  // Branch 1: assign node -> X if feasible.
  if (s.currentSizeX < a) {
    State s1 = s; // Copy current state.
    s1.assigned[s.node] = true;
    for (int i = 0; i < s.node; i++) {
      if (!s1.assigned[i]) {
        s1.currentCutWeight += graph.getEdgeWeight(i, s.node);
      }
    }
    s1.currentSizeX++;
    s1.node = s.node + 1;
    processState(s1);
  }

  // Branch 2: assign node -> Y.
  State s2 = s; // Copy current state.
  s2.assigned[s.node] = false;
  for (int i = 0; i < s.node; i++) {
    if (s2.assigned[i]) {
      s2.currentCutWeight += graph.getEdgeWeight(i, s.node);
    }
  }
  s2.node = s.node + 1;
  processState(s2);
}

// Helper: state-based version of betterLowerBound.
// It works like the original betterLowerBound but uses a State.
int MinCutSolver::betterLowerBoundState(const State &s, int startNode) const {
  int lbSum = 0;
  int remainX = a - s.currentSizeX;
  int remainY = (n - a) - ((startNode)-s.currentSizeX);

  for (int i = startNode; i < n; i++) {
    int costX = 0;
    int costY = 0;
    if (remainX > 0) {
      for (int j = 0; j < i; j++) {
        if (!s.assigned[j]) {
          costX += graph.getEdgeWeight(i, j);
        }
      }
    } else {
      costX = INT_MAX;
    }
    if (remainY > 0) {
      for (int j = 0; j < i; j++) {
        if (s.assigned[j]) {
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

// BB-DFS with basic lower bound estimate (deprecated)
void MinCutSolver::dfs(int node) {
  recursiveCalls++;

  // Base case.
  if (node == n) {
    if (currentSizeX == a) {
      if (currentCutWeight < minCutWeight) {
        minCutWeight = currentCutWeight;
        bestPartition.assign(assigned.begin(), assigned.end());
      }
    }
    return;
  }

  if (currentCutWeight + LowerBound(node) >= minCutWeight) {
    return;
  }

  // If putting node in X is feasible.
  if (currentSizeX < a) {
    assigned[node] = true;
    int oldCut = currentCutWeight;
    for (int i = 0; i < node; i++) {
      if (!assigned[i]) {
        currentCutWeight += graph.getEdgeWeight(i, node);
      }
    }
    currentSizeX++;
    dfs(node + 1);
    currentSizeX--;
    currentCutWeight = oldCut;
  }

  // Try node -> Y.
  assigned[node] = false;
  int oldCut = currentCutWeight;
  for (int i = 0; i < node; i++) {
    if (assigned[i]) {
      currentCutWeight += graph.getEdgeWeight(i, node);
    }
  }
  dfs(node + 1);
  currentCutWeight = oldCut;
}

// BB-DFS with the better lower bound estimate (sequential version, deprecated
// by master-slave model)
void MinCutSolver::betterDfs(int node) {
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

  int lb = betterLowerBound(node);
  if (currentCutWeight + lb >= minCutWeight) {
    return;
  }

  if (currentSizeX < a) {
    assigned[node] = true;
    int oldCut = currentCutWeight;
    for (int i = 0; i < node; i++) {
      if (!assigned[i]) {
        currentCutWeight += graph.getEdgeWeight(i, node);
      }
    }
    currentSizeX++;
    betterDfs(node + 1);
    currentSizeX--;
    currentCutWeight = oldCut;
  }

  assigned[node] = false;
  int oldCut2 = currentCutWeight;
  for (int i = 0; i < node; i++) {
    if (assigned[i]) {
      currentCutWeight += graph.getEdgeWeight(i, node);
    }
  }
  betterDfs(node + 1);
  currentCutWeight = oldCut2;
}

// To not start from nothing, try to (gu)es(s)timate initial minCutWeight
// from [numTries] random configurations.
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

// More accurate LB estimate: for each unassigned node i, compute cost if i->X
// vs. i->Y.
int MinCutSolver::betterLowerBound(int startNode) const {
  int lbSum = 0;
  int remainX = a - currentSizeX;
  int remainY = (n - a) - ((startNode)-currentSizeX);

  for (int i = startNode; i < n; i++) {
    int costX = 0;
    int costY = 0;
    if (remainX > 0) {
      for (int j = 0; j < i; j++) {
        if (assigned[j] == false) {
          costX += graph.getEdgeWeight(i, j);
        }
      }
    } else {
      costX = INT_MAX;
    }
    if (remainY > 0) {
      for (int j = 0; j < i; j++) {
        if (assigned[j] == true) {
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

// Basic lower bound estimate (deprecated).
int MinCutSolver::LowerBound(int startNode) const {
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

// Computes the cut of a given assignment.
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
  std::cout << "  Total Recursive Calls: " << recursiveCalls << "\n\n";
}

void MinCutSolver::printBestSolution() const {
  std::cout << "  Best Min-Cut Weight Found: " << minCutWeight << "\n";
  std::cout << "\n";
}
