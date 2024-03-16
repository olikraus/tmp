
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

#include <time.h>

int main(void)
{
  
  int cnt = 43;
  int is_subset = 0;
  clock_t t0, t1;
  bcp p = bcp_New(cnt);
  bcl a = bcp_NewBCLWithRandomTautology(p, cnt+2, cnt);
  bcl b = bcp_NewBCLWithRandomTautology(p, cnt+2, cnt);
  bcl ic = bcp_NewBCL(p);
  
  
  bcp_IntersectionBCLs(p, ic, a, b);
  assert( ic->list != 0 );
  printf("raw  ic->cnt = %d\n", ic->cnt);
  bcp_MinimizeBCL(p, ic);
  printf("mini ic->cnt = %d\n", ic->cnt);
  
  //puts("original:");
  //bcp_ShowBCL(p, l);

  t0 = clock();
  //is_subset = bcp_IsBCLSubsetWithSubstract(p, a, ic);
  t1 = clock();  
  //printf("bcp_IsBCLSubsetWithSubstract(p, a, ic): is_subset=%d clock=%ld\n", is_subset, t1-t0);  

  t0 = clock();
  is_subset = bcp_IsBCLSubsetWithCofactor(p, a, ic);
  t1 = clock();  
  printf("bcp_IsBCLSubsetWithCofactor(p, a, ic): is_subset=%d clock=%ld\n", is_subset, t1-t0);  

  t0 = clock();
  //is_subset = bcp_IsBCLSubsetWithSubstract(p, ic, a);
  t1 = clock();  
  //printf("bcp_IsBCLSubsetWithSubstract(p, ic, a): is_subset=%d clock=%ld\n", is_subset, t1-t0);  

  t0 = clock();
  is_subset = bcp_IsBCLSubsetWithCofactor(p, ic, a);
  t1 = clock();  
  printf("bcp_IsBCLSubsetWithCofactor(p, ic, a): is_subset=%d clock=%ld\n", is_subset, t1-t0);  


  t0 = clock();
  //is_subset = bcp_IsBCLSubsetWithSubstract(p, b, ic);
  t1 = clock();  
  //printf("bcp_IsBCLSubsetWithSubstract(p, b, ic): is_subset=%d clock=%ld\n", is_subset, t1-t0);  

  t0 = clock();
  is_subset = bcp_IsBCLSubsetWithCofactor(p, b, ic);
  t1 = clock();  
  printf("bcp_IsBCLSubsetWithCofactor(p, b, ic): is_subset=%d clock=%ld\n", is_subset, t1-t0);  

  t0 = clock();
  //is_subset = bcp_IsBCLSubsetWithSubstract(p, ic, b);
  t1 = clock();  
  //printf("bcp_IsBCLSubsetWithSubstract(p, ic, b): is_subset=%d clock=%ld\n", is_subset, t1-t0);  

  t0 = clock();
  is_subset = bcp_IsBCLSubsetWithCofactor(p, ic, b);
  t1 = clock();  
  printf("bcp_IsBCLSubsetWithCofactor(p, ic, b): is_subset=%d clock=%ld\n", is_subset, t1-t0);  


  t0 = clock();
  //is_subset = bcp_IsBCLSubsetWithSubstract(p, a, b);
  t1 = clock();  
  //printf("bcp_IsBCLSubsetWithSubstract(p, a, b): is_subset=%d clock=%ld\n", is_subset, t1-t0);  

  t0 = clock();
  is_subset = bcp_IsBCLSubsetWithCofactor(p, a, b);
  t1 = clock();  
  printf("bcp_IsBCLSubsetWithCofactor(p, a, b): is_subset=%d clock=%ld\n", is_subset, t1-t0);  

  t0 = clock();
  //is_subset = bcp_IsBCLSubsetWithSubstract(p, b, a);
  t1 = clock();  
  //printf("bcp_IsBCLSubsetWithSubstract(p, b, a): is_subset=%d clock=%ld\n", is_subset, t1-t0);  

  t0 = clock();
  is_subset = bcp_IsBCLSubsetWithCofactor(p, b, a);
  t1 = clock();  
  printf("bcp_IsBCLSubsetWithCofactor(p, b, a): is_subset=%d clock=%ld\n", is_subset, t1-t0);  
  
  bcp_DeleteBCL(p,  a);
  bcp_DeleteBCL(p,  b);
  bcp_DeleteBCL(p,  ic);

  bcp_Delete(p);  
  
  internalTest(19);
  
  return 0;
}

