#include <stdio.h>
#include <stdlib.h>
#include "zoltan.h"

/****************************************************************************/
/* Test packing and unpacking of ZZ struct for RCB cut-tree communication   */
/* See issue #8476                                                          */
/****************************************************************************/


/*****************************************************************************
 * Toy mesh and callback functions on it 
 * Store global mesh on each processor.  
 * Each processor reports only a portion of the mesh to Zoltan in callbacks
 *****************************************************************************/
struct Mesh {
  int nCoords;        // Store global mesh on each proc
  double *x, *y, *z;  // Store global mesh on each proc
  int nMyCoords;      // Number of coordinates reported by this proc
  int myFirstCoord;   // First coordinate reported by this proc
};

void initMesh(struct Mesh *mesh, int me, int np, 
              int n_, double *x_, double *y_, double *z_) 
{
  mesh->nCoords = n_;
  mesh->x = x_;  mesh->y = y_;  mesh->z = z_;
  int coordsPerProc = mesh->nCoords/np;
  mesh->nMyCoords = (me == (np-1) ? mesh->nCoords - (np-1)*coordsPerProc 
                                  : coordsPerProc);
  mesh->myFirstCoord = me * coordsPerProc;
}

int nObj(void *data, int *ierr) {
  *ierr = ZOLTAN_OK; 
  return ((struct Mesh *) data)->nMyCoords;
}

void objMulti(void *data, int ngid, int nlid, 
              ZOLTAN_ID_PTR gid, ZOLTAN_ID_PTR lid, int wdim, float *wgt,
              int *ierr) 
{
  *ierr = ZOLTAN_OK;
  struct Mesh *mesh = (struct Mesh *) data;
  for (int i = 0; i < mesh->nMyCoords; i++) {
    lid[i] = i+mesh->myFirstCoord;
    gid[i] = i+mesh->myFirstCoord; 
  }
}

int nGeom(void *data, int *ierr) { *ierr = ZOLTAN_OK; return 3; }

void geomMulti(void *data, int ngid, int nlid, int nobj, 
               ZOLTAN_ID_PTR gid, ZOLTAN_ID_PTR lid, int ndim,
               double *coords, int *ierr)
{
  struct Mesh *mesh = (struct Mesh *) data;

  for (int i = 0; i < nobj; i++) {
    coords[i*ndim]   = mesh->x[lid[i]];
    coords[i*ndim+1] = mesh->y[lid[i]];
    coords[i*ndim+2] = mesh->z[lid[i]];
  }
  *ierr = ZOLTAN_OK;
}

/**************************************************************************
 * Test packing and unpacking of ZZ struct for RCB cut-tree communication
 * Subcommunicator computes RCB decomposition and keeps cuts.
 * Some rank which is in both subcommunicator and MPI_COMM_WORLD (in this 
 * case, rank 0) packs the resulting Zoltan struct into a buffer and 
 * broadcasts it to all procs in MPI_COMM_WORLD. 
 * All procs unpack the buffer into a new Zoltan struct.
 * All procs compare LB_Point_Assign results using the new Zoltan struct 
 * to baselines using the original.
 **************************************************************************/

int main(int narg, char **arg) {

  MPI_Init(&narg, &arg);
  float ver;
  Zoltan_Initialize(&narg, &arg, &ver);
  int ierr = ZOLTAN_OK;

  int me, np;
  MPI_Comm_size(MPI_COMM_WORLD, &np);
  MPI_Comm_size(MPI_COMM_WORLD, &me);
  if (np < 3) printf("This test is more useful on three or more processors.\n");

  /* Coordinates to be partitioned */
  const int nOne = 27;
  double xOne[nOne] = {0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1,2};
  double yOne[nOne] = {0,0,0,1,1,1,2,2,2,0,0,0,1,1,1,2,2,2,0,0,0,1,1,1,2,2,2};
  double zOne[nOne] = {0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2};
  struct Mesh meshOne;
  initMesh(&meshOne, me, np, nOne, xOne, yOne, zOne);

  /* Coordinates to use for testing */
  const int nTwo = 8;
  double xTwo[nTwo] = {0.1, 1.1, 2.1, 1.1, 0.1, 0.1, 2.1, 2.1};
  double yTwo[nTwo] = {0.1, 0.1, 0.1, 1.1, 1.1, 0.1, 1.1, 2.1};
  double zTwo[nTwo] = {0.1, 0.1, 0.1, 1.1, 1.1, 1.1, 1.1, 2.1};

  /* Create a subcommunicator in which to do partitioning. */
  /* For this test, we'll put rank 0 of MPI_COMM_WORLD in subComm */
  MPI_Comm subComm;
  MPI_Comm_split(MPI_COMM_WORLD, (me <= np / 2 ? 1 : MPI_UNDEFINED), 0,
                 &subComm);
  
  /* Compute the RCB partition on a subcommunicator subComm */
  struct Zoltan_Struct *zz = NULL;

  if (subComm != MPI_COMM_NULL) {

    zz = Zoltan_Create(subComm);

    Zoltan_Set_Num_Obj_Fn(zz, nObj, &meshOne);
    Zoltan_Set_Obj_List_Fn(zz, objMulti, &meshOne);
    Zoltan_Set_Num_Geom_Fn(zz, nGeom, &meshOne);
    Zoltan_Set_Geom_Multi_Fn(zz, geomMulti, &meshOne);

    Zoltan_Set_Param(zz, "LB_METHOD", "RCB");
    Zoltan_Set_Param(zz, "KEEP_CUTS", "1");
    Zoltan_Set_Param(zz, "RETURN_LISTS", "PART");
    Zoltan_Set_Param(zz, "NUM_GLOBAL_PARTS", "5");

    int nChanges;
    int nGid, nLid;
    int nImp, nExp;
    ZOLTAN_ID_PTR iGid = NULL, iLid = NULL;
    ZOLTAN_ID_PTR eGid = NULL, eLid = NULL;
    int *iPart = NULL, *iProc = NULL;
    int *partOne = NULL, *eProc = NULL;

    Zoltan_LB_Partition(zz, nChanges, nGid, nLid,
                        nImp, iGid, iLid, iProc, iPart,
                        nExp, eGid, eLid, eProc, partOne);

    Zoltan_LB_Free_Part(&iGid, &iLid, &iProc, &iPart);
    Zoltan_LB_Free_Part(&eGid, &eLid, &eProc, &partOne);
  }

  /* Pack the buffer; broadcast to all procs in MPI_COMM_WORLD. */
  /* Test assumes that rank 0 of MPI_COMM_WORLD is in subComm.  */

  /* First broadcast the buffer size; we make the user own the buffer */
  size_t bufSize;
  if (me == 0) bufSize = Zoltan_Serialize_Size(zz);
  MPI_Bcast(&bufSize, 1, MPI_INT, 0, MPI_COMM_WORLD);

  /* Then allocate and broadcast the buffer */
  char *buf = NULL;
  buf = (char *) malloc(bufSize * sizeof(char));
  if (me == 0) ierr = Zoltan_Serialize(zz, bufSize, buf);
  MPI_Bcast(&buf, bufSize, MPI_CHAR, 0, MPI_COMM_WORLD);

  /* All processors unpack the buffer into a new ZZ struct */

  struct Zoltan_Struct *newZZ = Zoltan_Create(MPI_COMM_WORLD);
  ierr = Zoltan_Deserialize(newZZ, bufSize, buf);

  free(buf); buf = NULL;
  
  /* Check the results */
  /* Compute and broadcast answer using original struct zz */

  int answer[nTwo];
  if (me == 0) {
    for (int i = 0; i < nTwo; i++) {
      int ignore;
      double tmp[3]; tmp[0] = xTwo[i]; tmp[1] = yTwo[i]; tmp[2] = zTwo[i];
      Zoltan_LB_Point_PP_Assign(zz, tmp, &ignore, &answer[i]);
      printf("Point (%f %f %f) on part %d\n", 
              xTwo[i], yTwo[i], zTwo[i], answer[i]);
    }
  }
  MPI_Bcast(answer, nTwo, MPI_INT, 0, MPI_COMM_WORLD);

  /* Each processor computes answer using new struct newZZ */

  int errCnt = 0;
  for (int i = 0; i < nTwo; i++) {
    int ignore;
    int newAnswer;
    double tmp[3]; tmp[0] = xTwo[i]; tmp[1] = yTwo[i]; tmp[2] = zTwo[i];
    Zoltan_LB_Point_PP_Assign(newZZ, tmp, &ignore, &newAnswer);
    if (newAnswer != answer[i]) {
      errCnt++;
      printf("%d Error (%f %f %f):  part %d != new part %d\n", 
             me, xTwo[i], yTwo[i], zTwo[i], answer[i], newAnswer);
    }
  }

  /* Gather global test result */

  int gErrCnt = 0;
  MPI_Allreduce(&errCnt, &gErrCnt, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
  if (me == 0) 
    printf("TEST %s: gErrCnt=%d\n", (gErrCnt ? "FAILED" : "PASSED"), gErrCnt);

  return gErrCnt;
}
