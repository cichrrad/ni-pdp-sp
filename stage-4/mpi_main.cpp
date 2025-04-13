// main.cpp
#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mpi.h>
#include <omp.h>
#include <queue>
#include <stdexcept>
#include <vector>

//---------------------------------------------------------------------
// Graph class with a flattened adjacency matrix.
class Graph {
public:
  int n;                   // number of vertices
  std::vector<int> matrix; // flattened adjacency matrix (row-major)

  // Construct from file.
  Graph(const std::string &filename) {
    std::ifstream fin(filename);
    if (!fin)
      throw std::runtime_error("Cannot open file " + filename);
    fin >> n;
    if (n <= 0)
      throw std::runtime_error("Invalid number of vertices");
    matrix.resize(n * n, 0);
    for (int i = 0; i < n * n; i++) {
      fin >> matrix[i];
    }
    // Optionally, you can include your heuristic reordering here.
  }

  // Construct from provided flattened matrix (for MPI broadcast).
  Graph(int n_, const std::vector<int> &flatMatrix)
      : n(n_), matrix(flatMatrix) {}

  int getEdgeWeight(int i, int j) const { return matrix[i * n + j]; }
};

//---------------------------------------------------------------------
// PartialSolution used for task distribution.
struct PartialSolution {
  int node;             // next node to assign
  int currentCutWeight; // cut weight computed so far
  int currentSizeX;     // number of vertices assigned to partition X
  std::vector<bool>
      assigned; // assignment vector for all vertices (true means in X)
};

// Utility: convert vector<bool> to vector<int> (1 for true, 0 for false)
std::vector<int> boolVectorToIntVector(const std::vector<bool> &v) {
  std::vector<int> res(v.size(), 0);
  for (size_t i = 0; i < v.size(); i++)
    res[i] = v[i] ? 1 : 0;
  return res;
}

// Utility: convert vector<int> to vector<bool>
std::vector<bool> intVectorToBoolVector(const std::vector<int> &v) {
  std::vector<bool> res(v.size(), false);
  for (size_t i = 0; i < v.size(); i++)
    res[i] = (v[i] != 0);
  return res;
}

//---------------------------------------------------------------------
// MinCutSolver implements the DFS-based branch-and-bound procedure.
class MinCutSolver {
public:
  const Graph &graph;
  int n;            // number of vertices
  int a;            // required size for partition X
  int minCutWeight; // current best cut weight found (used in bounding)
  std::vector<bool> bestPartition; // best complete assignment (partition)

  MinCutSolver(const Graph &g, int subsetSize)
      : graph(g), n(g.n), a(subsetSize), minCutWeight(INT_MAX),
        bestPartition(g.n, false) {}

  // Compute the cut weight for a full assignment.
  int computeCut(const std::vector<bool> &assignment) const {
    int cutVal = 0;
    for (int i = 0; i < n; i++) {
      for (int j = i + 1; j < n; j++) {
        if (assignment[i] != assignment[j])
          cutVal += graph.getEdgeWeight(i, j);
      }
    }
    return cutVal;
  }

  // A simple lower bound estimation from 'node' onward.
  int lowerBound(int node, int currentSizeX,
                 const std::vector<bool> &assigned) const {
    int lbSum = 0;
    for (int i = node; i < n; i++) {
      int costX = 0, costY = 0;
      int remainX = a - currentSizeX;
      int remainY = (n - a) - (i - currentSizeX);
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
      int best = (costX < costY ? costX : costY);
      if (best == INT_MAX)
        best = 0;
      lbSum += best;
    }
    return lbSum;
  }

  // DFS-based branch-and-bound from a partial state.
  // This function updates localBestCut and localBestPartition if a complete
  // solution is found.
  void dfsSequential(int node, int currentCutWeight, int currentSizeX,
                     std::vector<bool> &assigned, int &localBestCut,
                     std::vector<bool> &localBestPartition, long &recCalls) {
    recCalls++;
    if (node == n) {
      if (currentSizeX == a && currentCutWeight < localBestCut) {
        localBestCut = currentCutWeight;
        localBestPartition = assigned;
      }
      return;
    }
    int lb = lowerBound(node, currentSizeX, assigned);
    if (currentCutWeight + lb >= localBestCut)
      return;

    // Option 1: assign node to set X (if feasible)
    if (currentSizeX < a) {
      assigned[node] = true;
      int oldCut = currentCutWeight;
      for (int i = 0; i < node; i++) {
        if (!assigned[i])
          currentCutWeight += graph.getEdgeWeight(i, node);
      }
      dfsSequential(node + 1, currentCutWeight, currentSizeX + 1, assigned,
                    localBestCut, localBestPartition, recCalls);
      currentCutWeight = oldCut;
    }
    // Option 2: assign node to set Y.
    assigned[node] = false;
    int oldCut = currentCutWeight;
    for (int i = 0; i < node; i++) {
      if (assigned[i])
        currentCutWeight += graph.getEdgeWeight(i, node);
    }
    dfsSequential(node + 1, currentCutWeight, currentSizeX, assigned,
                  localBestCut, localBestPartition, recCalls);
    currentCutWeight = oldCut;
  }

  // Generate partial solutions (tasks) up to a given frontier depth.
  void generatePartialSolutions(int node, int currentCutWeight,
                                int currentSizeX,
                                const std::vector<bool> &assigned,
                                int frontierDepth,
                                std::vector<PartialSolution> &solutions) {
    if (node == frontierDepth) {
      PartialSolution ps;
      ps.node = node;
      ps.currentCutWeight = currentCutWeight;
      ps.currentSizeX = currentSizeX;
      ps.assigned = assigned; // copy
      solutions.push_back(ps);
      return;
    }
    int lb = lowerBound(node, currentSizeX, assigned);
    if (currentCutWeight + lb >= minCutWeight)
      return;
    // Branch: assign node to X (if possible)
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
    // Branch: assign node to Y.
    std::vector<bool> assignedY = assigned;
    assignedY[node] = false;
    int newCutWeight = currentCutWeight;
    for (int i = 0; i < node; i++) {
      if (assigned[i])
        newCutWeight += graph.getEdgeWeight(i, node);
    }
    generatePartialSolutions(node + 1, newCutWeight, currentSizeX, assignedY,
                             frontierDepth, solutions);
  }
};

//---------------------------------------------------------------------
// Main: MPI master-worker solver that distributes DFS tasks.
int main(int argc, char *argv[]) {
  MPI_Init(&argc, &argv);

  int rank, nProcs;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nProcs);

  if (argc < 3) {
    if (rank == 0)
      std::cerr << "Usage: " << argv[0] << " <graph_file> <subset_size>\n";
    MPI_Finalize();
    return 1;
  }

  int subsetSize = std::stoi(argv[2]);
  int n; // number of vertices
  std::vector<int> flatMatrix;
  Graph *graph = nullptr;

  if (rank == 0) {
    // Master reads the graph file.
    std::ifstream fin(argv[1]);
    if (!fin) {
      std::cerr << "Error opening file " << argv[1] << "\n";
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
    fin >> n;
    flatMatrix.resize(n * n);
    for (int i = 0; i < n * n; i++) {
      fin >> flatMatrix[i];
    }
  }
  // Broadcast n and the graph data to all processes.
  MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
  if (rank != 0) {
    flatMatrix.resize(n * n);
  }
  MPI_Bcast(flatMatrix.data(), n * n, MPI_INT, 0, MPI_COMM_WORLD);
  graph = new Graph(n, flatMatrix);

  // Decide on a frontier depth for generating partial states.
  int frontierDepth = std::min(subsetSize, 16);

  if (rank == 0) {
    // MASTER PROCESS
    MinCutSolver solver(*graph, subsetSize);
    std::vector<bool> initAssign(n, false);
    std::vector<PartialSolution> tasks;
    solver.generatePartialSolutions(0, 0, 0, initAssign, frontierDepth, tasks);
    int totalTasks = tasks.size();
    int tasksSent = 0;
    int tasksCompleted = 0;
    int globalBestCut = INT_MAX;
    std::vector<bool> globalBestPartition(n, false);
    long long totalRecCalls = 0;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Initially send one task to each worker.
    for (int dest = 1; dest < nProcs; dest++) {
      if (tasksSent < totalTasks) {
        // Prepare a buffer of 3+n ints.
        std::vector<int> buffer(3 + n);
        buffer[0] = tasks[tasksSent].node;
        buffer[1] = tasks[tasksSent].currentCutWeight;
        buffer[2] = tasks[tasksSent].currentSizeX;
        std::vector<int> assignInt =
            boolVectorToIntVector(tasks[tasksSent].assigned);
        for (int i = 0; i < n; i++)
          buffer[3 + i] = assignInt[i];
        MPI_Send(buffer.data(), 3 + n, MPI_INT, dest, 0, MPI_COMM_WORLD);
        tasksSent++;
      } else {
        // Send termination signal (a single int -1).
        int term = -1;
        MPI_Send(&term, 1, MPI_INT, dest, 0, MPI_COMM_WORLD);
      }
    }

    // Dynamically assign tasks as workers return results.
    while (tasksCompleted < totalTasks) {
      std::vector<int> resultBuffer(2 + n);
      MPI_Status status;
      MPI_Recv(resultBuffer.data(), 2 + n, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG,
               MPI_COMM_WORLD, &status);
      int source = status.MPI_SOURCE;
      int bestCut = resultBuffer[0];
      int recCalls = resultBuffer[1];
      totalRecCalls += recCalls;
      if (bestCut < globalBestCut) {
        globalBestCut = bestCut;
        globalBestPartition = intVectorToBoolVector(
            std::vector<int>(resultBuffer.begin() + 2, resultBuffer.end()));
      }
      tasksCompleted++;

      // If tasks remain, send the next task to the worker.
      if (tasksSent < totalTasks) {
        std::vector<int> buffer(3 + n);
        buffer[0] = tasks[tasksSent].node;
        buffer[1] = tasks[tasksSent].currentCutWeight;
        buffer[2] = tasks[tasksSent].currentSizeX;
        std::vector<int> assignInt =
            boolVectorToIntVector(tasks[tasksSent].assigned);
        for (int i = 0; i < n; i++)
          buffer[3 + i] = assignInt[i];
        MPI_Send(buffer.data(), 3 + n, MPI_INT, source, 0, MPI_COMM_WORLD);
        tasksSent++;
      } else {
        // No more tasks: send termination signal.
        int term = -1;
        MPI_Send(&term, 1, MPI_INT, source, 0, MPI_COMM_WORLD);
      }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = endTime - startTime;

    // Report results.
    std::cout << "Minimum cut weight: " << globalBestCut << "\n";
    std::cout << "Total recursion calls: " << totalRecCalls << "\n";
    std::cout << "Best Partition:\nX: ";
    for (int i = 0; i < n; i++) {
      if (globalBestPartition[i])
        std::cout << i << " ";
    }
    std::cout << "\nY: ";
    for (int i = 0; i < n; i++) {
      if (!globalBestPartition[i])
        std::cout << i << " ";
    }
    std::cout << "\nElapsed time: " << elapsed.count() << " seconds\n";
  } else {
    // WORKER PROCESSES
    MinCutSolver solver(*graph, subsetSize);
    while (true) {
      // Receive a task: we expect a buffer of 3+n ints.
      std::vector<int> buffer(3 + n);
      MPI_Status status;
      MPI_Recv(buffer.data(), 3 + n, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD,
               &status);
      if (buffer[0] == -1) { // termination signal
        break;
      }
      PartialSolution task;
      task.node = buffer[0];
      task.currentCutWeight = buffer[1];
      task.currentSizeX = buffer[2];
      std::vector<int> assignInt(n);
      for (int i = 0; i < n; i++)
        assignInt[i] = buffer[3 + i];
      task.assigned = intVectorToBoolVector(assignInt);

      int localBestCut = INT_MAX;
      std::vector<bool> localBestPartition(n, false);
      long recCalls = 0;
      solver.dfsSequential(task.node, task.currentCutWeight, task.currentSizeX,
                           task.assigned, localBestCut, localBestPartition,
                           recCalls);

      // Prepare result buffer: 2+n ints.
      std::vector<int> resultBuffer(2 + n);
      resultBuffer[0] = localBestCut;
      resultBuffer[1] = recCalls;
      std::vector<int> partInt = boolVectorToIntVector(localBestPartition);
      for (int i = 0; i < n; i++)
        resultBuffer[2 + i] = partInt[i];

      MPI_Send(resultBuffer.data(), 2 + n, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }
  }

  delete graph;
  MPI_Finalize();
  return 0;
}
