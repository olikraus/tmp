
#include "bc.h"
#include <assert.h>
#include <stdio.h>



bcl bcp_NewBCLWithRandomTautology(bcp p, int size, int dc2one_conversion_cnt)
{
  bcl l = bcp_NewBCL(p);
  int cube_pos = bcp_AddBCLCubeByCube(p, l, bcp_GetGlobalCube(p, 3));
  int var_pos = 0; 
  unsigned value;
  int i;

  for(;;)
  {
    cube_pos = rand() % l->cnt;
    var_pos = rand() % p->var_cnt;  
    value = bcp_GetCubeVar(p, bcp_GetBCLCube(p, l, cube_pos), var_pos);
    if ( value == 3 )
    {
      bcp_SetCubeVar(p, bcp_GetBCLCube(p, l, cube_pos), var_pos, 1);
      cube_pos = bcp_AddBCLCubeByCube(p, l, bcp_GetBCLCube(p, l, cube_pos));
      bcp_SetCubeVar(p, bcp_GetBCLCube(p, l, cube_pos), var_pos, 2);
    }
    if ( l->cnt >= size )
      break;
  }

  for( i = 0; i < dc2one_conversion_cnt; i++ )
  {
    for(;;)
    {
      cube_pos = rand() % l->cnt;
      var_pos = rand() % p->var_cnt;  
      value = bcp_GetCubeVar(p, bcp_GetBCLCube(p, l, cube_pos), var_pos);
      if ( value == 3 )
      {
        bcp_SetCubeVar(p, bcp_GetBCLCube(p, l, cube_pos), var_pos, 2);
        break;
      }
    }
  }
  
  return l;
}



/*============================================================*/



void internalTest(int var_cnt)
{
  bcp p = bcp_New(var_cnt);
  bcl t = bcp_NewBCLWithRandomTautology(p, var_cnt, 0);
  bcl r = bcp_NewBCLWithRandomTautology(p, var_cnt, var_cnt);
  bcl l = bcp_NewBCL(p);
  bcl m = bcp_NewBCL(p);
  bcl n;
  
  int tautology;
  
  printf("tautology test 1\n");
  tautology = bcp_IsBCLTautology(p, t);
  assert(tautology != 0);
  
  printf("copy test\n");
  bcp_CopyBCL(p, l, t);
  assert( l->cnt == t->cnt );
  
  printf("tautology test 2\n");
  tautology = bcp_IsBCLTautology(p, l);
  assert(tautology != 0);

  printf("subtract test 1\n");
  bcp_SubtractBCL(p, l, t, 1);
  assert( l->cnt == 0 );

  printf("tautology test 3\n");
  tautology = bcp_IsBCLTautology(p, r);
  assert(tautology == 0);

  printf("subtract test 2\n");
  bcp_ClearBCL(p, l);
  bcp_AddBCLCubeByCube(p, l, bcp_GetGlobalCube(p, 3));  // "l" contains the universal cube
  bcp_SubtractBCL(p, l, r, 1);             // "l" contains the negation of "r"
  printf("subtract result size %d\n", l->cnt);
  assert( l->cnt != 0 );

  printf("intersection test\n");
  bcp_IntersectionBCLs(p, m, l, r);
  printf("intersection result  m->cnt=%d l->cnt=%d r->cnt=%d\n", m->cnt, l->cnt, r->cnt);
  assert( m->cnt == 0 );


  printf("tautology test 4\n");
  bcp_AddBCLCubesByBCL(p, l, r);
  //bcp_DoBCLSingleCubeContainment(p, l);
  printf("merge result size %d\n", l->cnt);
  tautology = bcp_IsBCLTautology(p, l);
  assert(tautology != 0);               // error with var_cnt == 20 

  bcp_CopyBCL(p, l, t);
  assert( l->cnt == t->cnt );
  printf("subtract test 3\n");          // repeat the subtract test with "r", use the tautology list "t" instead of the universal cube
  bcp_SubtractBCL(p, l, r, 1);             // "l" contains the negation of "r"
  assert( l->cnt != 0 );
  printf("intersection test 2\n");
  bcp_IntersectionBCLs(p, m, l, r);
  assert( m->cnt == 0 );

  printf("tautology test 5\n");
  bcp_AddBCLCubesByBCL(p, l, r);
  printf("merge result size %d\n", l->cnt);
  //bcp_ShowBCL(p, bcp_NewBCLComplementWithCofactor(p, l));
  tautology = bcp_IsBCLTautology(p, l);
  assert(tautology != 0);               



  printf("cofactor complement test\n");
  bcp_ClearBCL(p, l);
  
  n = bcp_NewBCLComplementWithCofactor(p, r);
  printf("complement result size %d\n", n->cnt);
  assert( n->cnt != 0 );
  
  printf("simple expand\n");
  bcp_DoBCLSimpleExpand(p, n);
  //bcp_DoBCLSingleCubeContainment(p, n);
  printf("simple expand new size %d\n", n->cnt);

  printf("intersection test 3\n");
  bcp_IntersectionBCLs(p, m, n, r);
  printf("intersection result  m->cnt=%d n->cnt=%d r->cnt=%d\n", m->cnt, n->cnt, r->cnt);
  assert( m->cnt == 0 );

  printf("tautology test 6\n");
  bcp_AddBCLCubesByBCL(p, n, r);
  //bcp_DoBCLSingleCubeContainment(p, l);
  printf("merge result size %d\n", n->cnt);
  //bcp_ShowBCL(p, bcp_NewBCLComplementWithSubtract(p, r));
  tautology = bcp_IsBCLTautology(p, n);
  assert(tautology != 0);


  bcp_DeleteBCL(p,  n);
  bcp_DeleteBCL(p,  t);
  bcp_DeleteBCL(p,  r);
  bcp_DeleteBCL(p,  l);
  bcp_Delete(p); 

}

