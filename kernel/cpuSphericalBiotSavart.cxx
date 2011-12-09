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
#define KERNEL
#include "kernel.h"
#undef KERNEL
#include "biotsavart.h"

template<>
void Kernel<BiotSavart>::initialize() {}

template<>
void Kernel<BiotSavart>::P2M(C_iter Ci) const {
  for( B_iter B=Ci->LEAF; B!=Ci->LEAF+Ci->NDLEAF; ++B ) {
    vect dist = B->X - Ci->X;
    real rho, alpha, beta;
    cart2sph(rho,alpha,beta,dist);
    evalMultipole(rho,alpha,-beta);
    for( int n=0; n!=P; ++n ) {
      for( int m=0; m<=n; ++m ) {
        const int nm  = n * n + n + m;
        const int nms = n * (n + 1) / 2 + m;
        for( int d=0; d!=3; ++d ) {
          Ci->M[3*nms+d] += double(B->SRC[d]) * Ynm[nm];
        }
      }
    }
  }
}

template<>
void Kernel<BiotSavart>::M2M(C_iter Ci, C_iter Cj) const {
  const complex I(0.,1.);                                       // Imaginary unit
  vect dist = Ci->X - Cj->X;
  real rho, alpha, beta;
  cart2sph(rho,alpha,beta,dist);
  evalMultipole(rho,alpha,-beta);
  for( int j=0; j!=P; ++j ) {
    for( int k=0; k<=j; ++k ) {
      const int jk = j * j + j + k;
      const int jks = j * (j + 1) / 2 + k;
      complex M[3] = {0, 0, 0};
      for( int n=0; n<=j; ++n ) {
        for( int m=-n; m<=std::min(k-1,n); ++m ) {
          if( j-n >= k-m ) {
            const int jnkm  = (j - n) * (j - n) + j - n + k - m;
            const int jnkms = (j - n) * (j - n + 1) / 2 + k - m;
            const int nm    = n * n + n + m;
            for( int d=0; d!=3; ++d ) {
              M[d] += Cj->M[3*jnkms+d] * std::pow(I,double(m-abs(m))) * Ynm[nm]
                    * double(ODDEVEN(n) * Anm[nm] * Anm[jnkm] / Anm[jk]);
            }
          }
        }
        for( int m=k; m<=n; ++m ) {
          if( j-n >= m-k ) {
            const int jnkm  = (j - n) * (j - n) + j - n + k - m;
            const int jnkms = (j - n) * (j - n + 1) / 2 - k + m;
            const int nm    = n * n + n + m;
            for( int d=0; d!=3; ++d ) {
              M[d] += std::conj(Cj->M[3*jnkms+d]) * Ynm[nm]
                    * double(ODDEVEN(k+n+m) * Anm[nm] * Anm[jnkm] / Anm[jk]);
            }
          }
        }
      }
      for( int d=0; d!=3; ++d ) {
        Ci->M[3*jks+d] += M[d];
      }
    }
  }
}

template<>
void Kernel<BiotSavart>::M2L(C_iter Ci, C_iter Cj) const {
  vect dist = Ci->X - Cj->X - Xperiodic;
  real rho, alpha, beta;
  cart2sph(rho,alpha,beta,dist);
  evalLocal(rho,alpha,beta);
  for( int j=0; j!=P; ++j ) {
    for( int k=0; k<=j; ++k ) {
      const int jk = j * j + j + k;
      const int jks = j * (j + 1) / 2 + k;
      complex L[3] = {0, 0, 0};
      for( int n=0; n!=P; ++n ) {
        for( int m=-n; m<0; ++m ) {
          const int nm   = n * n + n + m;
          const int nms  = n * (n + 1) / 2 - m;
          const int jknm = jk * P2 + nm;
          const int jnkm = (j + n) * (j + n) + j + n + m - k;
          for( int d=0; d!=3; ++d ) {
            L[d] += std::conj(Cj->M[3*nms+d]) * Cnm[jknm] * Ynm[jnkm];
          }
        }
        for( int m=0; m<=n; ++m ) {
          const int nm   = n * n + n + m;
          const int nms  = n * (n + 1) / 2 + m;
          const int jknm = jk * P2 + nm;
          const int jnkm = (j + n) * (j + n) + j + n + m - k;
          for( int d=0; d!=3; ++d ) {
            L[d] += Cj->M[3*nms+d] * Cnm[jknm] * Ynm[jnkm];
          }
        }
      }
      for( int d=0; d!=3; ++d ) {
        Ci->L[3*jks+d] += L[d];
      }
    }
  }
}

template<>
void Kernel<BiotSavart>::M2P(C_iter Ci, C_iter Cj) const {
  const complex I(0.,1.);                                       // Imaginary unit
  for( B_iter B=Ci->LEAF; B!=Ci->LEAF+Ci->NDLEAF; ++B ) {
    vect dist = B->X - Cj->X - Xperiodic;
    vect spherical[3] = {0, 0, 0};
    vect cartesian[3] = {0, 0, 0};
    real r, theta, phi;
    cart2sph(r,theta,phi,dist);
    evalLocal(r,theta,phi);
    for( int n=0; n!=P; ++n ) {
      int nm  = n * n + n;
      int nms = n * (n + 1) / 2;
      for( int d=0; d!=3; ++d ) {
        spherical[d][0] -= (Cj->M[3*nms+d] * Ynm[nm]).real() / r * (n+1);
        spherical[d][1] += (Cj->M[3*nms+d] * YnmTheta[nm]).real();
      }
      for( int m=1; m<=n; ++m ) {
        nm  = n * n + n + m;
        nms = n * (n + 1) / 2 + m;
        for( int d=0; d!=3; ++d ) {
          spherical[d][0] -= 2 * (Cj->M[3*nms+d] * Ynm[nm]).real() / r * (n+1);
          spherical[d][1] += 2 * (Cj->M[3*nms+d] * YnmTheta[nm]).real();
          spherical[d][2] += 2 * (Cj->M[3*nms+d] * Ynm[nm] * I).real() * m;
        }
      }
    }
    for( int d=0; d!=3; ++d ) {
      sph2cart(r,theta,phi,spherical[d],cartesian[d]);
    }
    B->TRG[0] += 0.25 / M_PI * (cartesian[1][2] - cartesian[2][1]);
    B->TRG[1] += 0.25 / M_PI * (cartesian[2][0] - cartesian[0][2]);
    B->TRG[2] += 0.25 / M_PI * (cartesian[0][1] - cartesian[1][0]);
  }
}

template<>
void Kernel<BiotSavart>::L2L(C_iter Ci, C_iter Cj) const {
  const complex I(0.,1.);                                       // Imaginary unit
  vect dist = Ci->X - Cj->X;
  real rho, alpha, beta;
  cart2sph(rho,alpha,beta,dist);
  evalMultipole(rho,alpha,beta);
  for( int j=0; j!=P; ++j ) {
    for( int k=0; k<=j; ++k ) {
      const int jk = j * j + j + k;
      const int jks = j * (j + 1) / 2 + k;
      complex L[3] = {0, 0, 0};
      for( int n=j; n!=P; ++n ) {
        for( int m=j+k-n; m<0; ++m ) {
          const int jnkm = (n - j) * (n - j) + n - j + m - k;
          const int nm   = n * n + n - m;
          const int nms  = n * (n + 1) / 2 - m;
          for( int d=0; d!=3; ++d ) {
            L[d] += std::conj(Cj->L[3*nms+d]) * Ynm[jnkm]
                  * double(ODDEVEN(k) * Anm[jnkm] * Anm[jk] / Anm[nm]);
          }
        }
        for( int m=0; m<=n; ++m ) {
          if( n-j >= abs(m-k) ) {
            const int jnkm = (n - j) * (n - j) + n - j + m - k;
            const int nm   = n * n + n + m;
            const int nms  = n * (n + 1) / 2 + m;
            for( int d=0; d!=3; ++d ) {
              L[d] += Cj->L[3*nms+d] * std::pow(I,double(m-k-abs(m-k))) * Ynm[jnkm]
                    * double(Anm[jnkm] * Anm[jk] / Anm[nm]);
            }
          }
        }
      }
      for( int d=0; d!=3; ++d ) {
        Ci->L[3*jks+d] += L[d];
      }
    }
  }
}

template<>
void Kernel<BiotSavart>::L2P(C_iter Ci) const {
  const complex I(0.,1.);                                       // Imaginary unit
  for( B_iter B=Ci->LEAF; B!=Ci->LEAF+Ci->NDLEAF; ++B ) {
    vect dist = B->X - Ci->X;
    vect spherical[3] = {0, 0, 0};
    vect cartesian[3] = {0, 0, 0};
    real r, theta, phi;
    cart2sph(r,theta,phi,dist);
    evalMultipole(r,theta,phi);
    for( int n=0; n!=P; ++n ) {
      int nm  = n * n + n;
      int nms = n * (n + 1) / 2;
      for( int d=0; d!=3; ++d ) {
        spherical[d][0] += (Ci->L[3*nms+d] * Ynm[nm]).real() / r * n;
        spherical[d][1] += (Ci->L[3*nms+d] * YnmTheta[nm]).real();
      }
      for( int m=1; m<=n; ++m ) {
        nm  = n * n + n + m;
        nms = n * (n + 1) / 2 + m;
        for( int d=0; d!=3; ++d ) {
          spherical[d][0] += 2 * (Ci->L[3*nms+d] * Ynm[nm]).real() / r * n;
          spherical[d][1] += 2 * (Ci->L[3*nms+d] * YnmTheta[nm]).real();
          spherical[d][2] += 2 * (Ci->L[3*nms+d] * Ynm[nm] * I).real() * m;
        }
      }
    }
    for( int d=0; d!=3; ++d ) {
      sph2cart(r,theta,phi,spherical[d],cartesian[d]);
    }
    B->TRG[0] += 0.25 / M_PI * (cartesian[1][2] - cartesian[2][1]);
    B->TRG[1] += 0.25 / M_PI * (cartesian[2][0] - cartesian[0][2]);
    B->TRG[2] += 0.25 / M_PI * (cartesian[0][1] - cartesian[1][0]);
  }
}

template<>
void Kernel<BiotSavart>::finalize() {}
