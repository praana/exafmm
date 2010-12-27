#include "body.h"
#include "tree.h"
#include "vtk.h"

class cell {
  int NLEAF;
  int NCHILD;
  bigint I;
  bodies *LEAF;
  cell *PARENT;
  cell *CHILD[8];
  coef *M,*L;
};

int main()
{
  double tic,toc;
  int const Nbody=100000;
  bodies B(Nbody,Nbody);
  bodies B2(Nbody,Nbody);
  Tree T(B);

  tic = get_time();
  for( B=B.begin(); B!=B.end(); ++B ) {                         // Loop over all bodies
    for( int d=0; d!=3; ++d )                                   //  Loop over each dimension
      B.pos()[d] = rand()/(1.+RAND_MAX)*2-1;                    //   Initialize positions
    real r = sqrt(B.pos()[0]*B.pos()[0]+B.pos()[1]*B.pos()[1]+B.pos()[2]*B.pos()[2]);
    for( int d=0; d!=3; ++d )                                   //  Loop over each dimension
      B.pos()[d] /= r;                                          //   Normalize positions
    B.scal() = 1./B.size();                                     //  Initialize source value
  }                                                             // End loop over all bodies
  toc = get_time();
  std::cout << "Initialize    : " << toc-tic << std::endl;

  tic = get_time();
  T.setDomain();
  toc = get_time();
  std::cout << "Set domain    : " << toc-tic << std::endl;

  tic = get_time();
  bigint *index = new bigint [Nbody];
  T.getMorton(index);
  toc = get_time();
  std::cout << "Get Morton    : " << toc-tic << std::endl;

  tic = get_time();
  T.sort(index,B,B2);
  toc = get_time();
  std::cout << "Sort Morton   : " << toc-tic << std::endl;

  tic = get_time();
  T.grow(index,B2);
  T.sort(index,B,B2);
  toc = get_time();
  std::cout << "Grow tree     : " << toc-tic << std::endl;

  int Ncell(0);
  vtkPlot vtk;
  vtk.setDomain(T.getR0(),T.getX0());
  vtk.setGroupOfPoints(index,B,Ncell);
  vtk.plot(Ncell);
  T.sortDealloc();
  delete[] index;
}
