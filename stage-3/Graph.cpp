#include "Graph.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

Graph::Graph(const std::string &filename) {
  std::ifstream fin(filename);
  if (!fin.is_open()) {
    throw std::runtime_error("Could not open file " + filename);
  }

  // Read number of vertices from the first line.
  fin >> n;
  if (n <= 0) {
    throw std::runtime_error("Invalid number of vertices in file " + filename);
  }

  // Allocate a flattened adjacency matrix.
  matrix.resize(n * n, 0);

  // Read the matrix.
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      fin >> matrix[i * n + j];
      if (!fin.good()) {
        throw std::runtime_error("Error reading data at row " +
                                 std::to_string(i) + ", col " +
                                 std::to_string(j));
      }
    }
  }
  fin.close();

  // --- Heuristic Reordering ---
  // Compute a degree for each vertex (sum of its edge weights).
  std::vector<int> degrees(n, 0);
  for (int i = 0; i < n; ++i) {
    int deg = 0;
    for (int j = 0; j < n; ++j) {
      deg += matrix[i * n + j];
    }
    degrees[i] = deg;
  }

  // Create an ordering vector and sort vertices by descending degree.
  std::vector<int> order(n);
  for (int i = 0; i < n; ++i) {
    order[i] = i;
  }
  std::sort(order.begin(), order.end(),
            [&](int a, int b) { return degrees[a] > degrees[b]; });

  // Reorder the adjacency matrix.
  std::vector<int> newMatrix(n * n, 0);
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      newMatrix[i * n + j] = matrix[order[i] * n + order[j]];
    }
  }
  matrix = newMatrix;
}

int Graph::getEdgeWeight(int i, int j) const { return matrix[i * n + j]; }
