// mpi_main.cpp

#include "Graph.h"
#include "MinCutSolver.h"
#include "mpi.h"
#include <algorithm>
#include <climits>
#include <iostream>
#include <string>
#include <vector>

// Message tags for MPI.
#define WORK_TAG 1
#define RESULT_TAG 2
#define TERMINATION_TAG 3

// -----------------------------------------------------------------------------
// Updated helper functions for packing/unpacking a PartialSolution.
// The PartialSolution (defined in MinCutSolver) is sent as an array of
// integers. The message layout is now:
//   [ globalBound, node, currentCutWeight, currentSizeX, assigned[0],
//   assigned[1], ..., assigned[n-1] ]
// -----------------------------------------------------------------------------
void packPartialSolution(const MinCutSolver::PartialSolution &ps,
                         std::vector<int> &data, int n, int globalBound) {
  data.clear();
  data.push_back(globalBound); // Embedded global best bound
  data.push_back(ps.node);
  data.push_back(ps.currentCutWeight);
  data.push_back(ps.currentSizeX);
  // Convert each bool to an int (0 or 1).
  for (int i = 0; i < n; i++) {
    data.push_back(ps.assigned[i] ? 1 : 0);
  }
}

void unpackPartialSolution(const std::vector<int> &data,
                           MinCutSolver::PartialSolution &ps, int n,
                           int &globalBound) {
  globalBound = data[0]; // Extract global best bound from the message
  ps.node = data[1];
  ps.currentCutWeight = data[2];
  ps.currentSizeX = data[3];
  ps.assigned.resize(n);
  for (int i = 0; i < n; i++) {
    ps.assigned[i] = (data[4 + i] != 0);
  }
}

// -----------------------------------------------------------------------------
// MPI main: Master-slave dynamic work distribution using the improved
// partition representation, global bound embedding, and lower frontier depth.
// -----------------------------------------------------------------------------
int main(int argc, char *argv[]) {
  MPI_Init(&argc, &argv);

  int rank, numProcs;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &numProcs);

  // Command-line check: need graph filename and subset size.
  if (argc < 3) {
    if (rank == 0) {
      std::cerr << "Usage: " << argv[0] << " <graph_file> <subset_size>\n";
    }
    MPI_Finalize();
    return 1;
  }
  std::string filename = argv[1];
  int subsetSize = std::stoi(argv[2]);

  try {
    // All processes load the graph.
    Graph g(filename);
    int n = g.getNumVertices();

    // Use a lower frontier depth (e.g. 4 or min(subsetSize, 4)) to reduce the
    // number of tasks.
    int frontierDepth = std::min(subsetSize, 4);

    // Each process creates its own instance of the solver.
    MinCutSolver solver(g, subsetSize);

    if (rank == 0) {
      // ------------------------- MASTER PROCESS -----------------------------
      std::cout << "Master: Running on " << numProcs << " processes.\n";

      // Generate partial solutions up to the chosen (lower) frontier depth.
      std::vector<MinCutSolver::PartialSolution> partialSolutions;
      std::vector<bool> initAssign(n, false);
      solver.generatePartialSolutions(0, 0, 0, initAssign, frontierDepth,
                                      partialSolutions);
      int numWorkItems = partialSolutions.size();
      std::cout << "Master: Generated " << numWorkItems
                << " partial solutions.\n";

      // Global best bound maintained by the master.
      int globalBestCut =
          solver.minCutWeight; // (may be updated by guesstimate)
      if (globalBestCut == INT_MAX) {
        globalBestCut = INT_MAX;
      }

      int workIndex = 0;
      int resultsReceived = 0;
      std::vector<int> data; // For packing work items.

      // Initially, send one work item to each slave.
      for (int dest = 1; dest < numProcs; dest++) {
        if (workIndex < numWorkItems) {
          packPartialSolution(partialSolutions[workIndex], data, n,
                              globalBestCut);
          MPI_Send(data.data(), data.size(), MPI_INT, dest, WORK_TAG,
                   MPI_COMM_WORLD);
          workIndex++;
        } else {
          // No work left: send termination signal.
          int termSignal = -1;
          MPI_Send(&termSignal, 1, MPI_INT, dest, TERMINATION_TAG,
                   MPI_COMM_WORLD);
        }
      }

      // Process results and send new work items until all tasks are completed.
      while (resultsReceived < numWorkItems) {
        MPI_Status status;
        int slaveResult;
        MPI_Recv(&slaveResult, 1, MPI_INT, MPI_ANY_SOURCE, RESULT_TAG,
                 MPI_COMM_WORLD, &status);
        resultsReceived++;
        if (slaveResult < globalBestCut) {
          globalBestCut = slaveResult;
        }
        // When a result is received, send the next available work item.
        if (workIndex < numWorkItems) {
          packPartialSolution(partialSolutions[workIndex], data, n,
                              globalBestCut);
          MPI_Send(data.data(), data.size(), MPI_INT, status.MPI_SOURCE,
                   WORK_TAG, MPI_COMM_WORLD);
          workIndex++;
        } else {
          // No more work: send termination signal.
          int termSignal = -1;
          MPI_Send(&termSignal, 1, MPI_INT, status.MPI_SOURCE, TERMINATION_TAG,
                   MPI_COMM_WORLD);
        }
      }
      std::cout << "Master: Global best cut weight = " << globalBestCut << "\n";
    } else {
      // ------------------------- SLAVE PROCESSES -----------------------------
      while (true) {
        MPI_Status status;
        MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        if (status.MPI_TAG == TERMINATION_TAG) {
          int dummy;
          MPI_Recv(&dummy, 1, MPI_INT, 0, TERMINATION_TAG, MPI_COMM_WORLD,
                   &status);
          break; // Termination signal received.
        } else if (status.MPI_TAG == WORK_TAG) {
          // Expect a message with 4+n integers.
          int messageSize = 4 + n;
          std::vector<int> data(messageSize);
          MPI_Recv(data.data(), messageSize, MPI_INT, 0, WORK_TAG,
                   MPI_COMM_WORLD, &status);
          MinCutSolver::PartialSolution ps;
          int receivedGlobalBound;
          unpackPartialSolution(data, ps, n, receivedGlobalBound);
          // Set the solver's local bound to the received global best.
          solver.minCutWeight = receivedGlobalBound;
          // Process the partial solution with DFS (using OpenMP internally).
          solver.dfsSequential(ps.node, ps.currentCutWeight, ps.currentSizeX,
                               ps.assigned);
          int localBest = solver.minCutWeight;
          // Send back the best cut weight found for this task.
          MPI_Send(&localBest, 1, MPI_INT, 0, RESULT_TAG, MPI_COMM_WORLD);
          // Reset solver's minCutWeight before processing the next work item.
          solver.minCutWeight = INT_MAX;
        }
      }
    }
  } catch (const std::exception &e) {
    if (rank == 0) {
      std::cerr << "ERROR: " << e.what() << std::endl;
    }
    MPI_Finalize();
    return 1;
  }

  MPI_Finalize();
  return 0;
}
