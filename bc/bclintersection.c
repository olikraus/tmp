/*

  bclintersection.c
  
  boolean cube list: calculate intersection

*/
#include "bc.h"
#include <assert.h>

/*
  calculates the intersection of a and b and stores the result into "result"
  this will apply SCC
*/
int bcp_IntersectionBCLs(bcp p, bcl result, bcl a, bcl b)
{
  int i, j;
  bc tmp;
  
  bcp_StartCubeStackFrame(p);
  tmp = bcp_GetTempCube(p);
  
  assert(result != a);
  assert(result != b);
  
  bcp_ClearBCL(p, result);
  for( i = 0; i < b->cnt; i++ )
  {
    bcp_ClearBCL(p, result);
    for( j = 0; j < a->cnt; j++ )
    {
      if ( bcp_IntersectionCube(p, tmp, bcp_GetBCLCube(p, a, j), bcp_GetBCLCube(p, b, i)) )
      {
        if ( bcp_AddBCLCubeByCube(p, result, tmp) < 0 )
          return bcp_EndCubeStackFrame(p), 0;
      }
    }
  }
  
  bcp_DoBCLSingleCubeContainment(p, result);

  bcp_EndCubeStackFrame(p);  
  return 1;
}

/* a = a intersection with b, result has SCC property */
int bcp_IntersectionBCL(bcp p, bcl a, bcl b)
{
  bcl result = bcp_NewBCL(p);
  
  if ( bcp_IntersectionBCLs(p, result, a, b) == 0 )
    return bcp_DeleteBCL(p, result), 0;
  
  bcp_CopyBCL(p, a, result);
  bcp_DeleteBCL(p, result);
  return 1;
}


