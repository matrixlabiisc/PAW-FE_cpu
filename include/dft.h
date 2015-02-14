#ifndef dft_H_
#define dft_H_
#include "headers.h"
#include "poisson.h"
#include "eigen.h"
//iclude alglib
#include "/nfs/mcfs_home/rudraa/Public/alglib/cpp/src/interpolation.h"
#include "/nfs/mcfs_home/rudraa/Public/libxc/libxc-2.2.0/installDir/include/xc.h"

//Initialize Namespace
using namespace dealii;

//Define dft class
class dft{
 public:
  dft();
  void run();
  
 private:
  void mesh();
  void init();
  void initRho();
  double totalCharge();
  void locateAtomCoreNodes();

  //FE data structres
  parallel::distributed::Triangulation<3> triangulation;
  FE_Q<3>            FE;
  DoFHandler<3>      dofHandler;

  //parallel objects
  MPI_Comm   mpi_communicator;
  const unsigned int n_mpi_processes;
  const unsigned int this_mpi_process;
  IndexSet   locally_owned_dofs;
  IndexSet   locally_relevant_dofs;

  //parallel message stream
  ConditionalOStream  pcout;  
  
  //compute-time logger
  TimerOutput computing_timer;

  //poisson problem related objects
  poisson<3> poissonObject;
  PETScWrappers::MPI::SparseMatrix jacobian;
  PETScWrappers::MPI::Vector       residual;
  PETScWrappers::MPI::Vector       phiTotRhoIn, phiTotRhoOut, phiExtRhoOut;
  ConstraintMatrix   constraintsZero, constraints1byR;
  
  //eigen problem related objects
  eigen<3> eigenObject;
  PETScWrappers::MPI::Vector       massVector;
  PETScWrappers::MPI::SparseMatrix massMatrix, hamiltonianMatrix;
  std::vector<PETScWrappers::MPI::Vector> eigenVectors;
  std::vector<PETScWrappers::MPI::Vector> eigenVectorsProjected;
  std::vector<double> eigenValues;
  ConstraintMatrix constraintsNone;
  
  //dft related objects
  Table<2,double> *rhoInValues, *rhoOutValues;
  std::vector<Table<2,double>*> rhoInVals, rhoOutVals;
  std::vector<alglib::spline1dinterpolant> denSpline;
  xc_func_type funcX, funcC;
  unsigned int originIDs[numAtomTypes];
};

#endif
