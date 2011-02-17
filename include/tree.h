#ifndef tree_h
#define tree_h
#include "sort.h"
#include "kernel.h"

class TreeStructure : public Sort {
private:
  C_iter CI0;                                                   // icells.begin()
  C_iter CJ0;                                                   // jcells.begin()

protected:
  Pairs  pairs;                                                 // Stack of interaction pairs
  vect   X0;                                                    // Center of root cell
  real   R0;                                                    // Radius of root cell
  Kernel K;                                                     // Kernels

public:
  Bodies buffer;                                                // Buffer for comm & sort

private:
  bigint getParent(bigint index) {                              // Get parent cell index from current cell index
    int level = getLevel(index);                                // Get level from cell index
    bigint cOff = ((1 << 3 *  level   ) - 1) / 7;               // Cell index offset of current level
    bigint pOff = ((1 << 3 * (level-1)) - 1) / 7;               // Cell index offset of parent level
    bigint i = ((index-cOff) >> 3) + pOff;                      // Cell index of parent cell
    return i;                                                   // Return cell index of parent cell
  }

  void unique(Cells &cells, Cells &sticks, int begin, int &end) {// Merge sticks with cells (levelwise)
    int c_old = begin;                                          // Initialize old cell counter
    for( int c=begin; c!=end; ++c ) {                           // Loop over all cells in level
      if( cells[c].I != cells[c_old].I ) {                      //  If current cell index is different from previous
        c_old = c;                                              //   Update old cell counter
      } else if( c != c_old ) {                                 //  If cell index is repeated
        if( cells[c].NCHILD != 0 ) {                            //   Stick-cell collision
          cells[c_old].NCHILD = cells[c].NCHILD;                //    Copy number of children
          cells[c_old].NLEAF  = cells[c].NLEAF;                 //    Copy number of leafs
          cells[c_old].PARENT = cells[c].PARENT;                //    Copy parent link
          for( int i=0; i!=cells[c].NCHILD; ++i ) {             //    Loop over children
            cells[c_old].CHILD[i] = cells[c].CHILD[i];          //     Copy child link
          }                                                     //    End loop over children
          cells[c_old].LEAF   = cells[c].LEAF;                  //    Copy iterator of first leaf
          sticks.push_back(cells[c_old]);                       //    Push stick into vector
        }                                                       //   Endif for collision type
        cells[c_old].M += cells[c].M;                           //   Accumulate multipole
        cells.erase(cells.begin()+c);                           //   Erase colliding cell
        c--;                                                    //   Decrement counter to account for erase
        end--;                                                  //   Decrement end to account for erase
      }                                                         //  Endif for repeated cell index
    }                                                           // End loop over all cells in level
  }

  void linkParent(Cells &cells, int &begin, int &end) {         // Form parent-child mutual link
    Cell parent;                                                // Define parent cell
    Cells parents;                                              // Define parent cell vector;
    int oldend = end;                                           // Save old end counter
    parent.I = getParent(cells[begin].I);                       // Set cell index
    parent.M = parent.NLEAF = parent.NCHILD = 0;                // Initialize multipole, NLEAF & NCHILD
    parent.LEAF = cells[begin].LEAF;                            // Set pointer to first leaf
    getCenter(parent);                                          // Set cell center and radius
    for( int i=begin; i!=oldend; ++i ) {                        // Loop over all cells at this level
      if( getParent(cells[i].I) != parent.I ) {                 //  If it belongs to a new parent cell
        cells.push_back(parent);                                //   Push cells into vector
        end++;                                                  //   Increment cell counter
        parent.I = getParent(cells[i].I);                       //   Set cell index
        parent.M = parent.NLEAF = parent.NCHILD = 0;            //   Initialize multipole, NLEAF & NCHILD
        parent.LEAF = cells[i].LEAF;                            //   Set pointer to first leaf
        getCenter(parent);                                      //   Set cell center and radius
      }                                                         //  Endif for new parent cell
      for( int c=0; c!=cells[i].NCHILD; ++c ) {                 //  Loop over child cells
        (cells.begin()+cells[i].CHILD[c])->PARENT = i;          //   Link child to current
      }                                                         //  End loop over child cells
      cells[i].PARENT = end;                                    //  Link to current to parent
      parent.NLEAF += cells[i].NLEAF;                           //  Add nleaf of child to parent
      parents.push_back(parent);                                //  Push parent cell into vector
      K.M2M(parents.begin(),cells.begin()+i);                   //  Calculate M of parent with M2M
      parent = parents.back();                                  //  Copy information from vector
      parents.pop_back();                                       //  Pop parent cell from vector
      parent.CHILD[parent.NCHILD] = i;                          //  Link to child
      parent.NCHILD++;                                          //  Increment child counter
    }                                                           // End loop over all cells at this level
    cells.push_back(parent);                                    // Push cells into vector
    end++;                                                      // Increment cell counter
    begin = oldend;                                             // Set new begin index to old end index
  }

protected:
  int getLevel(bigint index) {                                  // Get level from cell index
    int level = -1;                                             // Initialize level counter
    while( index >= 0 ) {                                       // While cell index is non-negative
      level++;                                                  //  Increment level
      index -= 1 << 3*level;                                    //  Subtract number of cells in that level
    }                                                           // End while loop for cell index
    return level;                                               // Return the level
  }

  void getCenter(Cell &cell) {                                  // Get cell center and radius from cell index
    int level = getLevel(cell.I);                               // Get level from cell index
    bigint index = cell.I - ((1 << 3*level) - 1) / 7;           // Subtract cell index offset of current level
    cell.R = R0 / (1 << level);                                 // Cell radius
    int d = level = 0;                                          // Initialize dimension and level
    vec<3,int> nx = 0;                                          // Initialize 3-D cell index
    while( index != 0 ) {                                       // Deinterleave bits while index is nonzero
      nx[d] += (index % 2) * (1 << level);                      //  Add deinterleaved bit to 3-D cell index
      index >>= 1;                                              //  Right shift the bits
      d = (d+1) % 3;                                            //  Increment dimension
      if( d == 0 ) level++;                                     //  If dimension is 0 again, increment level
    }                                                           // End while loop for deinterleaving bits
    for( d=0; d!=3; ++d ) {                                     // Loop over dimensions
      cell.X[d] = (X0[d]-R0) + (2 *nx[d] + 1) * cell.R;         //  Calculate cell center from 3-D cell index
    }                                                           // End loop over dimensions
  }

  void sortCells(Cells &cells, bool ascend=true, int begin=0, int end=0) {// Sort cells according to cell index
    if( end == 0 ) end = cells.size();                          // Default size is all cells
    Cells cbuffer;                                              // Define vector for sort buffer
    cbuffer.resize(cells.size());                               // Resize vector for sort buffer
    sort(cells,cbuffer,ascend,begin,end);                       // Sort cells in descending order
  }

  void bodies2twigs(Bodies &bodies, Cells &twigs){              // Group bodies into twig cells
    int nleaf = 0;                                              // Initialize number of leafs
    bigint index = bodies[0].I;                                 // Initialize cell index
    B_iter firstLeaf = bodies.begin();                          // Initialize body iterator for first leaf
    Cell cell;                                                  // Define cell
    for( B_iter B=bodies.begin(); B!=bodies.end(); ++B ) {      // Loop over all bodies
      if( B->I != index ) {                                     //  If it belongs to a new cell
        cell.NLEAF = nleaf;                                     //   Set number of leafs
        cell.NCHILD = 0;                                        //   Set number of child cells
        cell.I = index;                                         //   Set cell index
        cell.LEAF = firstLeaf;                                  //   Set pointer to first leaf
        getCenter(cell);                                        //   Set cell center and radius
        twigs.push_back(cell);                                  //   Push cells into vector
        firstLeaf = B;                                          //   Set new first leaf
        nleaf = 0;                                              //   Reset number of bodies
        index = B->I;                                           //   Set new cell
      }                                                         //  Endif for new cell
      nleaf++;                                                  //  Increment body counter
    }                                                           // End loop over bodies
    cell.NLEAF = nleaf;                                         // Set number of leafs
    cell.NCHILD = 0;                                            // Set number of child cells
    cell.I = index;                                             // Set cell index
    cell.LEAF = firstLeaf;                                      // Set pointer to first leaf
    getCenter(cell);                                            // Set cell center and radius
    twigs.push_back(cell);                                      // Push cells into vector
  }

public:
  TreeStructure() : X0(0),R0(0) {}                              // Constructor
  ~TreeStructure() {}                                           // Destructor

  vect getX0() {return X0;}                                     // Get center of root cell
  real getR0() {return R0;}                                     // Get radius of root cell
  void setCI0(C_iter ci0) { CI0 = ci0; }                        // Set icells.begin()
  void setCJ0(C_iter cj0) { CJ0 = cj0; }                        // Set jcells.begin()

  void setDomain(Bodies &bodies) {                              // Set center and size of root cell
    vect xmin,xmax;                                             // Min,Max of domain
    B_iter B = bodies.begin();                                  // Reset body iterator
    xmin = xmax = B->pos;                                       // Initialize xmin,xmax
    for( B=bodies.begin(); B!=bodies.end(); ++B ) {             // Loop over all bodies
      for( int d=0; d!=3; ++d ) {                               //  Loop over each dimension
        if     (B->pos[d] < xmin[d]) xmin[d] = B->pos[d];       //   Determine xmin
        else if(B->pos[d] > xmax[d]) xmax[d] = B->pos[d];       //   Determine xmax
      }                                                         //  End loop over each dimension
      X0 += B->pos;                                             //  Sum positions
    }                                                           // End loop over all bodies
    X0 /= bodies.size();                                        // Calculate average position
    for( int d=0; d!=3; ++d ) {                                 // Loop over each dimension
      X0[d] = int(X0[d]+.5);                                    //  Shift center to nearest integer
      R0 = std::max(xmax[d] - X0[d], R0);                       //  Calculate max distance from center
      R0 = std::max(X0[d] - xmin[d], R0);                       //  Calculate max distance from center
    }                                                           // End loop over each dimension
    R0 = pow(2.,int(1. + log(R0) / M_LN2));                     // Add some leeway to root radius
  }

  void twigs2cells(Cells &twigs, Cells &cells, Cells &sticks) { // Link twigs bottomup to create all cells in tree
    int begin = 0, end = 0;                                     // Initialize range of cell vector
    int level = getLevel(twigs.back().I);                       // Initialize level of tree
    while( !twigs.empty() ) {                                   // Keep poppig twigs until the vector is empty
      while( getLevel(twigs.back().I) != level ) {              //  While cell belongs to a higher level
        sortCells(cells,false,begin,end);                       //   Sort cells at this level
        unique(cells,sticks,begin,end);                         //   Get rid of duplicate cells
        linkParent(cells,begin,end);                            //   Form parent-child mutual link
        level--;                                                //   Go up one level
      }                                                         //  End while for higher level
      cells.push_back(twigs.back());                            // Push cells into vector
      twigs.pop_back();                                         // Pop twigs from vector
      end++;                                                    // Increment cell counter
    }                                                           // End while for popping twigs
    for( int l=level; l>0; --l ) {                              // Once all the twigs are done, do the rest
      sortCells(cells,false,begin,end);                         //  Sort cells at this level
      unique(cells,sticks,begin,end);                           //  Get rid of duplicate cells
      linkParent(cells,begin,end);                              //  Form parent-child mutual link
    }                                                           // End loop over levels
    unique(cells,sticks,begin,end);                             // Just in case there is a collision at root
  }

  void P2M(Cells &cells) {                                      // Interface for P2M kernel
    for( C_iter C=cells.begin(); C!=cells.end(); ++C ) {        // Loop over all cells
      C->M = 0;                                                 //  Initialize multipole coefficients
      C->L = 0;                                                 //  Initialize local coefficients
      if( C->NLEAF < NCRIT ) {                                  //  If cell is a twig
        K.P2M(C);                                               //   Evaluate P2M kernel
      }                                                         //  Endif for twigs
    }                                                           // End loop over cells
  }

  void M2M(Cells &cells) {                                      // Interface for M2M kernel
    for( C_iter C=cells.begin(); C!=cells.end()-1; ++C ) {      // Loop over all cells bottomup (except root cell)
      K.M2M(CJ0+C->PARENT,C);                               //  Evaluate M2M kernel
    }                                                           // End loop over cells
  }

  void M2P(C_iter CI, C_iter CJ) {                              // Interface for M2P kernel
    vect dist = CI->X - CJ->X;                                  // Distance vector between cells
    real R = std::sqrt(norm(dist));                             // Distance between cells
    if( CI->NCHILD != 0 || CI->R + CJ->R > THETA*R ) {          // If target is not twig or box is too large
      Pair pair(CI,CJ);                                         //  Form pair of interacting cells
      pairs.push(pair);                                         //  Push interacting pair into stack
    } else {                                                    // If target is twig and box is small enough
      K.M2P(CI,CJ);                                             //  Evaluate M2P kernel
    }                                                           // Endif for interaction
  }

  void treecode(C_iter CI, C_iter CJ) {                         // Tree walk for treecode
    if( CI->NCHILD == 0 && CJ->NCHILD == 0) {                   // If both cells are twigs
      K.P2P(CI->LEAF,CI->LEAF+CI->NLEAF,CJ->LEAF,CJ->LEAF+CJ->NLEAF);// Evaluate P2P kernel
    } else if ( CI->NCHILD != 0 ) {                             // If target is not twig
      for( int i=0; i<CI->NCHILD; i++ ) {                       //  Loop over child cells of target
        M2P(CI0+CI->CHILD[i],CJ);                               //   Try to evaluate M2P kernel
      }                                                         //  End loop over child cells of target
    } else {                                                    // If target is twig
      for( int i=0; i<CJ->NCHILD; i++ ) {                       //  Loop over child cells of source
        M2P(CI,CJ0+CJ->CHILD[i]);                               //   Try to evaluate M2P kernel
      }                                                         //  End loop over child cells of source
    }                                                           // Endif for type of interaction
  }

  void M2L(C_iter CI, C_iter CJ) {                              // Interface for M2L kernel
    vect dist = CI->X - CJ->X;                                  // Distance vector between cells
    real R = std::sqrt(norm(dist));                             // Distance between cells
    if( CI->R + CJ->R > THETA*R ) {                             // If box is too large
      Pair pair(CI,CJ);                                         //  Form pair of interacting cells
      pairs.push(pair);                                         //  Push interacting pair into stack
    } else {                                                    // If bos is small enough
      K.M2L(CI,CJ);                                             //  Evaluate M2L kernel
    }                                                           // Endif for interaction
  }

  void FMM(C_iter CI, C_iter CJ) {                              // Tree walk for FMM
    if( CI->NCHILD == 0 && CJ->NCHILD == 0 ) {                  // If both cells are twigs
      if( CJ->NLEAF != 0 ) {                                    // If the twig has leafs
        K.P2P(CI->LEAF,CI->LEAF+CI->NLEAF,CJ->LEAF,CJ->LEAF+CJ->NLEAF);// Evaluate P2P kernel
      } else {                                                  // If the twig has no leafs
#ifdef DEBUG
        std::cout << "CJ->I=" << CJ->I << " has no leaf. Doing M2P instead." << std::endl;
#endif
        K.M2P(CI,CJ);                                           //  Evaluate M2P kernel
      }                                                         // Endif for twigs with leafs
    } else if ( CJ->NCHILD == 0 || (CI->NCHILD != 0 && CI->R > CJ->R) ) {// If source is twig or target is larger
      for( int i=0; i<CI->NCHILD; i++ ) {                       //  Loop over child cells of target
        M2L(CI0+CI->CHILD[i],CJ);                               //   Try to evaluate M2L kernel
      }                                                         //  End loop over child cells of target
    } else {                                                    // If target is twig or source is larger
      for( int i=0; i<CJ->NCHILD; i++ ) {                       //  Loop over child cells of source
        M2L(CI,CJ0+CJ->CHILD[i]);                               //   Try to evaluate M2L kernel
      }                                                         //  End loop over child cells of source
    }                                                           // Endif for type of interaction
  }

  void evaluate(Cells &cells, int method) {                     // Interface for treewalk
    C_iter root = cells.end()-1;                                // Iterator for root cell
    Pair pair(root,root);                                       // Form pair of root cells
    pairs.push(pair);                                           // Push pair to stack
    while( !pairs.empty() ) {                                   // While interaction stack is not empty
      pair = pairs.top();                                       //  Get interaction pair from top of stack
      pairs.pop();                                              //  Pop interaction stack
      switch (method) {                                         //  Swtich between methods
        case 0 : treecode(pair.CI,pair.CJ); break;              //   0 : treecode
        case 1 : FMM(pair.CI,pair.CJ);      break;              //   1 : FMM
      }                                                         //  End switch between methods
    }                                                           // End while loop for interaction stack
    for( C_iter C=root-1; C!=cells.begin()-1; --C ) {           // Loop over all cells topdown (except root cell)
      K.L2L(C,CI0+C->PARENT);                                   //  Evaluate L2L kernel
      if( C->NLEAF < NCRIT ) {                                  //  If cell is a twig
        K.L2P(C);                                               //   Evaluate L2P kernel
      }                                                         //  Endif for twig
    }                                                           // End loop over all cells topdown
  }

  void evaluate(Cells &cells, Cells &jcells, int method) {      // Interface for treewalk
    C_iter root = cells.end()-1;                                // Iterator for root cell
    C_iter jroot = jcells.end()-1;                              // Iterator for root cell
    Pair pair(root,jroot);                                      // Form pair of root cells
    pairs.push(pair);                                           // Push pair to stack
    while( !pairs.empty() ) {                                   // While interaction stack is not empty
      pair = pairs.top();                                       //  Get interaction pair from top of stack
      pairs.pop();                                              //  Pop interaction stack
      switch (method) {                                         //  Swtich between methods
        case 0 : treecode(pair.CI,pair.CJ); break;              //   0 : treecode
        case 1 : FMM(pair.CI,pair.CJ);      break;              //   1 : FMM
      }                                                         //  End switch between methods
    }                                                           // End while loop for interaction stack
    for( C_iter C=root-1; C!=cells.begin()-1; --C ) {           // Loop over all cells topdown (except root cell)
      K.L2L(C,CI0+C->PARENT);                                   //  Evaluate L2L kernel
      if( C->NLEAF < NCRIT ) {                                  //  If cell is a twig
        K.L2P(C);                                               //   Evaluate L2P kernel
      }                                                         //  Endif for twig
    }                                                           // End loop over all cells topdown
  }

};

#endif
