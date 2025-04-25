// main.cpp
#include <algorithm>
#include <chrono>
#include <climits>
#include <fstream>
#include <iostream>
#include <mpi.h>
#include <omp.h>
#include <stdexcept>
#include <vector>

//----------------------------------------------------------------------
// Simple Graph with a flattened adjacency matrix
struct Graph {
  int n;
  std::vector<int> mat;
  Graph(const std::string &fname) {
    std::ifstream fin(fname);
    if (!fin) throw std::runtime_error("Cannot open " + fname);
    fin >> n;
    if (n <= 0) throw std::runtime_error("Invalid n");
    mat.resize(n * n);
    for (int i = 0; i < n * n; ++i) fin >> mat[i];
  }
  // Broadcast constructor
  Graph(int n_, const std::vector<int> &flat) : n(n_), mat(flat) {}
  inline int ew(int i, int j) const { return mat[i * n + j]; }
};

//----------------------------------------------------------------------
// A single partial‐solution “task” (master→worker)
struct PartialSolution {
  int node, cutSoFar, sizeX;
  std::vector<bool> assigned;
};

// Helpers: pack/unpack vector<bool> into MPI buffers
static std::vector<int> b2i(const std::vector<bool> &v) {
  std::vector<int> r(v.size());
  for (size_t i = 0; i < v.size(); ++i) r[i] = v[i] ? 1 : 0;
  return r;
}
static std::vector<bool> i2b(const std::vector<int> &v) {
  std::vector<bool> r(v.size());
  for (size_t i = 0; i < v.size(); ++i) r[i] = (v[i] != 0);
  return r;
}

//----------------------------------------------------------------------
// The Hybrid MPI+OpenMP MinCut Solver
struct MinCutSolver {
  const Graph &G;
  int n, a;
  MinCutSolver(const Graph &g, int subsetSize) : G(g), n(g.n), a(subsetSize) {}

  // Tight lower bound from 'node' onward:
  //   baseline = sum of costY(i) for i=node..n-1
  //   deltas[i] = costX(i)-costY(i)
  //   we *must* pick exactly remainX of those to go X → pick the smallest deltas
  int lowerBound(int node, int sizeX, const std::vector<bool> &assigned) const {
    int unassigned = n - node;
    int remainX    = a - sizeX;
    // 1) baseline = all to Y
    std::vector<int> deltas; deltas.reserve(unassigned);
    int lb = 0;
    for (int i = node; i < n; ++i) {
      int costX = 0, costY = 0;
      for (int j = 0; j < node; ++j) {
        if (assigned[j]) costY += G.ew(i,j);
        else             costX += G.ew(i,j);
      }
      lb += costY;
      deltas.push_back(costX - costY);
    }
    // 2) we *must* send exactly remainX of these into X → pick the 'remainX' smallest deltas
    int k = std::max(0, std::min(remainX, unassigned));
    if (k > 0) {
      std::nth_element(deltas.begin(), deltas.begin()+k, deltas.end());
      for (int i = 0; i < k; ++i) lb += deltas[i];
    }
    return lb;
  }

  // OpenMP‐parallel DFS from a partial node → finds the best cut ≤ localBest
  void dfsParallel(int node,
                   int cutSoFar,
                   int sizeX,
                   const std::vector<bool> &assigned,
                   int &localBest,
                   std::vector<bool> &bestPart,
                   long &recCalls) const
  {
    recCalls++;
    if (node == n) {
      if (sizeX == a) {
        #pragma omp critical
        {
          if (cutSoFar < localBest) {
            localBest = cutSoFar;
            bestPart  = assigned;
          }
        }
      }
      return;
    }
    int lb = lowerBound(node, sizeX, assigned);
    if (cutSoFar + lb >= localBest) return;

    // Branch → put 'node' in X (if we still can)
    if (sizeX < a) {
      auto a2 = assigned;
      a2[node] = true;
      int c2 = cutSoFar;
      for (int j = 0; j < node; ++j)
        if (!a2[j]) c2 += G.ew(j,node);
      #pragma omp task firstprivate(node,c2,sizeX,a2) \
                       shared(localBest,bestPart,recCalls)
      dfsParallel(node+1, c2, sizeX+1, a2, localBest, bestPart, recCalls);
    }
    // Branch → put 'node' in Y
    {
      auto a2 = assigned;
      a2[node] = false;
      int c2 = cutSoFar;
      for (int j = 0; j < node; ++j)
        if (assigned[j]) c2 += G.ew(j,node);
      #pragma omp task firstprivate(node,c2,sizeX,a2) \
                       shared(localBest,bestPart,recCalls)
      dfsParallel(node+1, c2, sizeX, a2, localBest, bestPart, recCalls);
    }
    #pragma omp taskwait
  }

  // Generate *all* partial solutions down to frontierDepth by pure enumeration.
  void generatePartialSolutions(int node,
                                int cutSoFar,
                                int sizeX,
                                const std::vector<bool> &assigned,
                                int frontierDepth,
                                std::vector<PartialSolution> &out) const
  {
    if (node == frontierDepth) {
      out.push_back({node, cutSoFar, sizeX, assigned});
      return;
    }
    // always branch both ways (X only if sizeX<a)
    if (sizeX < a) {
      auto a2 = assigned;
      a2[node] = true;
      int c2 = cutSoFar;
      for (int j = 0; j < node; ++j)
        if (!a2[j]) c2 += G.ew(j,node);
      generatePartialSolutions(node+1, c2, sizeX+1, a2, frontierDepth, out);
    }
    {
      auto a2 = assigned;
      a2[node] = false;
      int c2 = cutSoFar;
      for (int j = 0; j < node; ++j)
        if (assigned[j]) c2 += G.ew(j,node);
      generatePartialSolutions(node+1, c2, sizeX, a2, frontierDepth, out);
    }
  }
};

//----------------------------------------------------------------------
// Main: master–worker via MPI, DFS via OpenMP
int main(int argc, char **argv) {
  MPI_Init(&argc,&argv);
  int rank, nProcs;
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&nProcs);
  if (argc < 3) {
    if (rank==0) std::cerr<<"Usage: "<<argv[0]<<" <graph> <subsetSize>\n";
    MPI_Finalize();
    return 1;
  }

  int subsetSize = std::stoi(argv[2]);
  int n;
  std::vector<int> flat;
  Graph *G=nullptr;

  // Master reads and broadcasts the graph
  if (rank==0) {
    std::ifstream fin(argv[1]);
    if (!fin) { std::cerr<<"Cannot open\n"; MPI_Abort(MPI_COMM_WORLD,1); }
    fin>>n;
    flat.resize(n*n);
    for (int i=0;i<n*n;++i) fin>>flat[i];
  }
  MPI_Bcast(&n,1,MPI_INT,0,MPI_COMM_WORLD);
  if (rank!=0) flat.resize(n*n);
  MPI_Bcast(flat.data(),n*n,MPI_INT,0,MPI_COMM_WORLD);
  G = new Graph(n, flat);

  int frontierDepth = std::min(subsetSize, 16);
  MinCutSolver solver(*G, subsetSize);

  if (rank==0) {
    // MASTER: generate tasks
    std::vector<bool> initA(n,false);
    std::vector<PartialSolution> tasks;
    solver.generatePartialSolutions(0,0,0,initA,frontierDepth,tasks);
    int total = tasks.size(), sent=0, done=0;
    int globalBest = INT_MAX;
    long long totalCalls=0;

    auto t0 = std::chrono::high_resolution_clock::now();
    // initial dispatch
    for (int dest=1; dest<nProcs; ++dest) {
      if (sent<total) {
        std::vector<int> buf(4+n);
        buf[0]=tasks[sent].node;
        buf[1]=tasks[sent].cutSoFar;
        buf[2]=tasks[sent].sizeX;
        buf[3]=globalBest;
        auto ai = b2i(tasks[sent].assigned);
        std::copy(ai.begin(), ai.end(), buf.begin()+4);
        MPI_Send(buf.data(),(int)buf.size(),MPI_INT,dest,0,MPI_COMM_WORLD);
        ++sent;
      } else {
        int term=-1;
        MPI_Send(&term,1,MPI_INT,dest,0,MPI_COMM_WORLD);
      }
    }
    // dynamic re-assign
    while (done<total) {
      std::vector<int> rbuf(2+n);
      MPI_Status st;
      MPI_Recv(rbuf.data(),(int)rbuf.size(),MPI_INT,
               MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&st);
      int source=st.MPI_SOURCE;
      int bestCut=rbuf[0], calls=rbuf[1];
      totalCalls += calls;
      if (bestCut<globalBest) globalBest=bestCut;
      ++done;

      if (sent<total) {
        std::vector<int> buf(4+n);
        buf[0]=tasks[sent].node;
        buf[1]=tasks[sent].cutSoFar;
        buf[2]=tasks[sent].sizeX;
        buf[3]=globalBest;
        auto ai=b2i(tasks[sent].assigned);
        std::copy(ai.begin(), ai.end(), buf.begin()+4);
        MPI_Send(buf.data(),(int)buf.size(),MPI_INT,source,0,MPI_COMM_WORLD);
        ++sent;
      } else {
        int term=-1;
        MPI_Send(&term,1,MPI_INT,source,0,MPI_COMM_WORLD);
      }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double secs = std::chrono::duration<double>(t1-t0).count();
    std::cout<<"Minimum cut weight: "<<globalBest<<"\n"
             <<"Total DFS calls:    "<<totalCalls<<"\n"
             <<"Elapsed time:       "<<secs<<" s\n";

  } else {
    // WORKER: receive a task, run dfsParallel, send back
    while (true) {
      std::vector<int> buf(4+n);
      MPI_Status st;
      MPI_Recv(buf.data(),(int)buf.size(),MPI_INT,
               0,MPI_ANY_TAG,MPI_COMM_WORLD,&st);
      if (buf[0]==-1) break;
      int node      = buf[0];
      int cutSoFar  = buf[1];
      int sizeX     = buf[2];
      int globalBound = buf[3];
      std::vector<int> ai(buf.begin()+4, buf.end());
      std::vector<bool> assigned = i2b(ai);

      int localBest = globalBound;
      std::vector<bool> bestPart(n,false);
      long recCalls = 0;

      #pragma omp parallel
      {
        #pragma omp single nowait
        solver.dfsParallel(node,cutSoFar,sizeX,
                           assigned,localBest,
                           bestPart,recCalls);
      }

      std::vector<int> rbuf(2+n);
      rbuf[0] = localBest;
      rbuf[1] = (int)recCalls;
      auto pi = b2i(bestPart);
      std::copy(pi.begin(), pi.end(), rbuf.begin()+2);
      MPI_Send(rbuf.data(),(int)rbuf.size(),MPI_INT,
               0,0,MPI_COMM_WORLD);
    }
  }

  delete G;
  MPI_Finalize();
  return 0;
}

