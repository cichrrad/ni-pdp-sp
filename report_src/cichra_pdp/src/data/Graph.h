#ifndef GRAPH_H
#define GRAPH_H

#include <string>
#include <vector>

class Graph {
public:
  Graph() = default;
  explicit Graph(const std::string &filename);

  int getNumVertices() const { return n; }
  int getEdgeWeight(int i, int j) const;

private:
  int n = 0;               // number of vertices
  std::vector<int> matrix; // flattened adjacency matrix in row-major order
};

#endif // GRAPH_H
