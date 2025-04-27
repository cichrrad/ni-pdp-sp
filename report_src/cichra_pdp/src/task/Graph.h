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
  int n = 0;                                     // size
  std::vector<std::vector<int>> adjacencyMatrix; // representation
};

#endif // GRAPH_H
