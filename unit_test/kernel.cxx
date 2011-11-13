#include "evaluator.h"

int main() {
  const int numBodies = 100;
  IMAGES = 0;
  THETA = 1/sqrtf(3);
  Bodies ibodies(numBodies);
  Bodies ibodies2(numBodies);
  Bodies jbodies(numBodies);
  Cells  icells;
  Cells  jcells;
  Evaluator<Laplace> FMM;
  FMM.initialize();
  FMM.preCalculation();

  for( int it=0; it!=10; ++it ) {
    real dist = (1 << it) / 2;
    for( B_iter B=ibodies.begin(); B!=ibodies.end(); ++B ) {
      for( int d=0; d!=3; ++d ) {
        B->X[d] = -rand() / (1. + RAND_MAX) - dist;
      }
    }
    for( B_iter B=jbodies.begin(); B!=jbodies.end(); ++B ) {
      for( int d=0; d!=3; ++d ) {
        B->X[d] = rand() / (1. + RAND_MAX);
      }
    }
    FMM.initSource(jbodies);
    bool IeqJ = false;
    FMM.initTarget(ibodies,IeqJ);

    Cell cell;
    cell.NLEAF    = numBodies;
    cell.LEAF     = jbodies.begin();
    cell.X        = 0.5;
    cell.M        = 0;
    cell.ICELL    = 8;
    cell.NCHILD   = 0;
    cell.PARENT   = 1;
    jcells.push_back(cell);
    FMM.evalP2M(jcells);
    cell.X        = 1;
    cell.M        = 0;
    cell.ICELL    = 0;
    cell.NCHILD   = 1;
    cell.CHILD[0] = 0;
    jcells.push_back(cell);
    FMM.evalM2M(jcells);
    jcells.erase(jcells.begin());
    cell.X        = -1 - dist;
    cell.L        = 0;
    cell.ICELL    = 0;
    icells.push_back(cell);
    FMM.addM2L(jcells.begin());
    FMM.evalM2L(icells,true);
    cell.NLEAF    = numBodies;
    cell.LEAF     = ibodies.begin();
    cell.X        = -0.5 - dist;
    cell.L        = 0;
    cell.ICELL    = 1;
    cell.NCHILD   = 0;
    cell.PARENT   = 1;
    icells.insert(icells.begin(),cell);
    FMM.evalL2L(icells);
    icells.pop_back();
    FMM.evalL2P(icells);

    ibodies2 = ibodies;
    FMM.initTarget(ibodies2,IeqJ);
    FMM.evalP2P(ibodies2,jbodies);

    real diff1 = 0, norm1 = 0, diff2 = 0, norm2 = 0;
    FMM.evalError(ibodies,ibodies2,diff1,norm1,diff2,norm2);
    std::cout << "Distance      : " << dist << std::endl;
    FMM.printError(diff1,norm1,diff2,norm2);

    FMM.initTarget(ibodies);
    FMM.addM2P(jcells.begin());
    FMM.evalM2P(icells,true);
    icells.clear();
    jcells.clear();
    diff1 = norm1 = diff2 = norm2 = 0;
    FMM.evalError(ibodies,ibodies2,diff1,norm1,diff2,norm2);
    FMM.printError(diff1,norm1,diff2,norm2);
  }
  FMM.postCalculation();
  FMM.finalize();
}
