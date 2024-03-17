/*

  bclcontainment.c
  
  boolean cube list: containment
  
*/

#include "bc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>


/*
  In the given BCL, ensure, that no cube is part of any other cube
  This will call bcp_PurgeBCL()
*/
void bcp_DoBCLSingleCubeContainment(bcp p, bcl l)
{
  int i, j;
  int cnt = l->cnt;
  bc c;
  int vc;
  
  /*
    calculate the number of 01 and 10 codes for each of the cubes in "l"
    idea is to reduce the number of "subset" tests, because for a cube with n variables,
    can be a subset of another cube only if this other cube has lesser variables.
    however: also the requal case is checked, to check wether two cubes are identical
  */
  int *vcl = bcp_GetBCLVarCntList(p, l);

  
  for( i = 0; i < cnt; i++ )
  {
    if ( l->flags[i] == 0 )
    {
      c = bcp_GetBCLCube(p, l, i);
      vc = vcl[i];
      for( j = 0; j < cnt; j++ )
      {
        if ( l->flags[j] == 0 )
        {
          if ( i != j )
          {
            /*
              test, whether "b" is a subset of "a"
              returns:      
                1: yes, "b" is a subset of "a"
                0: no, "b" is not a subset of "a"
              A mandatory condition for this is, that b has more variables than a
            */
            if ( vcl[j] >= vc )
            {
              if ( bcp_IsSubsetCube(p, c, bcp_GetBCLCube(p, l, j)) != 0 )
              {
                l->flags[j] = 1;      // mark the j cube as deleted
              }
            }
          } // j != i
        } // j cube not deleted
      } // j loop
    } // i cube not deleted
  } // i loop
  bcp_PurgeBCL(p, l);
  free(vcl);
}
