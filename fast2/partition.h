/*
Copyright (C) 2011 by Rio Yokota, Simon Layton, Lorena Barba

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#ifndef partition_h
#define partition_h
#include "../include/mympi.h"
#include "serialfmm.h"

//! Handles all the partitioning of domains
template<Equation equation>
class Partition : public MyMPI, public SerialFMM<equation> {
private:
  vec<3,int> Npartition;                                        //!< Number of partitions in each direction
  vect       Xpartition;                                        //!< Size of partitions in each direction
  int       *sendCount;                                         //!< Send count
  int       *sendDispl;                                         //!< Send displacement
  int       *recvCount;                                         //!< Receive count
  int       *recvDispl;                                         //!< Receive displacement

protected:
  std::vector<vect> XMIN;                                       //!< Minimum position vector of bodies
  std::vector<vect> XMAX;                                       //!< Maximum position vector of bodies

public:
  using Kernel<equation>::printNow;                             //!< Switch to print timings
  using Kernel<equation>::startTimer;                           //!< Start timer for given event
  using Kernel<equation>::stopTimer;                            //!< Stop timer for given event
  using Kernel<equation>::sortBodies;                           //!< Sort bodies according to cell index
  using Kernel<equation>::X0;                                   //!< Set center of root cell
  using Kernel<equation>::R0;                                   //!< Set radius of root cell

private:
//! Set partition of global domain
  void setPartition(Bodies &bodies) {
    int mpisize = MPISIZE;                                      // Initialize MPI size counter
    int d = 0;                                                  // Initialize dimension counter
    while( mpisize != 1 ) {                                     // Divide domain  while counter is not one
      Npartition[d] <<= 1;                                      //  Divide this dimension
      d = (d+1) % 3;                                            //  Increment dimension
      mpisize >>= 1;                                            //  Right shift the bits of counter
    }                                                           // End while loop for domain subdivision
    for( d=0; d!=3; ++d ) {                                     // Loop over dimensions
      Xpartition[d] = 2 * R0 / Npartition[d];                   //  Size of partition in each direction
    }                                                           // End loop over dimensions
    for( B_iter B=bodies.begin(); B!=bodies.end(); ++B ) {      // Loop over bodies
      int ix = int((B->X[0] + R0 - X0[0]) / Xpartition[0]);     //  x contribution to send rank
      int iy = int((B->X[1] + R0 - X0[1]) / Xpartition[1]);     //  y contribution
      int iz = int((B->X[2] + R0 - X0[2]) / Xpartition[2]);     //  z contribution
      B->ICELL = ix + Npartition[0] * (iy + iz * Npartition[1]);//  Set cell index to send rank
    }                                                           // End loop over bodies
  }

//! Allgather boundaries of all partitions
  void allgather() {
    int ix = MPIRANK % Npartition[0];                           // x index of partition
    int iy = MPIRANK / Npartition[0] % Npartition[1];           // y index of partition
    int iz = MPIRANK / Npartition[0] / Npartition[1];           // z index of partition
    vect xmin, xmax;                                            // Local domain boundaries at current rank
    xmin[0] = X0[0] - R0 + ix * Xpartition[0];                  // xmin of local domain at current rank
    xmin[1] = X0[1] - R0 + iy * Xpartition[1];                  // ymin of local domain at current rank
    xmin[2] = X0[2] - R0 + iz * Xpartition[2];                  // zmin of local domain at current rank
    xmax[0] = X0[0] - R0 + (ix + 1) * Xpartition[0];            // xmax of local domain at current rank
    xmax[1] = X0[1] - R0 + (iy + 1) * Xpartition[1];            // ymax of local domain at current rank
    xmax[2] = X0[2] - R0 + (iz + 1) * Xpartition[2];            // zmax of local domain at current rank
    MPI_Datatype MPI_TYPE = getType(xmin[0]);                   // Get MPI data type
    XMIN.resize(MPISIZE);                                       // Xmin of every local domain
    XMAX.resize(MPISIZE);                                       // Xmax of every local domain
    MPI_Allgather(xmin,3,MPI_TYPE,&XMIN[0],3,MPI_TYPE,MPI_COMM_WORLD);// Gather all domain boundaries
    MPI_Allgather(xmax,3,MPI_TYPE,&XMAX[0],3,MPI_TYPE,MPI_COMM_WORLD);// Gather all domain boundaries
  }

//! Alltoall send count
  void alltoall(Bodies &bodies) {
    Bodies buffer = bodies;                                     // Sort buffer
    stopTimer("Partition");                                     // Stop timer 
    sortBodies(bodies,buffer);                                  // Sort bodies in ascending order
    startTimer("Partition");                                    // Start timer
    for( int i=0; i!=MPISIZE; ++i ) {                           // Loop over ranks
      sendCount[i] = 0;                                         //  Initialize send counts
    }                                                           // End loop over ranks
    for( B_iter B=bodies.begin(); B!=bodies.end(); ++B ) {      // Loop over bodies
      sendCount[B->ICELL]++;                                    //  Fill send count bucket
    }                                                           // End loop over bodies
    MPI_Alltoall(sendCount,1,MPI_INT,recvCount,1,MPI_INT,MPI_COMM_WORLD);// Communicate send count to get recv count
  }

//! Alltoallv bodies
  void alltoallv(Bodies &bodies) {
    int word = sizeof(bodies[0]) / 4;                           // Word size of body structure
    sendDispl[0] = recvDispl[0] = 0;                            // Initialize send/recv displacements
    for( int irank=0; irank!=MPISIZE-1; ++irank ) {             // Loop over ranks
      sendDispl[irank+1] = sendDispl[irank] + sendCount[irank]; //  Set send displacement based on send count
      recvDispl[irank+1] = recvDispl[irank] + recvCount[irank]; //  Set recv displacement based on recv count
    }                                                           // End loop over ranks
    Bodies buffer(recvDispl[MPISIZE-1]+recvCount[MPISIZE-1]);   // Resize recv buffer
    for( int irank=0; irank!=MPISIZE; ++irank ) {               // Loop over ranks
      sendCount[irank] *= word;                                 //  Multiply send count by word size of data
      sendDispl[irank] *= word;                                 //  Multiply send displacement by word size of data
      recvCount[irank] *= word;                                 //  Multiply recv count by word size of data
      recvDispl[irank] *= word;                                 //  Multiply recv displacement by word size of data
    }                                                           // End loop over ranks
    MPI_Alltoallv(&bodies[0],sendCount,sendDispl,MPI_INT,       // Communicate bodies
                  &buffer[0],recvCount,recvDispl,MPI_INT,MPI_COMM_WORLD);
    bodies = buffer;                                            // Copy recv buffer to bodies
  }

public:
//! Constructor
  Partition() : MyMPI(), Npartition(1), Xpartition(0),
                sendCount(NULL), sendDispl(NULL), recvCount(NULL), recvDispl(NULL),
                XMIN(), XMAX() {
    sendCount = new int [MPISIZE];                              // Allocate send count
    sendDispl = new int [MPISIZE];                              // Allocate send displacement
    recvCount = new int [MPISIZE];                              // Allocate receive count
    recvDispl = new int [MPISIZE];                              // Allocate receive displacement
  }
//! Destructor
  ~Partition() {
    delete[] sendCount;                                         // Deallocate send count
    delete[] sendDispl;                                         // Deallocate send displacement
    delete[] recvCount;                                         // Deallocate receive count
    delete[] recvDispl;                                         // Deallocate receive displacement
  }

//! Send bodies to next rank (round robin)
  void shiftBodies(Bodies &bodies) {
    int newSize;                                                // New number of bodies
    int oldSize = bodies.size();                                // Current number of bodies
    const int word = sizeof(bodies[0]);                         // Word size of body structure
    const int isend = (MPIRANK + 1          ) % MPISIZE;        // Send to next rank (wrap around)
    const int irecv = (MPIRANK - 1 + MPISIZE) % MPISIZE;        // Receive from previous rank (wrap around)
    MPI_Request sreq,rreq;                                      // Send, recv request handles

    MPI_Isend(&oldSize,1,MPI_INT,irecv,0,MPI_COMM_WORLD,&sreq); // Send current number of bodies
    MPI_Irecv(&newSize,1,MPI_INT,isend,0,MPI_COMM_WORLD,&rreq); // Receive new number of bodies
    MPI_Wait(&sreq,MPI_STATUS_IGNORE);                          // Wait for send to complete
    MPI_Wait(&rreq,MPI_STATUS_IGNORE);                          // Wait for recv to complete

    Bodies buffer(newSize);                                     // Resize buffer to new number of bodies
    MPI_Isend(&bodies[0],oldSize*word,MPI_INT,irecv,            // Send bodies to next rank
              1,MPI_COMM_WORLD,&sreq);
    MPI_Irecv(&buffer[0],newSize*word,MPI_INT,isend,            // Receive bodies from previous rank
              1,MPI_COMM_WORLD,&rreq);
    MPI_Wait(&sreq,MPI_STATUS_IGNORE);                          // Wait for send to complete
    MPI_Wait(&rreq,MPI_STATUS_IGNORE);                          // Wait for recv to complete
    bodies = buffer;                                            // Copy bodies from buffer
  }

//! Partition by recursive octsection
  void octsection(Bodies &bodies) {
    startTimer("Partition");                                    // Start timer
    X0 = 0;                                                     // Set center of global domain
    R0 = M_PI;                                                  // Set radius of global domain
    setPartition(bodies);                                       // Set partitioning strategy
    allgather();                                                // Allgather boundaries of all partitions
    alltoall(bodies);                                           // Alltoall send count
    alltoallv(bodies);                                          // Alltoallv bodies
    stopTimer("Partition",printNow);                            // Stop timer 
  }

//! Send bodies back to where they came from
  void unpartition(Bodies &bodies) {
    startTimer("Unpartition");                                  // Start timer
    int word = sizeof(bodies[0]) / 4;                           // Word size of body structure
    for( B_iter B=bodies.begin(); B!=bodies.end(); ++B ) {      // Loop over bodies
      B->ICELL = B->IPROC;                                      //  Copy process rank to cell index for sorting
    }                                                           // End loop over bodies
    Bodies buffer = bodies;                                     // Resize sort buffer
    stopTimer("Unpartition");                                   // Stop timer 
    sortBodies(bodies,buffer);                                  // Sort bodies in ascending order
    startTimer("Unpartition");                                  // Start timer
    alltoall(bodies);                                           // Alltoall send count
    alltoallv(bodies);                                          // Alltoallv bodies
    for( B_iter B=bodies.begin(); B!=bodies.end(); ++B ) {      // Loop over bodies
      assert(B->IPROC == MPIRANK);                              //  Make sure bodies are in the right rank
    }                                                           // End loop over bodies
    stopTimer("Unpartition",printNow);                          // Stop timer 
  }
};

#endif
