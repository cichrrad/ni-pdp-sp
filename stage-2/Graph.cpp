#include "Graph.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

Graph::Graph(const std::string &filename) {
  std::ifstream fin(filename);
  if (!fin.is_open()) {
    throw std::runtime_error("Could not open file " + filename);
  }

  // expects n on first line
  fin >> n;
  if (n <= 0) {
    throw std::runtime_error("Invalid number of vertices in file " + filename);
  }

  // adj matrix init
  adjacencyMatrix.resize(n, std::vector<int>(n, 0));

  // parse
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      fin >> adjacencyMatrix[i][j];
      if (!fin.good()) {
        throw std::runtime_error("Error reading adjacency matrix data at row " +
                                 std::to_string(i) + ", col " +
                                 std::to_string(j));
      }
    }
  }
  fin.close();
}

int Graph::getEdgeWeight(int i, int j) const { return adjacencyMatrix[i][j]; }
