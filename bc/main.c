
#include "bc.h"
#include <assert.h>
#include <stdio.h>

/*============================================================*/


int mainx(void)
{
  bcp p = bcp_New(65);
  bc c;
  int i;
  
  printf("blk_cnt = %d\n", p->blk_cnt);
  bcp_StartCubeStackFrame(p);
  c = bcp_GetTempCube(p);
  printf("%s\n", bcp_GetStringFromCube(p, c));
  //bcp_SetCubeByString(p, c, "10-11--1----1111-0-0-0-0-0-0-0-0-0-0-0-0-0-0-0-0-0-0-0-1-0-0-0-01");
  bcp_SetCubeByString(p, c, "1111111111111111-0-0-0-0-0-0-0-0-0-0-0-0-0-0-0-0-0-0-0-1-0-0-0-01");

  printf("%s\n", bcp_GetStringFromCube(p, c));

  for( i = 0; i < p->var_cnt; i++ )
  {
    bcp_SetCubeVar(p, c, i, 3);
    printf("%s\n", bcp_GetStringFromCube(p, c));
  }
  
  bcp_EndCubeStackFrame(p);
  bcp_Delete(p);  
  
  
  return 0;
}

char *cubes_string= 
"1-1-11\n"
"110011\n"
"1-0-10\n"
"1001-0\n"
;



int mainy(void)
{
  bcp p = bcp_New(bcp_GetVarCntFromString(cubes_string));
  bcl l = bcp_NewBCL(p);
  bcl m = bcp_NewBCL(p);
  bcp_AddBCLCubesByString(p, l, cubes_string);
  bcp_AddBCLCubeByCube(p, m, bcp_GetGlobalCube(p, 3));
  bcp_SubtractBCL(p, m, l, 1);
  
  puts("original:");
  bcp_ShowBCL(p, l);
  puts("complement:");
  bcp_ShowBCL(p, m);

  bcp_IntersectionBCL(p, m, l);
  printf("intersction cube count %d\n", m->cnt);

  bcp_ClearBCL(p, m);
  bcp_AddBCLCubeByCube(p, m, bcp_GetGlobalCube(p, 3));
  bcp_SubtractBCL(p, m, l, 1);
  
  bcp_AddBCLCubesByBCL(p, m, l);        // add l to m
  printf("tautology=%d\n", bcp_IsBCLTautology(p, m));   // do a tautology test on the union

  
  bcp_DeleteBCL(p,  l);
  bcp_DeleteBCL(p,  m);
  bcp_Delete(p); 
  
  internalTest(21);
  return 0;
}



int main1(void)
{
  char *s = 
"-11\n"
"110\n"
"11-\n"
"0--\n"
  ;
  
  bcp p = bcp_New(bcp_GetVarCntFromString(s));
  bcl l = bcp_NewBCL(p);
  //bcl n;

  bcp_AddBCLCubesByString(p, l, s);
  puts("original:");
  bcp_ShowBCL(p, l);

  //n = bcp_NewBCLCofactorByCube(p, l, bcp_GetBCLCube(p, l, 2), 2);
  bcp_DoBCLMultiCubeContainment(p, l);

  puts("MCC:");
  bcp_ShowBCL(p, l);
  /*
  puts("cofactor:");
  bcp_ShowBCL(p, n);
  printf("tautology=%d\n", bcp_IsBCLTautology(p, n));   // do a tautology test 
  */
  
  bcp_DeleteBCL(p,  l);
  //bcp_DeleteBCL(p,  n);

  bcp_Delete(p);  
  return 0;
}

int main2(void)
{
  
  
  internalTest(19);
  //speedTest(21);
 
  minimizeTest(21);
  return 0;
}


int main3(int argc, char **argv)
{
  FILE *fp;
  if ( argc <= 1 )
  {
    printf("%s jsonfile\n", argv[0]);
    return 0;
  }
  fp = fopen(argv[1], "r");
  if ( fp == NULL )
    return perror(argv[1]), 0;
  bc_ExecuteJSON(fp);
  fclose(fp);
  return 0;
}


int main()
{
  bcp p = bcp_New(1);
  bcx x = bcp_Parse(p, "a&b|c&b");
  bcp_AddVarsFromBCX(p, x);
  bcp_ShowBCX(p, x);
  bcp_BuildVarList(p);
  
  
  //bcp_PrintBCX(p, x);
  
  coPrint(p->var_map); puts("");
  coPrint(p->var_list); puts("");
  
  bcp_DeleteBCX(p, x);
  bcp_Delete(p);
  puts("");
  return 0;
}
