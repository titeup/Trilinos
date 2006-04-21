#! /usr/bin/env python
try:
   import setpath
except ImportError:
   from PyTrilinos import Epetra, Galeri, AztecOO
   print "Using system-installed Epetra, Galeri, AztecOO"
else:
   import Epetra
   import Galeri
   import AztecOO

comm = Epetra.PyComm()

nx = 30; ny = 30
GaleriList = {
  "n": nx * ny,  # for Linear map
  "nx": nx,      # for Laplace2D, which requires nx
  "ny": ny       # and ny
}
Map = Galeri.CreateMap("Linear", comm, GaleriList)
Matrix = Galeri.CreateCrsMatrix("Laplace2D", Map, GaleriList)
Exact = Epetra.Vector(Map) 
LHS = Epetra.Vector(Map) 
RHS = Epetra.Vector(Map) 
Exact.Random()       # fix exact solution
LHS.PutScalar(0.0)   # fix starting solution
Matrix.Multiply(False, Exact, RHS) # fix rhs corresponding to Exact

# Solve the linear problem
if 0:
  # this does not work on most installations
  Problem = Epetra.LinearProblem(Matrix, LHS, RHS)
  Solver = AztecOO.AztecOO(Problem)
else:
  Solver = AztecOO.AztecOO(Matrix, LHS, RHS)

Solver.SetAztecOption(AztecOO.AZ_solver, AztecOO.AZ_cg)
Solver.SetAztecOption(AztecOO.AZ_precond, AztecOO.AZ_dom_decomp)
Solver.SetAztecOption(AztecOO.AZ_subdomain_solve, AztecOO.AZ_icc)
Solver.SetAztecOption(AztecOO.AZ_output, 16)
Solver.Iterate(1550, 1e-5)

if comm.MyPID() == 0: print "End Result: TEST PASSED"
