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



/*============================================================*/


/*
  return a list with the variable count for each cube in the list.
  returns an allocated array, which must be free'd by the calling function
*/
int *bcp_GetBCLVarCntList(bcp p, bcl l)
{
  int i;
  int *vcl = (int *)malloc(sizeof(int)*l->cnt);  
  assert(l != NULL);
  if ( vcl == NULL )
    return NULL;
  for( i = 0; i < l->cnt; i++ )
  {
    if ( l->flags[i] == 0 )
    {
      vcl[i] = bcp_GetCubeVariableCount(p, bcp_GetBCLCube(p,l,i));
    }
    else
    {
      vcl[i] = -1;
    }
  }
  return vcl;
}


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

/*
  checks, whether the given cube "c" is a subset (covered) of the list "l"
  cube "c" must not be physical part of the list. if this is the case use bcp_IsBCLCubeRedundant() instead
  
*/
int bcp_IsBCLCubeCovered(bcp p, bcl l, bc c)
{
  bcl n = bcp_NewBCLCofactorByCube(p, l, c, -1);
  int result = bcp_IsBCLTautology(p, n);
  bcp_DeleteBCL(p, n);
  return result;
}

/*
  checks, whether the cube at position "pos" is a subset of all other cubes in the same list.
*/
int bcp_IsBCLCubeRedundant(bcp p, bcl l, int pos)
{
  bcl n = bcp_NewBCLCofactorByCube(p, l, bcp_GetBCLCube(p, l, pos), pos);
  int result = bcp_IsBCLTautology(p, n);
  bcp_DeleteBCL(p, n);
  return result;
}

/*
  IRREDUNDANT 
*/
void bcp_DoBCLMultiCubeContainment(bcp p, bcl l)
{
  int i;
  int *vcl = bcp_GetBCLVarCntList(p, l);
  int min = p->var_cnt;
  int max = 0;
  int vc;

  for( i = 0; i < l->cnt; i++ )
  {
    if ( l->flags[i] == 0 )
    {
      if ( min > vcl[i] )
        min = vcl[i];
      if ( max < vcl[i] )
        max = vcl[i];
    }
  }

  for( vc = max; vc >= min; vc-- )      // it seems to be faster to start checking the smallest cubes first
  {
    for( i = 0; i < l->cnt; i++ )
    {
      if ( l->flags[i] == 0 && vcl[i] == vc )
      {
        if ( bcp_IsBCLCubeRedundant(p, l, i) )
        {
          l->flags[i] = 1;
        }
        
      } // i cube not deleted
    } // i loop
  } // vc loop
  free(vcl);
  bcp_PurgeBCL(p, l);
  
}


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
  Subtract cube b from a: a#b. All cubes resulting from this operation are appended to l if
    - the new cube is valid
    - the new cube is not covered by any other cube in l
  After adding a cube, existing cubes are checked to be a subset of the newly added cube and marked for deletion if so
*/
void bcp_DoBCLSharpOperation(bcp p, bcl l, bc a, bc b)
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


/* a = a - b 
  is_mcc: whether to execute multi cube containment or not
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
  Calculate the cofactor of the given list "l" with respect to the variable at "var_pos" and the given "value".
  "value" must be either 1 (zero) or 2 (one)

*/
void bcp_DoBCLOneVariableCofactor(bcp p, bcl l, unsigned var_pos, unsigned value)
{
  int i;
  int cnt = l->cnt;
  unsigned v;
  bc c;
  
  assert(value == 1 || value == 2);
  
  //printf("bcp_DoBCLOneVariableCofactor pre var_pos=%d value=%d\n", var_pos, value);
  //bcp_ShowBCL(p, l);
  for( i = 0; i < cnt; i++ )
  {
    if ( l->flags[i] == 0 )
    {
      c = bcp_GetBCLCube(p, l, i);
      v = bcp_GetCubeVar(p, c, var_pos);
      if ( v != 3 )  // is the variable already don't care for the cofactor variable?
      {
        if ( (v | value) == 3 ) // if no, then check if the variable would become don't care
        {
          bcp_SetCubeVar(p, c, var_pos, 3);   // yes, variable will become don't care
          bcp_DoBCLSubsetCubeMark(p, l, i);
        } // check for "becomes don't care'
      } // check for not don't care
    }
  } // i loop
  bcp_PurgeBCL(p, l);  // cleanup for bcp_DoBCLSubsetCubeMark()
}

bcl bcp_NewBCLCofacterByVariable(bcp p, bcl l, unsigned var_pos, unsigned value)
{
  bcl n = bcp_NewBCLByBCL(p, l);
  if ( n == NULL )
    return NULL;
  bcp_DoBCLOneVariableCofactor(p, n, var_pos, value);
  return n;
}


/*
  Calculate the cofactor of l against c
  c might be part of l
  additionally ignore the element at position exclude (which is assumed to be c)
  exclude can be negative
*/
void bcp_DoBCLCofactorByCube(bcp p, bcl l, bc c, int exclude)
{
  int i;
  int b;
  bc lc;
  __m128i cc;
  __m128i dc;
  
  dc = _mm_loadu_si128(bcp_GetGlobalCube(p, 3));
  
  if ( exclude >= 0 )
    l->flags[exclude] = 1;
  
  for( b = 0; b < p->blk_cnt; b++ )
  {
    cc = _mm_andnot_si128( _mm_loadu_si128(c+b), dc);
    
    for( i = 0; i < l->cnt; i++ )
    {
      if ( l->flags[i] == 0 )
      {
        lc = bcp_GetBCLCube(p, l, i);
        _mm_storeu_si128(lc+b,  _mm_or_si128(cc, _mm_loadu_si128(lc+b)));
      }
    }
  }
  bcp_DoBCLSingleCubeContainment(p, l);
}

bcl bcp_NewBCLCofactorByCube(bcp p, bcl l, bc c, int exclude)
{
  bcl n = bcp_NewBCLByBCL(p, l);
  if ( n == NULL )
    return NULL;
  bcp_DoBCLCofactorByCube(p, n, c, exclude);
  return n;
}





/*
  do an "and" between all elements of l
  the result of this operation is stored in cube "r"
  Interpretation:
    If "r" is illegal (contains one or more 00 codes), then the list has the BINATE property
    Varibales, which have the illegal code, can be used for the cofactor split operation
    If "r" is not illegal, then the list has the UNATE property
      A list with the UNATE property has the tautology property if there is the tautology block included
*/
void bcp_AndBCL(bcp p, bc r, bcl l)
{
  int i, blk_cnt = p->blk_cnt;
  int j, list_cnt = l->cnt;
  bc lc;
  __m128i m;
  bcp_CopyGlobalCube(p, r, 3);          // global cube 3 has all vars set to don't care (tautology block)
  

  for( i = 0; i < blk_cnt; i++ )
  {
    m = _mm_loadu_si128(r+i);
    for( j = 0; j < list_cnt; j++ )
    {
        if ( (l->flags[j]&1) == 0 )             // check delete flag
        {
          lc = bcp_GetBCLCube(p, l, j);
          m = _mm_and_si128(m, _mm_loadu_si128(lc+i));
        }
    }
    _mm_storeu_si128(r+i, m);
  }
}



/* 16 bit version */
void bcp_CalcBCLBinateSplitVariableTable(bcp p, bcl l)
{
	int i, blk_cnt = p->blk_cnt;
	int j, list_cnt = l->cnt;
	
	bc zero_cnt_cube[8];
	bc one_cnt_cube[8];
	

	__m128i c;  // current block from the current cube from the list
	__m128i t;	// temp block
	__m128i oc0, oc1, oc2, oc3, oc4, oc5, oc6, oc7;	// one count
	__m128i zc0, zc1, zc2, zc3, zc4, zc5, zc6, zc7;	// zero count
	__m128i mc; // mask cube for the lowest bit in each byte

	/* constuct the byte mask: we need the lowest bit of each byte */
	mc = _mm_setzero_si128();
	mc = _mm_insert_epi16(mc, 1, 0);
	mc = _mm_insert_epi16(mc, 1, 1);
	mc = _mm_insert_epi16(mc, 1, 2);
	mc = _mm_insert_epi16(mc, 1, 3);
	mc = _mm_insert_epi16(mc, 1, 4);
	mc = _mm_insert_epi16(mc, 1, 5);
	mc = _mm_insert_epi16(mc, 1, 6);
	mc = _mm_insert_epi16(mc, 1, 7);

	/* "misuse" the cubes as SIMD storage area for the counters */
	zero_cnt_cube[0] = bcp_GetGlobalCube(p, 4);
	zero_cnt_cube[1] = bcp_GetGlobalCube(p, 5);
	zero_cnt_cube[2] = bcp_GetGlobalCube(p, 6);
	zero_cnt_cube[3] = bcp_GetGlobalCube(p, 7);
	zero_cnt_cube[4] = bcp_GetGlobalCube(p, 8);
	zero_cnt_cube[5] = bcp_GetGlobalCube(p, 9);
	zero_cnt_cube[6] = bcp_GetGlobalCube(p, 10);
	zero_cnt_cube[7] = bcp_GetGlobalCube(p, 11);
	
	one_cnt_cube[0] = bcp_GetGlobalCube(p, 12);
	one_cnt_cube[1] = bcp_GetGlobalCube(p, 13);
	one_cnt_cube[2] = bcp_GetGlobalCube(p, 14);
	one_cnt_cube[3] = bcp_GetGlobalCube(p, 15);
	one_cnt_cube[4] = bcp_GetGlobalCube(p, 16);
	one_cnt_cube[5] = bcp_GetGlobalCube(p, 17);
	one_cnt_cube[6] = bcp_GetGlobalCube(p, 18);
	one_cnt_cube[7] = bcp_GetGlobalCube(p, 19);
	
	/* loop over the blocks */
	for( i = 0; i < blk_cnt; i++ )
	{
		/* clear all the conters for the current block */
		zc0 = _mm_setzero_si128();
		zc1 = _mm_setzero_si128();
		zc2 = _mm_setzero_si128();
		zc3 = _mm_setzero_si128();
		zc4 = _mm_setzero_si128();
		zc5 = _mm_setzero_si128();
		zc6 = _mm_setzero_si128();
		zc7 = _mm_setzero_si128();
		oc0 = _mm_setzero_si128();
		oc1 = _mm_setzero_si128();
		oc2 = _mm_setzero_si128();
		oc3 = _mm_setzero_si128();
		oc4 = _mm_setzero_si128();
		oc5 = _mm_setzero_si128();
		oc6 = _mm_setzero_si128();
		oc7 = _mm_setzero_si128();
		
		for( j = 0; j < list_cnt; j++ )
		{
                  if ( l->flags[j] == 0 )
                  {
			/*
				Goal: 
					Count, how often 01 (zero) and 10 (one) do appear in the list at a specific cube position
					This means, for a cube with x variables, we will generate 2*x numbers
					For this count we have to ignore 11 (don't care) values.
				Assumption:
					The code 00 (illegal) is assumed to be absent.
				Idea:
					Because only code 11, 01 and 10 will be there in the cubes, it will be good enough
					to count the 0 bits from 01 and 10.
					Each counter will be 8 bit only (hoping that this is enough) with satuartion at 255 (thanks to SIMD instructions)
					In order to add or not add the above "0" (from the 01 or 10 code) to the counter, we will just mask and invert the 0 bit.
				
				Example:
					Assume a one variable at bit pos 0/1:
						xxxxxx10		code for value "one" at bits 0/1
					this is inverted and masked with one SIMD instruction:
						00000001		increment value for the counter
					this value is then added (with sturation) to the "one" counter 
					if, on the other hand, there would be a don't care or zero, it would look like this:
						xxxxxx01		code for value "zero" at bits 0/1
						xxxxxx11		code for value "don't care" at bits 0/1
					the invert and mask operation for bit 0 will generate a 0 byte in both cases:
						00000000		increment value for the counter in case of "zero" and "don't care" value
			*/
			c = _mm_loadu_si128(bcp_GetBCLCube(p, l, j)+i);
			/* handle variable at bits 0/1 */
			t = _mm_andnot_si128(c, mc);		// flip the lowerst bit and mask the lowerst bit in each byte: the "10" code for value "one" will become "00000001"
			oc0 = _mm_adds_epi16(oc0, t);		// sum the "one" value with saturation
			c = _mm_srai_epi16(c,1);			// shift right to proceed with the "zero" value
			t = _mm_andnot_si128(c, mc);
			zc0 = _mm_adds_epi16(zc0, t);
			
			c = _mm_srai_epi16(c,1);			// shift right to process the next variable

			/* handle variable at bits 2/3 */
			t = _mm_andnot_si128(c, mc);
			oc1 = _mm_adds_epi16(oc1, t);
			c = _mm_srai_epi16(c,1);
			t = _mm_andnot_si128(c, mc);
			zc1 = _mm_adds_epi16(zc1, t);

			c = _mm_srai_epi16(c,1);

			/* handle variable at bits 4/5 */
			t = _mm_andnot_si128(c, mc);
			oc2 = _mm_adds_epi16(oc2, t);
			c = _mm_srai_epi16(c,1);
			t = _mm_andnot_si128(c, mc);
			zc2 = _mm_adds_epi16(zc2, t);

			c = _mm_srai_epi16(c,1);

			/* handle variable at bits 6/7 */
			t = _mm_andnot_si128(c, mc);
			oc3 = _mm_adds_epi16(oc3, t);
			c = _mm_srai_epi16(c,1);
			t = _mm_andnot_si128(c, mc);
			zc3 = _mm_adds_epi16(zc3, t);
                        
			c = _mm_srai_epi16(c,1);

			/* handle variable at bits 8/9 */
			t = _mm_andnot_si128(c, mc);
			oc4 = _mm_adds_epi16(oc4, t);
			c = _mm_srai_epi16(c,1);
			t = _mm_andnot_si128(c, mc);
			zc4 = _mm_adds_epi16(zc4, t);
                        
			c = _mm_srai_epi16(c,1);

			/* handle variable at bits 10/11 */
			t = _mm_andnot_si128(c, mc);
			oc5 = _mm_adds_epi16(oc5, t);
			c = _mm_srai_epi16(c,1);
			t = _mm_andnot_si128(c, mc);
			zc5 = _mm_adds_epi16(zc5, t);
                        
			c = _mm_srai_epi16(c,1);

			/* handle variable at bits 12/13 */
			t = _mm_andnot_si128(c, mc);
			oc6 = _mm_adds_epi16(oc6, t);
			c = _mm_srai_epi16(c,1);
			t = _mm_andnot_si128(c, mc);
			zc6 = _mm_adds_epi16(zc6, t);
                        
			c = _mm_srai_epi16(c,1);

			/* handle variable at bits 14/15 */
			t = _mm_andnot_si128(c, mc);
			oc7 = _mm_adds_epi16(oc7, t);
			c = _mm_srai_epi16(c,1);
			t = _mm_andnot_si128(c, mc);
			zc7 = _mm_adds_epi16(zc7, t);
                  }  // flag test
		
                } // j, list loop
		/* store the registers for counting zeros and ones */
		_mm_storeu_si128(zero_cnt_cube[0] + i, zc0);
		_mm_storeu_si128(zero_cnt_cube[1] + i, zc1);
		_mm_storeu_si128(zero_cnt_cube[2] + i, zc2);
		_mm_storeu_si128(zero_cnt_cube[3] + i, zc3);
		_mm_storeu_si128(zero_cnt_cube[4] + i, zc4);
		_mm_storeu_si128(zero_cnt_cube[5] + i, zc5);
		_mm_storeu_si128(zero_cnt_cube[6] + i, zc6);
		_mm_storeu_si128(zero_cnt_cube[7] + i, zc7);
		
		_mm_storeu_si128(one_cnt_cube[0] + i, oc0);
		_mm_storeu_si128(one_cnt_cube[1] + i, oc1);
		_mm_storeu_si128(one_cnt_cube[2] + i, oc2);
		_mm_storeu_si128(one_cnt_cube[3] + i, oc3);
		_mm_storeu_si128(one_cnt_cube[4] + i, oc4);
		_mm_storeu_si128(one_cnt_cube[5] + i, oc5);
		_mm_storeu_si128(one_cnt_cube[6] + i, oc6);
		_mm_storeu_si128(one_cnt_cube[7] + i, oc7);
	
        /* 
          based on the calculated number of "one" and "zero" values, find a variable which fits best for splitting.
          According to R. Rudell in "Multiple-Valued Logic Minimization for PLA Synthesis" this should be the
          variable with the highest value of one_cnt + zero_cnt
        */
      } // i, block loop
}

/*
  Precondition: call to 
    void bcp_CalcBCLBinateSplitVariableTable(bcp p, bcl l)  

  returns the binate variable for which the number of one's plus number of zero's is max under the condition, that both number of once's and zero's are >0 

  Implementation without SSE2
*/

/* 16 bit version */
int bcp_GetBCLMaxBinateSplitVariableSimple(bcp p, bcl l)
{
  int max_sum_cnt = -1;
  int max_sum_var = -1;
  int cube_idx;
  int blk_idx;
  int word_idx;
  int one_cnt;
  int zero_cnt;
  
  int i;
  
  bc zero_cnt_cube[8];
  bc one_cnt_cube[8];

  /* "misuse" the cubes as SIMD storage area for the counters */
  zero_cnt_cube[0] = bcp_GetGlobalCube(p, 4);
  zero_cnt_cube[1] = bcp_GetGlobalCube(p, 5);
  zero_cnt_cube[2] = bcp_GetGlobalCube(p, 6);
  zero_cnt_cube[3] = bcp_GetGlobalCube(p, 7);
  zero_cnt_cube[4] = bcp_GetGlobalCube(p, 8);
  zero_cnt_cube[5] = bcp_GetGlobalCube(p, 9);
  zero_cnt_cube[6] = bcp_GetGlobalCube(p, 10);
  zero_cnt_cube[7] = bcp_GetGlobalCube(p, 11);
  
  one_cnt_cube[0] = bcp_GetGlobalCube(p, 12);
  one_cnt_cube[1] = bcp_GetGlobalCube(p, 13);
  one_cnt_cube[2] = bcp_GetGlobalCube(p, 14);
  one_cnt_cube[3] = bcp_GetGlobalCube(p, 15);
  one_cnt_cube[4] = bcp_GetGlobalCube(p, 16);
  one_cnt_cube[5] = bcp_GetGlobalCube(p, 17);
  one_cnt_cube[6] = bcp_GetGlobalCube(p, 18);
  one_cnt_cube[7] = bcp_GetGlobalCube(p, 19);

  
  max_sum_cnt = -1;
  max_sum_var = -1;
  for( i = 0; i < p->var_cnt; i++ )
  {
          cube_idx = i & 7;
          blk_idx = i / 64;
          word_idx = (i & 63)>>3;
          one_cnt = ((uint16_t *)(one_cnt_cube[cube_idx] + blk_idx))[word_idx];
          zero_cnt = ((uint16_t *)(zero_cnt_cube[cube_idx] + blk_idx))[word_idx];
          //printf("%02x/%02x ", one_cnt, zero_cnt);
    
          if ( one_cnt > 0 && zero_cnt > 0 )
          {
            if ( max_sum_cnt < (one_cnt + zero_cnt) )
            {
                    max_sum_cnt = one_cnt + zero_cnt;
                    max_sum_var = i;
            }
          }
  }  
  //printf("\n");
  return max_sum_var;
}



/*
  Precondition: call to 
    void bcp_CalcBCLBinateSplitVariableTable(bcp p, bcl l)  

  returns the binate variable for which the number of one's plus number of zero's is max under the condition, that both number of once's and zero's are >0 

  SSE2 Implementation
*/
/* 16 bit version */
int bcp_GetBCLMaxBinateSplitVariable(bcp p, bcl l)
{
  int max_sum_cnt = -1;
  int max_sum_var = -1;
  int i, b;
  __m128i c;
  __m128i z;
  __m128i o;
  
  __m128i c_cmp = _mm_setzero_si128();
  __m128i c_max = _mm_setzero_si128();
  __m128i c_idx = _mm_setzero_si128();
  __m128i c_max_idx = _mm_setzero_si128();
  
  uint16_t m_base_idx[8] = { 0, 8, 16, 24,  32, 40, 48, 56 };
  uint16_t m_base_inc[8] = { 1, 1, 1, 1,   1, 1, 1, 1 };
  
  uint16_t m_idx[8];
  uint16_t m_max[8];
  
  bc zero_cnt_cube[8];
  bc one_cnt_cube[8];

  if ( l->cnt == 0 )
    return -1;

  /* "misuse" the cubes as SIMD storage area for the counters */
  zero_cnt_cube[0] = bcp_GetGlobalCube(p, 4);
  zero_cnt_cube[1] = bcp_GetGlobalCube(p, 5);
  zero_cnt_cube[2] = bcp_GetGlobalCube(p, 6);
  zero_cnt_cube[3] = bcp_GetGlobalCube(p, 7);
  zero_cnt_cube[4] = bcp_GetGlobalCube(p, 8);
  zero_cnt_cube[5] = bcp_GetGlobalCube(p, 9);
  zero_cnt_cube[6] = bcp_GetGlobalCube(p, 10);
  zero_cnt_cube[7] = bcp_GetGlobalCube(p, 11);
  
  one_cnt_cube[0] = bcp_GetGlobalCube(p, 12);
  one_cnt_cube[1] = bcp_GetGlobalCube(p, 13);
  one_cnt_cube[2] = bcp_GetGlobalCube(p, 14);
  one_cnt_cube[3] = bcp_GetGlobalCube(p, 15);
  one_cnt_cube[4] = bcp_GetGlobalCube(p, 16);
  one_cnt_cube[5] = bcp_GetGlobalCube(p, 17);
  one_cnt_cube[6] = bcp_GetGlobalCube(p, 18);
  one_cnt_cube[7] = bcp_GetGlobalCube(p, 19);

  for( b = 0; b < p->blk_cnt; b++ )
  {
      c_idx = _mm_loadu_si128((__m128i *)m_base_idx);
      c_max = _mm_setzero_si128();
      c_max_idx = _mm_setzero_si128();
    
      for( i = 0; i < 8; i++ )
      {
        z = _mm_loadu_si128(zero_cnt_cube[i]+b);
        o = _mm_loadu_si128(one_cnt_cube[i]+b);

        /*
        printf("a%d: ", i);
        print128_num(o);
        
        printf("a%d: ", i);
        print128_num(z);
        */
        
        // idea: if either count is zero, then set also the other count to zero
        // because we don't want any index on unate variables, we clear both counts if one count is zero
        c = _mm_cmpeq_epi16(z, _mm_setzero_si128());     // check for zero count of zeros
        o = _mm_andnot_si128 (c, o);                                    // and clear the one count if the zero count is zero
        c = _mm_cmpeq_epi16(o, _mm_setzero_si128());     // check for zero count of ones
        z = _mm_andnot_si128 (c, z);                                    // and clear the zero count if the one count is zero

        /*
        printf("b     %d: ", i);
        print128_num(o);
        
        printf("b     %d: ", i);
        print128_num(z);
        */
        
        // at this point either both o and z are zero or both are not zero        
        // now, calculate the sum of both counts and store the sum in z, o is not required any more
        z = _mm_adds_epi16(z, o);

        /*
        printf("z     %d: ", i);
        print128_num(z);
        */
        
        c_cmp = _mm_cmplt_epi16( c_max, z );

        c_max = _mm_or_si128( _mm_andnot_si128(c_cmp, c_max), _mm_and_si128(c_cmp, z) );                        // update max value if required
        c_max_idx = _mm_or_si128( _mm_andnot_si128(c_cmp, c_max_idx), _mm_and_si128(c_cmp, c_idx) );    // update index value if required        
        
        c_idx = _mm_adds_epu16(c_idx, _mm_loadu_si128((__m128i *)m_base_inc));   // we just add 1 to the value, so the highest value per byte is 64
      }
      
      _mm_storeu_si128( (__m128i *)m_max, c_max );
      _mm_storeu_si128( (__m128i *)m_idx, c_max_idx );
      for( i = 0; i < 8; i++ )
        if ( m_max[i] > 0 )
          if ( max_sum_cnt < m_max[i] )
          {
            max_sum_cnt = m_max[i];
            max_sum_var = m_idx[i] + b*p->vars_per_blk_cnt;
          }
  }
  
  /*
  {
    int mv = bcp_GetBCLMaxBinateSplitVariableSimple(p, l);
    if ( max_sum_var != mv )
    {
      printf("failed max_sum_var=%d, mv=%d\n", max_sum_var, mv);
      //bcp_ShowBCL(p, l);
    }
  }
  */
  
  return max_sum_var;
}

/*
  this differs from 
    bcp_GetBCLMaxBinateSplitVariable(bcp p, bcl l)
  by also considering unate variables

NOT TESTED
*/

int bcp_GetBCLMaxSplitVariable(bcp p, bcl l)
{
  int max_sum_cnt = -1;
  int max_sum_var = -1;
  int i, b;
  __m128i z;
  __m128i o;
  
  __m128i c_cmp = _mm_setzero_si128();
  __m128i c_max = _mm_setzero_si128();
  __m128i c_idx = _mm_setzero_si128();
  __m128i c_max_idx = _mm_setzero_si128();
  
  uint16_t m_base_idx[8] = { 0, 8, 16, 24,  32, 40, 48, 56 };
  uint16_t m_base_inc[8] = { 1, 1, 1, 1,   1, 1, 1, 1 };
  
  uint16_t m_idx[8];
  uint16_t m_max[8];
  
  bc zero_cnt_cube[8];
  bc one_cnt_cube[8];

  if ( l->cnt == 0 )
    return -1;
  
  
  /* "misuse" the cubes as SIMD storage area for the counters */
  zero_cnt_cube[0] = bcp_GetGlobalCube(p, 4);
  zero_cnt_cube[1] = bcp_GetGlobalCube(p, 5);
  zero_cnt_cube[2] = bcp_GetGlobalCube(p, 6);
  zero_cnt_cube[3] = bcp_GetGlobalCube(p, 7);
  zero_cnt_cube[4] = bcp_GetGlobalCube(p, 8);
  zero_cnt_cube[5] = bcp_GetGlobalCube(p, 9);
  zero_cnt_cube[6] = bcp_GetGlobalCube(p, 10);
  zero_cnt_cube[7] = bcp_GetGlobalCube(p, 11);
  
  one_cnt_cube[0] = bcp_GetGlobalCube(p, 12);
  one_cnt_cube[1] = bcp_GetGlobalCube(p, 13);
  one_cnt_cube[2] = bcp_GetGlobalCube(p, 14);
  one_cnt_cube[3] = bcp_GetGlobalCube(p, 15);
  one_cnt_cube[4] = bcp_GetGlobalCube(p, 16);
  one_cnt_cube[5] = bcp_GetGlobalCube(p, 17);
  one_cnt_cube[6] = bcp_GetGlobalCube(p, 18);
  one_cnt_cube[7] = bcp_GetGlobalCube(p, 19);

  for( b = 0; b < p->blk_cnt; b++ )
  {
      c_idx = _mm_loadu_si128((__m128i *)m_base_idx);
      c_max = _mm_setzero_si128();
      c_max_idx = _mm_setzero_si128();
    
      for( i = 0; i < 8; i++ )
      {
        z = _mm_loadu_si128(zero_cnt_cube[i]+b);
        o = _mm_loadu_si128(one_cnt_cube[i]+b);

        // calculate the sum of both counts and store the sum in z, o is not required any more
        z = _mm_adds_epi16(z, o);

        c_cmp = _mm_cmplt_epi16( c_max, z );

        c_max = _mm_or_si128( _mm_andnot_si128(c_cmp, c_max), _mm_and_si128(c_cmp, z) );                        // update max value if required
        c_max_idx = _mm_or_si128( _mm_andnot_si128(c_cmp, c_max_idx), _mm_and_si128(c_cmp, c_idx) );    // update index value if required        
        
        c_idx = _mm_adds_epu16(c_idx, _mm_loadu_si128((__m128i *)m_base_inc));   // we just add 1 to the value, so the highest value per byte is 64
      }
      
      _mm_storeu_si128( (__m128i *)m_max, c_max );
      _mm_storeu_si128( (__m128i *)m_idx, c_max_idx );
      for( i = 0; i < 8; i++ )
        if ( m_max[i] > 0 )
          if ( max_sum_cnt < m_max[i] )
          {
            max_sum_cnt = m_max[i];
            max_sum_var = m_idx[i] + b*p->vars_per_blk_cnt;
          }
  }
  return max_sum_var;
}


/*
  Precondition: call to 
    void bcp_CalcBCLBinateSplitVariableTable(bcp p, bcl l)  

  return 0 if there is any variable which has one's and "zero's in the table
  otherwise this function returns 1
*/
int bcp_IsBCLUnate8(bcp p)
{
  int b;
  bc zero_cnt_cube[4];
  bc one_cnt_cube[4];
  __m128i z;
  __m128i o;

  /* "misuse" the cubes as SIMD storage area for the counters */
  zero_cnt_cube[0] = bcp_GetGlobalCube(p, 4);
  zero_cnt_cube[1] = bcp_GetGlobalCube(p, 5);
  zero_cnt_cube[2] = bcp_GetGlobalCube(p, 6);
  zero_cnt_cube[3] = bcp_GetGlobalCube(p, 7);
  
  one_cnt_cube[0] = bcp_GetGlobalCube(p, 8);
  one_cnt_cube[1] = bcp_GetGlobalCube(p, 9);
  one_cnt_cube[2] = bcp_GetGlobalCube(p, 10);
  one_cnt_cube[3] = bcp_GetGlobalCube(p, 11);

  for( b = 0; b < p->blk_cnt; b++ )
  {
        z = _mm_cmpeq_epi8(_mm_loadu_si128(zero_cnt_cube[0]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
        o = _mm_cmpeq_epi8(_mm_loadu_si128(one_cnt_cube[0]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
                       // if ones and zeros are present, then both values are 0x00 and the reseult of the or is also 0x00
                      // this means, if there is any bit in c set to 0, then BCL is not unate and we can finish this procedure
        if ( _mm_movemask_epi8(_mm_or_si128(o, z)) != 0x0ffff )
          return 0;
        
        z = _mm_cmpeq_epi8(_mm_loadu_si128(zero_cnt_cube[1]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
        o = _mm_cmpeq_epi8(_mm_loadu_si128(one_cnt_cube[1]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
                       // if ones and zeros are present, then both values are 0x00 and the reseult of the or is also 0x00
                      // this means, if there is any bit in c set to 0, then BCL is not unate and we can finish this procedure
        if ( _mm_movemask_epi8(_mm_or_si128(o, z)) != 0x0ffff )
          return 0;

        z = _mm_cmpeq_epi8(_mm_loadu_si128(zero_cnt_cube[2]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
        o = _mm_cmpeq_epi8(_mm_loadu_si128(one_cnt_cube[2]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
                       // if ones and zeros are present, then both values are 0x00 and the reseult of the or is also 0x00
                      // this means, if there is any bit in c set to 0, then BCL is not unate and we can finish this procedure
        if ( _mm_movemask_epi8(_mm_or_si128(o, z)) != 0x0ffff )
          return 0;

        z = _mm_cmpeq_epi8(_mm_loadu_si128(zero_cnt_cube[3]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
        o = _mm_cmpeq_epi8(_mm_loadu_si128(one_cnt_cube[3]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
                       // if ones and zeros are present, then both values are 0x00 and the reseult of the or is also 0x00
                      // this means, if there is any bit in c set to 0, then BCL is not unate and we can finish this procedure
        if ( _mm_movemask_epi8(_mm_or_si128(o, z)) != 0x0ffff )
          return 0;
  }
  return 1;
}


/* 16 bit version */
int bcp_IsBCLUnate(bcp p)
{
  int b;
  bc zero_cnt_cube[8];
  bc one_cnt_cube[8];
  __m128i z;
  __m128i o;

  /* "misuse" the cubes as SIMD storage area for the counters */
  zero_cnt_cube[0] = bcp_GetGlobalCube(p, 4);
  zero_cnt_cube[1] = bcp_GetGlobalCube(p, 5);
  zero_cnt_cube[2] = bcp_GetGlobalCube(p, 6);
  zero_cnt_cube[3] = bcp_GetGlobalCube(p, 7);
  zero_cnt_cube[4] = bcp_GetGlobalCube(p, 8);
  zero_cnt_cube[5] = bcp_GetGlobalCube(p, 9);
  zero_cnt_cube[6] = bcp_GetGlobalCube(p, 10);
  zero_cnt_cube[7] = bcp_GetGlobalCube(p, 11);
  
  one_cnt_cube[0] = bcp_GetGlobalCube(p, 12);
  one_cnt_cube[1] = bcp_GetGlobalCube(p, 13);
  one_cnt_cube[2] = bcp_GetGlobalCube(p, 14);
  one_cnt_cube[3] = bcp_GetGlobalCube(p, 15);
  one_cnt_cube[4] = bcp_GetGlobalCube(p, 16);
  one_cnt_cube[5] = bcp_GetGlobalCube(p, 17);
  one_cnt_cube[6] = bcp_GetGlobalCube(p, 18);
  one_cnt_cube[7] = bcp_GetGlobalCube(p, 19);

  for( b = 0; b < p->blk_cnt; b++ )
  {
        z = _mm_cmpeq_epi8(_mm_loadu_si128(zero_cnt_cube[0]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
        o = _mm_cmpeq_epi8(_mm_loadu_si128(one_cnt_cube[0]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
                       // if ones and zeros are present, then both values are 0x00 and the reseult of the or is also 0x00
                      // this means, if there is any bit in c set to 0, then BCL is not unate and we can finish this procedure
        if ( _mm_movemask_epi8(_mm_or_si128(o, z)) != 0x0ffff )
          return 0;
        
        z = _mm_cmpeq_epi8(_mm_loadu_si128(zero_cnt_cube[1]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
        o = _mm_cmpeq_epi8(_mm_loadu_si128(one_cnt_cube[1]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
                       // if ones and zeros are present, then both values are 0x00 and the reseult of the or is also 0x00
                      // this means, if there is any bit in c set to 0, then BCL is not unate and we can finish this procedure
        if ( _mm_movemask_epi8(_mm_or_si128(o, z)) != 0x0ffff )
          return 0;

        z = _mm_cmpeq_epi8(_mm_loadu_si128(zero_cnt_cube[2]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
        o = _mm_cmpeq_epi8(_mm_loadu_si128(one_cnt_cube[2]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
                       // if ones and zeros are present, then both values are 0x00 and the reseult of the or is also 0x00
                      // this means, if there is any bit in c set to 0, then BCL is not unate and we can finish this procedure
        if ( _mm_movemask_epi8(_mm_or_si128(o, z)) != 0x0ffff )
          return 0;

        z = _mm_cmpeq_epi8(_mm_loadu_si128(zero_cnt_cube[3]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
        o = _mm_cmpeq_epi8(_mm_loadu_si128(one_cnt_cube[3]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
                       // if ones and zeros are present, then both values are 0x00 and the reseult of the or is also 0x00
                      // this means, if there is any bit in c set to 0, then BCL is not unate and we can finish this procedure
        if ( _mm_movemask_epi8(_mm_or_si128(o, z)) != 0x0ffff )
          return 0;
        
        
        

        z = _mm_cmpeq_epi8(_mm_loadu_si128(zero_cnt_cube[4]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
        o = _mm_cmpeq_epi8(_mm_loadu_si128(one_cnt_cube[4]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
                       // if ones and zeros are present, then both values are 0x00 and the reseult of the or is also 0x00
                      // this means, if there is any bit in c set to 0, then BCL is not unate and we can finish this procedure
        if ( _mm_movemask_epi8(_mm_or_si128(o, z)) != 0x0ffff )
          return 0;

        z = _mm_cmpeq_epi8(_mm_loadu_si128(zero_cnt_cube[5]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
        o = _mm_cmpeq_epi8(_mm_loadu_si128(one_cnt_cube[5]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
                       // if ones and zeros are present, then both values are 0x00 and the reseult of the or is also 0x00
                      // this means, if there is any bit in c set to 0, then BCL is not unate and we can finish this procedure
        if ( _mm_movemask_epi8(_mm_or_si128(o, z)) != 0x0ffff )
          return 0;

        z = _mm_cmpeq_epi8(_mm_loadu_si128(zero_cnt_cube[6]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
        o = _mm_cmpeq_epi8(_mm_loadu_si128(one_cnt_cube[6]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
                       // if ones and zeros are present, then both values are 0x00 and the reseult of the or is also 0x00
                      // this means, if there is any bit in c set to 0, then BCL is not unate and we can finish this procedure
        if ( _mm_movemask_epi8(_mm_or_si128(o, z)) != 0x0ffff )
          return 0;

        z = _mm_cmpeq_epi8(_mm_loadu_si128(zero_cnt_cube[7]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
        o = _mm_cmpeq_epi8(_mm_loadu_si128(one_cnt_cube[7]+b), _mm_setzero_si128());     // 0 cnt becomes 0xff and all other values become 0x00
                       // if ones and zeros are present, then both values are 0x00 and the reseult of the or is also 0x00
                      // this means, if there is any bit in c set to 0, then BCL is not unate and we can finish this procedure
        if ( _mm_movemask_epi8(_mm_or_si128(o, z)) != 0x0ffff )
          return 0;
        
        
  }
  return 1;
}


// the unate check is faster than the split var calculation, but if the BCL is binate, then both calculations have to be done
// so the unate precheck does not improve performance soo much (maybe 5%)
//#define BCL_TAUTOLOGY_WITH_UNATE_PRECHECK
int bcp_IsBCLTautologySub(bcp p, bcl l, int depth)
{
  int var_pos;
  bcl f1;
  bcl f2;
  
#ifdef BCL_TAUTOLOGY_WITH_UNATE_PRECHECK  
  int is_unate;
#endif
  
  assert(depth < 2000);
  
  if ( l->cnt == 0 )
    return 0;
  
  bcp_CalcBCLBinateSplitVariableTable(p, l);

  // if bcp_IsBCLUnate() return 1, then bcp_GetBCLMaxBinateSplitVariable() will return -1 !
  // however bcp_IsBCLUnate() is much faster then bcp_GetBCLMaxBinateSplitVariable()
  
#ifdef BCL_TAUTOLOGY_WITH_UNATE_PRECHECK  
  is_unate = bcp_IsBCLUnate(p);
  if ( is_unate )
  {
    int i, cnt = l->cnt;
    for( i = 0; i < cnt; i++ )
    {
      if ( bcp_IsTautologyCube(p, bcp_GetBCLCube(p, l, i)) )
        return 1;
    }
    return 0;
  }
#endif

  var_pos = bcp_GetBCLMaxBinateSplitVariable(p, l);     // bcp_GetBCLMaxBinateSplitVariableSimple has similar performance
#ifndef BCL_TAUTOLOGY_WITH_UNATE_PRECHECK  
  if ( var_pos < 0 )
  {
    int i, cnt = l->cnt;
    for( i = 0; i < cnt; i++ )
    {
      if ( bcp_IsTautologyCube(p, bcp_GetBCLCube(p, l, i)) )
        return 1;
    }
    return 0;
  }
#endif
  
  //printf("depth %d, split var %d, size %d\n", depth, var_pos, l->cnt);
  /*
  if ( var_pos < 0 )
  {
    printf("split var simple %d\n", bcp_GetBCLMaxBinateSplitVariableSimple(p, l));
  }
  */

  assert( var_pos >= 0 );
  f1 = bcp_NewBCLCofacterByVariable(p, l, var_pos, 1);
  f2 = bcp_NewBCLCofacterByVariable(p, l, var_pos, 2);
  assert( f1 != NULL );
  assert( f2 != NULL );

  if ( bcp_IsBCLTautologySub(p, f1, depth+1) == 0 )
    return bcp_DeleteBCL(p,  f1), bcp_DeleteBCL(p,  f2), 0;
  if ( bcp_IsBCLTautologySub(p, f2, depth+1) == 0 )
    return bcp_DeleteBCL(p,  f1), bcp_DeleteBCL(p,  f2), 0;
  

  return bcp_DeleteBCL(p,  f1), bcp_DeleteBCL(p,  f2), 1;
}

int bcp_IsBCLTautology(bcp p, bcl l)
{
  return bcp_IsBCLTautologySub(p, l, 0);
}

/*
  test, whether "b" is a subset of "a"
  returns:      
    1: yes, "b" is a subset of "a"
    0: no, "b" is not a subset of "a"

  this function is much faster than the substract version!

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

/*
  test, whether "b" is a subset of "a"
  returns:      
    1: yes, "b" is a subset of "a"
    0: no, "b" is not a subset of "a"
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


/*
  looks like the complement with substract is much faster. than the cofactor version
*/
bcl bcp_NewBCLComplementWithSubtract(bcp p, bcl l)
{
    bcl result = bcp_NewBCL(p);
    int is_mcc = 1;
    bcp_CalcBCLBinateSplitVariableTable(p, l);
    if ( bcp_IsBCLUnate(p) )
      is_mcc = 0;
    bcp_AddBCLCubeByCube(p, result, bcp_GetGlobalCube(p, 3));  // "result" contains the universal cube
    bcp_SubtractBCL(p, result, l, is_mcc);             // "result" contains the negation of "l"
    
    // do a small minimization step
    bcp_DoBCLExpandWithOffSet(p, result, l);
    bcp_DoBCLMultiCubeContainment(p, result);

    return result;
}



bcl bcp_NewBCLComplementWithCofactorSub(bcp p, bcl l)
{
  int var_pos;
  int i, j;
  bcl f1;
  bcl f2;
  bcl cf1;
  bcl cf2;
 
  /*
  if ( l->cnt <= 14 )
  {
    return bcp_NewBCLComplementWithSubtract(p, l);
  }
  */
    
  
  bcp_CalcBCLBinateSplitVariableTable(p, l);
  var_pos = bcp_GetBCLMaxBinateSplitVariable(p, l);
  if ( var_pos < 0 )
  {
    bcl result = bcp_NewBCL(p);
    bcp_AddBCLCubeByCube(p, result, bcp_GetGlobalCube(p, 3));  // "result" contains the universal cube
    bcp_SubtractBCL(p, result, l, 0);             // "result" contains the negation of "l"
    return result;
    /* return bcp_NewBCLComplementWithSubtract(p, l); */
  }
  
  f1 = bcp_NewBCLCofacterByVariable(p, l, var_pos, 1);
  assert(f1 != NULL);
  bcp_DoBCLSimpleExpand(p, f1);
  //bcp_DoBCLSingleCubeContainment(p, f1);
  
  f2 = bcp_NewBCLCofacterByVariable(p, l, var_pos, 2);
  assert(f2 != NULL);
  bcp_DoBCLSimpleExpand(p, f2);
  //bcp_DoBCLSingleCubeContainment(p, f2);
  
  cf1 = bcp_NewBCLComplementWithCofactorSub(p, f1);
  assert(cf1 != NULL);
  cf2 = bcp_NewBCLComplementWithCofactorSub(p, f2);
  assert(cf2 != NULL);
  
  for( i = 0; i < cf1->cnt; i++ )
    if ( cf1->flags[i] == 0 )
      bcp_SetCubeVar(p, bcp_GetBCLCube(p, cf1, i), var_pos, 2);  
  //bcp_DoBCLSimpleExpand(p, cf1);
  bcp_DoBCLSingleCubeContainment(p, cf1);

  for( i = 0; i < cf2->cnt; i++ )
    if ( cf2->flags[i] == 0 )
      bcp_SetCubeVar(p, bcp_GetBCLCube(p, cf2, i), var_pos, 1);  
  //bcp_DoBCLSimpleExpand(p, cf2);
  bcp_DoBCLSingleCubeContainment(p, cf2);

  /*
    at this point the merging process should be much smarter
    at least we need to check, whether cubes can be recombined with each other
  */

  /* do some simple merge, if the cube just differs in the selected variable */
  for( i = 0; i < cf2->cnt; i++ )
  {
    if ( cf2->flags[i] == 0 )
    {
      bc c = bcp_GetBCLCube(p, cf2, i);
      bcp_SetCubeVar(p, c, var_pos, 2);  
      for( j = 0; j < cf1->cnt; j++ )
      {
        if ( i != j )
        {
          if ( bcp_CompareCube(p, c, bcp_GetBCLCube(p, cf1, j)) == 0 )
          {
            // the two cubes only differ in the selected variable, so extend the cube in cf1 and remove the cube from cf2
            bcp_SetCubeVar(p, bcp_GetBCLCube(p, cf1, j), var_pos, 3);
            cf2->flags[i] = 1;  // remove the cube from cf2, so that it will not be added later by bcp_AddBCLCubesByBCL()
          }
        } // i != j
      } // for cf1
      bcp_SetCubeVar(p, c, var_pos, 1);  // undo the change in the cube from cf2
    }
  }

  bcp_DeleteBCL(p, f1);
  bcp_DeleteBCL(p, f2);
  
  if ( bcp_AddBCLCubesByBCL(p, cf1, cf2) == 0 )
    return  bcp_DeleteBCL(p, cf1), bcp_DeleteBCL(p, cf2), NULL;

  bcp_DeleteBCL(p, cf2);

  //bcp_DoBCLSimpleExpand(p, cf1);
  bcp_DoBCLExpandWithOffSet(p, cf1, l);
  bcp_DoBCLSingleCubeContainment(p, cf1);
  
  return cf1;
}

/*
  better use "bcp_NewBCLComplementWithSubtract()"
*/
bcl bcp_NewBCLComplementWithCofactor(bcp p, bcl l)
{
  bcl n = bcp_NewBCLComplementWithCofactorSub(p, l);
  bcp_DoBCLMultiCubeContainment(p, n);
  return n;
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

