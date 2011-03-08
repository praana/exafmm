#include "partition.h"
#include "dataset.h"
#include "construct.h"
#ifdef VTK
#include "vtk.h"
#endif

int main() {
  const int numBodies = 1000000;
  Bodies bodies(numBodies);
  Dataset D;
  Partition T;
  if( T.commRank() == 0 ) T.printNow = true;

  T.startTimer("Set bodies   ");
  D.random(bodies,T.commRank()+1);
  T.stopTimer("Set bodies   ",T.printNow);

  T.startTimer("Set domain   ");
  T.setGlobDomain(bodies);
  T.stopTimer("Set domain   ",T.printNow);

  T.startTimer("Set index    ");
  T.BottomUp::setIndex(bodies);
  T.binBodies(bodies,0);
  T.stopTimer("Set index    ",T.printNow);

  T.startTimer("Sort index   ");
  T.buffer.resize(bodies.size());
  T.sort(bodies,T.buffer);
  T.stopTimer("Sort index   ",T.printNow);

  T.startTimer("Nth element  ");
  bigint nthGlobal = numBodies * T.commSize() / 3;
  bigint iSplit = T.nth_element(bodies,nthGlobal);
  int nthLocal = T.splitBodies(bodies,iSplit);
  T.stopTimer("Nth element  ",T.printNow);
  for( B_iter B=bodies.begin(); B!=bodies.end(); ++B ) {
    B->I = B-bodies.begin() > nthLocal;
  }

#ifdef VTK
  if( T.commRank() == 0 ) {
    int Ncell = 0;
    vtkPlot vtk;
    vtk.setDomain(T.getR0(),T.getX0());
    vtk.setGroupOfPoints(bodies,Ncell);
    vtk.plot(Ncell);
  }
#endif
}
