/*

  bclsubstract.c
  
  boolean cube list: substract / difference calculation
  
  this file also contains the "sharp" which does the substract on cube level
  
  The core function is the bcp_DoBCLSharpOperation() which
  calculations a-b and adds the result to a given list "l"
  
  The substract algo can be used to
    - check whether a bcl is a subset of another bcl
    - calculate the complement of a given bcl
    
  In both cases there are alternative algorithms available
    - the subset test can be done by a subset check for each cube (via tautology test)
    - the calculation of the complement can be done via a cofactor split as
        long as there is a binate variable. 
  
*/

#include "bc.h"
#include <assert.h>

/*
  Subtract cube b from a: a#b. All cubes resulting from this operation are appended to l if
    - the new cube is valid
    - the new cube is not covered by any other cube in l
  After adding a cube, existing cubes are checked to be a subset of the newly added cube and marked for deletion if so
*/
static void bcp_DoBCLSharpOperation(bcp p, bcl l, bc a, bc b)
{
  int i;
  unsigned bb;
  unsigned orig_aa;
  unsigned new_aa;
  //int pos;

  for( i = 0; i < p->var_cnt; i++ )
  {
    bb = bcp_GetCubeVar(p, b, i);
    if ( bb != 3 )
    {
      orig_aa = bcp_GetCubeVar(p, a, i); 
      new_aa = orig_aa & (bb^3);
      if ( new_aa != 0 )
      {
        bcp_SetCubeVar(p, a, i, new_aa);        // modify a 
        bcp_AddBCLCubeByCube(p, l, a); // if a is not subcube of any existing cube, then add the modified a cube to the list
        bcp_SetCubeVar(p, a, i, orig_aa);        // undo the modification
      }
    }
  }
}

/* 
  a = a - b 
  is_mcc: whether to execute multi cube containment or not
  if is_mcc is 0, then the substract operation will generate all prime cubes.
  if b is unate, then executing mcc slows down the substract, otherwise if b is binate, then using mcc increases performance
*/
void bcp_SubtractBCL(bcp p, bcl a, bcl b, int is_mcc)
{
  int i, j;
  bcl result = bcp_NewBCL(p);
  for( i = 0; i < b->cnt; i++ )
  {
    bcp_ClearBCL(p, result);
    for( j = 0; j < a->cnt; j++ )
    {
      bcp_DoBCLSharpOperation(p, result, bcp_GetBCLCube(p, a, j), bcp_GetBCLCube(p, b, i));
    }
    bcp_CopyBCL(p, a, result);
    bcp_DoBCLSingleCubeContainment(p, a);
    if ( is_mcc )
      bcp_DoBCLMultiCubeContainment(p, a);
  }
  bcp_DeleteBCL(p, result);
}

