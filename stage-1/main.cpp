#include <iostream>
#include <string>
#include "Graph.h"

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <graph_file>\n";
        return 1;
    }

    std::string filename = argv[1];

    try {
        Graph g(filename);
        std::cout << "Graph ["+filename+"] loaded." << std::endl;
        std::cout << "Number of vertices: " << g.getNumVertices() << std::endl;
        std::cout << "Adjacency Matrix:\n";
        for (int i = 0; i < g.getNumVertices(); ++i) {
            for (int j = 0; j < g.getNumVertices(); ++j) {
                std::cout << g.getEdgeWeight(i, j) << " ";
            }
            std::cout << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
