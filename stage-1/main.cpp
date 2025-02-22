#include "Graph.h"
#include "MinCutSolver.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <graph_file> <subset_size>\n";
        return 1;
    }

    std::string filename = argv[1];
    int subsetSize = std::stoi(argv[2]);

    try {
        Graph g(filename);
        MinCutSolver solver(g, subsetSize);

        // 1) Run the original approach
        //solver.solve();
        //solver.printBestSolution();

        // 2) Run improved approach: multiple random solutions + better LB
        solver.solveWithBetterBoundAndMultiRandom(20);
        solver.printBestSolution();

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
