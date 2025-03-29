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
// Helper functions for packing/unpacking a PartialSolution.
// The PartialSolution is defined as a struct inside MinCutSolver.
// We pack it into an integer array with layout:
//   [ node, currentCutWeight, currentSizeX, assigned[0], assigned[1], ...,
//   assigned[n-1] ]
// Since vector<bool> is not contiguous, we pack each boolean as 0/1.
// -----------------------------------------------------------------------------
void packPartialSolution(const MinCutSolver::PartialSolution &ps,
                         std::vector<int> &data, int n) {
  data.clear();
  data.push_back(ps.node);
  data.push_back(ps.currentCutWeight);
  data.push_back(ps.currentSizeX);
  for (int i = 0; i < n; i++) {
    data.push_back(ps.assigned[i] ? 1 : 0);
  }
}

void unpackPartialSolution(const std::vector<int> &data,
                           MinCutSolver::PartialSolution &ps, int n) {
  ps.node = data[0];
  ps.currentCutWeight = data[1];
  ps.currentSizeX = data[2];
  ps.assigned.resize(n);
  for (int i = 0; i < n; i++) {
    ps.assigned[i] = (data[3 + i] != 0);
  }
}

// -----------------------------------------------------------------------------
// MPI main: Master-slave dynamic work distribution.
// -----------------------------------------------------------------------------
int main(int argc, char *argv[]) {
  MPI_Init(&argc, &argv);

  int rank, numProcs;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &numProcs);

  // Command line check: require graph filename and subset size.
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

    // Decide on a frontier depth (for generating partial solutions).
    int frontierDepth = std::min(subsetSize, 16);

    // Each process creates its own instance of the solver.
    MinCutSolver solver(g, subsetSize);

    if (rank == 0) {
      // ------------------------- MASTER PROCESS -----------------------------
      std::cout << "Master: Running on " << numProcs << " processes.\n";

      // Generate partial solutions up to the chosen frontier depth.
      std::vector<MinCutSolver::PartialSolution> partialSolutions;
      std::vector<bool> initAssign(n, false);
      solver.generatePartialSolutions(0, 0, 0, initAssign, frontierDepth,
                                      partialSolutions);
      int numWorkItems = partialSolutions.size();
      std::cout << "Master: Generated " << numWorkItems
                << " partial solutions.\n";

      int workIndex = 0;
      int resultsReceived = 0;
      int globalBestCut = INT_MAX;
      std::vector<int> data; // For packing partial solutions.

      // Initially, send one work item to each slave.
      for (int dest = 1; dest < numProcs; dest++) {
        if (workIndex < numWorkItems) {
          packPartialSolution(partialSolutions[workIndex], data, n);
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

      // Process results and send new work items until all are done.
      while (resultsReceived < numWorkItems) {
        MPI_Status status;
        int slaveResult;
        MPI_Recv(&slaveResult, 1, MPI_INT, MPI_ANY_SOURCE, RESULT_TAG,
                 MPI_COMM_WORLD, &status);
        resultsReceived++;
        if (slaveResult < globalBestCut) {
          globalBestCut = slaveResult;
        }
        // If there is more work, send the next work item to the slave that just
        // responded.
        if (workIndex < numWorkItems) {
          packPartialSolution(partialSolutions[workIndex], data, n);
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
          // Expect a message with 3+n integers.
          int messageSize = 3 + n;
          std::vector<int> data(messageSize);
          MPI_Recv(data.data(), messageSize, MPI_INT, 0, WORK_TAG,
                   MPI_COMM_WORLD, &status);
          MinCutSolver::PartialSolution ps;
          unpackPartialSolution(data, ps, n);

          // Process the partial solution with DFS (using OpenMP inside
          // dfsSequential).
          solver.dfsSequential(ps.node, ps.currentCutWeight, ps.currentSizeX,
                               ps.assigned);
          int localBest =
              solver.minCutWeight; // Best cut weight found in this DFS branch.

          // Send result back to master.
          MPI_Send(&localBest, 1, MPI_INT, 0, RESULT_TAG, MPI_COMM_WORLD);
          // Reset solver's minCutWeight for the next work item.
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
