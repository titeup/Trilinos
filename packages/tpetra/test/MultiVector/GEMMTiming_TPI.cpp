#include <Teuchos_GlobalMPISession.hpp>
#include <Teuchos_CommandLineProcessor.hpp>
#include <Teuchos_DefaultMpiComm.hpp>

#include <Kokkos_TPINode.hpp>

#include "GEMMTiming.hpp"

int main(int argc, char *argv[]) {
  Teuchos::oblackholestream blackhole;
  Teuchos::GlobalMPISession mpiSession(&argc,&argv);

  //
  // Get example parameters from command-line processor
  //  
  int M = 10000;
  int N = 1000;
  int numThreads = -1;
  int verbose = 1;
  Teuchos::CommandLineProcessor cmdp(false,true);
  cmdp.setOption("M",&M,"Global matrix num rows.");
  cmdp.setOption("N",&N,"Global matrix num cols.");
  cmdp.setOption("num-threads",&numThreads,"Number of threads.");
  cmdp.setOption("verbose",&verbose,"Verbose (zero for silent).");
  if (cmdp.parse(argc,argv) != Teuchos::CommandLineProcessor::PARSE_SUCCESSFUL) {
    return -1;
  }

  // 
  // Say hello, print some communicator info
  //
  Teuchos::RCP<const Teuchos::Comm<int> > comm = Teuchos::createMpiComm<int>(Teuchos::opaqueWrapper<MPI_Comm>(MPI_COMM_WORLD));
  if (comm->getRank() == 0) {
    std::cout << "\n" << Tpetra::version() << std::endl << std::endl;
    std::cout << argv[0] << ", M == " << M << ", N == " << N << std::endl;
    std::cout << "Comm info: " << *comm;
  }

  typedef Kokkos::TPINode Node;
  Teuchos::ParameterList params;
  params.set<int>("Num Threads",numThreads);
  params.set<int>("Verbose",verbose);
  Teuchos::RCP<Node> node = Teuchos::rcp(new Node(params));

  GEMMTiming<Node>(M,N,comm,node);

  return 0;
}
