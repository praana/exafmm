#ifndef body_h
#define body_h
#include <assert.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include "gettime.h"
#include "vec.h"

typedef int long long bigint;
typedef float real;
const int NCRIT = 1000;
typedef vec<3,real> vect;
typedef vec<20,real> coef;
const real EPS = 0.01;

struct body
{
  vect pos;
  real scal;
  vect acc;
  real pot;
};

class bodies
{
  int I;
  int const N;
  int const NMAX;
  body *P;
public:
  bodies(int const n, int const nmax) : N(n),NMAX(nmax) {
    P = new body [NMAX];
  }
  ~bodies() {
    delete[] P;
  }
  vect &pos() const {
    return P[I].pos;
  }
  real &scal() const {
    return P[I].scal;
  }
  vect &acc() const {
    return P[I].acc;
  }
  real &pot() const {
    return P[I].pot;
  }
  vect &pos(int i) const {
    return P[i].pos;
  }
  int begin() {
    I = 0;
    return I;
  }
  int end() const {
    return N;
  }
  int size() const {
    return N;
  }
  int index() const {
    return I;
  }
  body &operator[](int const i) const {
    return P[i];
  }
  int const &operator=(int i) {
    return I = i;
  }
  bodies const &operator++() {
    ++I;
    return *this;
  }
  bool operator!=(int i) const {
    return I != i;
  }
  operator int () {return I;}
  friend std::ostream &operator<<(std::ostream &s, bodies const &P) {
    s<<P.I;
    return s;
  }
};
#endif
