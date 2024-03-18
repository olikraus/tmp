/*

  bclsubset.c
  
  boolean cube list: check whether a bcl list is part or equal to another bcl list

  There are two ways to calculate to derive this property
    1) Via substract from each other
    2) Via check whether each cube of a bcl list is covered by the other list (cofactor version)
    
  This code defines both algorithms, but at the moment the cofactor version
  seems to be faster.
  
  This code defines also 
    bcl bcp_NewBCLComplement(bcp p, bcl l)
  which just calles "bcp_NewBCLComplementWithSubtract"
  
*/
#include "bc.h"
#include <assert.h>

/*
  test, whether "b" is a subset of "a"
  returns:      
    1: yes, "b" is a subset of "a"
    0: no, "b" is not a subset of "a"

  this function is much faster than the substract version below!

*/
int bcp_IsBCLSubsetWithCofactor(bcp p, bcl a, bcl b)
{
  int i;
  for( i = 0; i < b->cnt; i++ )
  {
    if ( bcp_IsBCLCubeCovered(p, a, bcp_GetBCLCube(p, b, i)) == 0 )
      return 0;
  }
  return 1;
}

int bcp_IsBCLSubset(bcp p, bcl a, bcl b)
{
  return bcp_IsBCLSubsetWithCofactor(p, a, b);
}

/*
  test, whether "b" is a subset of "a"
  returns:      
    1: yes, "b" is a subset of "a"
    0: no, "b" is not a subset of "a"

  SLOWER
*/
int bcp_IsBCLSubsetWithSubstract(bcp p, bcl a, bcl b)
{
  int result = 0;
  bcl tmp = bcp_NewBCLByBCL(p, b);  
  bcp_SubtractBCL(p, tmp, a, /* is_mcc */ 1);
  if ( tmp->cnt == 0 )
    result = 1;
  bcp_DeleteBCL(p, tmp);
  return result;
}

