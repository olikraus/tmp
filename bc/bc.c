/*

  bc.c
  
  boolean cube 
  
  Suggested article: R. Rudell "Multiple-Valued Logic Minimization for PLA Synthesis"
    https://www2.eecs.berkeley.edu/Pubs/TechRpts/1986/734.html
  This code will only use the binary case (and not the multiple value case) which is partly described in the above article.  

  https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#ssetechs=SSE,SSE2&ig_expand=80

  gcc -g -Wall -fsanitize=address bc.c
  
  predecessor output
  echo | gcc -dM -E -  
  echo | gcc -march=native -dM -E -
  echo | gcc -march=silvermont -dM -E -         // Pentium N3710: sse4.2 popcnt 
 

  
  === definitions ===
  
  - Variable: A boolean variable which is one (1), zero (0), don't care (-) or illegal (x)
  - Variable encoding: A variable is encoded with two bits: one=10, zero=01, don't care=11, illegal=00
  - Blk (block): A physical unit in the uC which can group multiple variables, e.g. __m128i or uint64_t
  - Cube: A vector of multiple blocks, which can hold all variables of a boolean cube problem. Usually this is interpreted as a "product" ("and" operation) of the boolean variables
  - Boolean cube problem: A master structure with a given number of boolean variables ("var_cnt")  
  
  === boolean cube problems ===
  
  bcp bcp_New(size_t var_cnt)                                           Create new boolean cube problem object
  void bcp_Delete(bcp p)                                                        Clear a boolean cube problem object
  void bcp_StartCubeStackFrame(bcp p);
  void bcp_EndCubeStackFrame(bcp p);
  bc bcp_GetTempCube(bcp p);

  === boolean cube ===

  void bcp_ClrCube(bcp p, bc c)                                         Clear a copy. All variables within the cube are set to "don't care"
  const char *bcp_GetStringFromCube(bcp p, bc c)           Get a visual representation of a cube     
  void bcp_SetCubeByString(bcp p, bc c, const char *s)    Use the visual (human readable) representation and assign it to a cube

  === boolean cube list ===

  bcl bcp_NewBCL(bcp p)                                 Create new boolean cube list
  bc bcp_GetBCLCube(bcp p, bcl l, int pos)      Get cube from list "l" at position "pos"
  void bcp_ShowBCL(bcp p, bcl l)                     Print cube list to stdout 
  int bcp_AddBCLCube(bcp p, bcl l)                                                      Add a new cube to the list, return the cube position within the list
  int bcp_AddBCLCubesByString(bcp p, bcl l, const char *s)              Add none, one or more cubes from the given strings to the list


  Notes:
    Expand should not expand and store the result for one variable, instead
    we should generate more expanded cubes, based on the one original cube, for example if we have
      -10-110-
      Then the expand my produce 
      --0-110-
      which might not allow other expansions. However there might be also
      -1--110-
      
      

*/

#include <stdalign.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "bc.h"


/*============================================================*/


/*
static void print128_num(__m128i var)
{
    uint16_t val[8];
    memcpy(val, &var, sizeof(val));
    printf("m128i: %04x %04x %04x %04x %04x %04x %04x %04x \n", 
           val[0], val[1], 
           val[2], val[3], 
           val[4], val[5], 
           val[6], val[7]);
}
*/

void print128_num(__m128i var)
{
    uint8_t val[16];
    memcpy(val, &var, sizeof(val));
    printf("m128i: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x \n", 
           val[0], val[1], 
           val[2], val[3], 
           val[4], val[5], 
           val[6], val[7],
           val[8], val[9], 
           val[10], val[11], 
           val[12], val[13], 
           val[14], val[15]
  );
}




/*============================================================*/



/*============================================================*/








/*
  try to expand cubes into another cube
  includes bcp_DoBCLSingleCubeContainment
*/
void bcp_DoBCLSimpleExpand(bcp p, bcl l)
{
  int i, j, v;
  int k;
  int cnt = l->cnt;
  int delta;
  int cval, dval;
  bc c, d;
  for( i = 0; i < cnt; i++ )
  {
    if ( l->flags[i] == 0 )
    {
      c = bcp_GetBCLCube(p, l, i);
      for( j = i+1; j < cnt; j++ )
      {
        if ( l->flags[j] == 0 )
        {
          //if ( i != j )
          {
            d = bcp_GetBCLCube(p, l, j); 
            delta = bcp_GetCubeDelta(p, c, d);
            //printf("delta=%d\n", delta);
            
            if ( delta == 1 )
            {
              // search for the variable, which causes the delta
              for( v = 0; v < p->var_cnt; v++ )
              {
                cval = bcp_GetCubeVar(p, c, v);
                dval = bcp_GetCubeVar(p, d, v);
                if ( (cval & dval) == 0 )
                {
                  break;
                }
              }
              if ( v < p->var_cnt )
              {
                //printf("v=%d\n", v);
                bcp_SetCubeVar(p, c, v, 3-cval);
                /*
                  test, whether second arg is a subset of first
                  returns:      
                    1: yes, second arg is a subset of first arg
                    0: no, second arg is not a subset of first arg
                */
                if ( bcp_IsSubsetCube(p, d, c) )
                {
                  // great, expand would be successful
                  bcp_SetCubeVar(p, c, v, 3);  // expand the cube, by adding don't care to that variable
                  //printf("v=%d success c\n", v);

                  /* check whether other cubes are covered by the new cube */
                  for( k = 0; k < cnt; k++ )
                  {
                    if ( k != j && k != i && l->flags[k] == 0)
                    {
                      if ( bcp_IsSubsetCube(p, c, bcp_GetBCLCube(p, l, k)) != 0 )
                      {
                        l->flags[k] = 1;      // mark the k cube as deleted
                      }
                    }
                  } // for k
                }
                else
                {
                  bcp_SetCubeVar(p, c, v, cval);  // revert variable
                  bcp_SetCubeVar(p, d, v, 3-dval);
                  if ( bcp_IsSubsetCube(p, c, d) )
                  {
                    // expand of d would be successful
                    bcp_SetCubeVar(p, d, v, 3);  // expand the d cube, by adding don't care to that variable
                    //printf("v=%d success d\n", v);
                    for( k = 0; k < cnt; k++ )
                    {
                      if ( k != j && k != i && l->flags[k] == 0)
                      {
                        if ( bcp_IsSubsetCube(p, d, bcp_GetBCLCube(p, l, k)) != 0 )
                        {
                          l->flags[k] = 1;      // mark the k cube as deleted
                        }
                      }
                    } // for k
                  }
                  else
                  {
                    bcp_SetCubeVar(p, d, v, dval);  // revert variable
                  }
                }
              }
            }
          } // j != i
        } // j cube not deleted
      } // j loop
    } // i cube not deleted
  } // i loop
  bcp_PurgeBCL(p, l);
}

void bcp_DoBCLExpandWithOffSet(bcp p, bcl l, bcl off)
{
  int i, j, v;
  bc c;
  int cval;
  for( i = 0; i < l->cnt; i++ )
  {
    if ( l->flags[i] == 0 )
    {
      c = bcp_GetBCLCube(p, l, i);
      for( v = 0; v < p->var_cnt; v++ )
      {
        cval = bcp_GetCubeVar(p, c, v);
        if ( cval != 3 )
        {
          bcp_SetCubeVar(p, c, v, 3);
          for( j = 0; j < off->cnt; j++ )
          {
            if ( off->flags[j] == 0 )
            {
              if ( bcp_IsIntersectionCube(p, c, bcp_GetBCLCube(p, off, j)) != 0 )
              {
                bcp_SetCubeVar(p, c, v, cval);    // there is an intersection with the off-set, so undo the don't care
                break;
              }
            }
          }
        }
      }
    }
  }
}


/*
  with the cube at postion "pos" within "l", check whether there are any other cubes, which are a subset of the cobe at postion "pos"
  The cubes, which are marked as subset are not deleted. This should done by a later call to bcp_BCLPurge()
*/
void bcp_DoBCLSubsetCubeMark(bcp p, bcl l, int pos)
{
  int j;
  int cnt = l->cnt;
  bc c = bcp_GetBCLCube(p, l, pos);
  for( j = 0; j < cnt; j++ )
  {
    if ( j != pos && l->flags[j] == 0  )
    {
      /*
        test, whether "b" is a subset of "a"
        returns:      
          1: yes, "b" is a subset of "a"
          0: no, "b" is not a subset of "a"
      */
      if ( bcp_IsSubsetCube(p, c, bcp_GetBCLCube(p, l, j)) != 0 )
      {
        l->flags[j] = 1;      // mark the j cube as covered (to be deleted)
      }            
    }
  }  
}


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














/*
  try to reduce the total number of element in "l" via some heuristics.
  The reduction is done by:
    - Widen the existing cubes
    - Removing redundant cubes
  The minimization does not:
    - Create new cubes (primes) which may cover existing terms better
*/
void bcp_MinimizeBCL(bcp p, bcl l)
{
  bcp_DoBCLSingleCubeContainment(p, l);         // do an initial SCC simplification
  bcl complement = bcp_NewBCLComplementWithSubtract(p, l);      // calculate the complement for the expand algo
  bcp_DoBCLExpandWithOffSet(p, l, complement);          // expand all terms as far es possible
  bcp_DoBCLSingleCubeContainment(p, l);                         // do another SCC as a preparation for the MCC
  bcp_DoBCLMultiCubeContainment(p, l);                          // finally do multi cube minimization
}



/*============================================================*/

