/*

  bcltautology.c
  
  boolean cube list: check whether a list has the tautology property
  
  tautology property: for any value, the result of the boolean function,
    which is described by the bcl is always true
    
*/

#include "bc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>


//#define BC_TAUT_DEBUG 


#ifdef BC_TAUT_DEBUG 

uint32_t rc_crc32(uint32_t crc, const char *buf, size_t len) // rosetta code
{
	static uint32_t table[256];
	static int have_table = 0;
	uint32_t rem;
	uint8_t octet;
	int i, j;
	const char *p, *q;

	/* This check is not thread safe; there is no mutex. */
	if (have_table == 0) {
		/* Calculate CRC table. */
		for (i = 0; i < 256; i++) {
			rem = i;  /* remainder from polynomial division */
			for (j = 0; j < 8; j++) {
				if (rem & 1) {
					rem >>= 1;
					rem ^= 0xedb88320;
				} else
					rem >>= 1;
			}
			table[i] = rem;
		}
		have_table = 1;
	}

	crc = ~crc;
	q = buf + len;
	for (p = buf; p < q; p++) {
		octet = *p;  /* Cast to unsigned octet. */
		crc = (crc >> 8) ^ table[(crc & 0xff) ^ octet];
	}
	return ~crc;
}


int bc_var_stack[1000];
#endif // BC_TAUT_DEBUG


/*
  try to find independent partitions in BCL, for example
  0---
  11--
  --01
  would contain two idependent problems: 11--/0--- and on the other side --01.
  the partition will be written into the flags array, which means, that the flags array must be all 0 entries
*/
int bcp_is_bcl_partition(bcp p, bcl l)
{
  int cnt = l->cnt;
  int i;
  int other_partition_cnt = 0;
  bc mask; // the current parition cube
  bc mask2; // the other parition cube
  unsigned bitcnt; // == variable count
  unsigned nbitcnt; // == variable count
  int is_changed;
  if ( cnt <= 1 )
    return 0;           // no partition if   there is only one entry
  assert(l->flags[0] == 0);
  bcp_StartCubeStackFrame(p);  
  mask = bcp_GetTempCube(p);
  mask2 = bcp_GetTempCube(p);
  bcp_GetVariableMask(p, mask, bcp_GetBCLCube(p, l, 0));
  bitcnt = bcp_OrBitCnt(p, mask, mask, mask);
  
  // in the first step, try to see, how much variables are reached
  do
  {
    // printf("mask = %s\n", bcp_GetStringFromCube(p, mask));
    // printf("bitcnt = %u\n", bitcnt);
    is_changed = 0;
    for( i = 1; i < cnt; i++ )
    {
      bcp_GetVariableMask(p, mask2, bcp_GetBCLCube(p, l, i));
      if ( bcp_IsAndZero(p, mask, mask2) == 0 )  // only if there is a connection between the variables
      {
        nbitcnt = bcp_OrBitCnt(p, mask, mask, mask2);
        if ( bitcnt < nbitcnt  )
        {
          bitcnt = nbitcnt;
          is_changed = 1;
        }
      }
    }
  } while( is_changed != 0 );
  
  // now mask contains a bit for each variable which is connected with each other. Try find two partitions with this
  
  for( i = 1; i < cnt; i++ )
  {
    assert(l->flags[i] == 0);           // we need to assume here, that the flag is unussed so far, because it will be used to mark the partion
    bcp_GetVariableMask(p, mask2, bcp_GetBCLCube(p, l, i));
    if ( bcp_IsAndZero(p, mask, mask2) )
    {
      other_partition_cnt++;            // partition found
      l->flags[0] = 1;
    }
  }
  
  if ( other_partition_cnt == 0 )
    return bcp_EndCubeStackFrame(p), 0;         // no partition found
  
  // at this point the two partions marked in the flag
  // for one partion all flags are zero, for the other parition the flags are 1

  bcp_EndCubeStackFrame(p);  
  printf("Partition %d/%d\n", other_partition_cnt, l->cnt);
  return 1;
}

bcl bcp_NewBCLByFlag(bcp p, bcl l, uint8_t flag)
{
  int cnt = l->cnt;
  int i;
  bcl ll = bcp_NewBCL(p);          // create new empty bcl
  for( i = 0; i < cnt; i++ )
  {
    if ( l->flags[i] == flag )
      if ( bcp_AddBCLCubeByCube(p, ll, bcp_GetBCLCube(p, l, i)) == 0 )
      {
        return bcp_DeleteBCL(p, ll), NULL;
      }
  }
  return ll;
}


// the unate check is faster than the split var calculation, but if the BCL is binate, 
// then both calculations have to be done
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
  
  assert(depth < 1000);
  
  if ( l->cnt == 0 )
    return 0;
  
  //assert( bcp_IsPurgeUsefull(p, l) == 0 );

  //bcp_PurgeBCL(p, l);
  //bcp_is_bcl_partition(p, l);
  
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
      if ( l->flags[0] == 0 )
        if ( bcp_IsTautologyCube(p, bcp_GetBCLCube(p, l, i)) )
          return 1;
    }
    return 0;
  }
#endif

  var_pos = bcp_GetBCLMaxBinateSplitVariable(p, l);     // bcp_GetBCLMaxBinateSplitVariableSimple has similar performance
#ifdef BC_TAUT_DEBUG 
  bc_var_stack[depth] = var_pos;
#endif // BC_TAUT_DEBUG

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

  assert( bcp_IsBCLVariableUnate(p, l, var_pos, 1) == 0 );
  assert( bcp_IsBCLVariableUnate(p, l, var_pos, 2) == 0 );
  
#ifdef BC_TAUT_DEBUG 
  for( int i = 0; i <= depth; i++ )
    printf("%02d ", bc_var_stack[i]);
  printf("depth %d, size %d, crc %08lx\n", depth, l->cnt,   (unsigned long)rc_crc32(0, (void *)l->list, p->bytes_per_cube_cnt*l->cnt));
  

  bcp_ShowBCL(p, l);
#endif // BC_TAUT_DEBUG
  
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
  //assert( bcp_IsPurgeUsefull(p, f1) == 0 );
  //assert( bcp_IsPurgeUsefull(p, f2) == 0 );

#ifdef BC_TAUT_DEBUG 
  bcp_IsBCLVariableUnate(p, f1, var_pos, 1);
  bcp_IsBCLVariableUnate(p, f2, var_pos, 2);
#endif // BC_TAUT_DEBUG

  
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
