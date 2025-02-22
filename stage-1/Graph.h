#ifndef GRAPH_H
#define GRAPH_H

#include <vector>
#include <string>

class Graph
{
public:
    // Constructors
    Graph() = default; // default constructor (optional)
    explicit Graph(const std::string& filename);

    // Accessors
    int getNumVertices() const { return n; }
    int getEdgeWeight(int i, int j) const;

private:
    int n = 0; // number of vertices
    std::vector<std::vector<int>> adjacencyMatrix;
};

#endif // GRAPH_H
