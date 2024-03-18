/*

  bc.h
  
  boolean cube

*/

#ifndef BC_H
#define BC_H

#include <x86intrin.h>
#include <stdint.h>



typedef struct bcp_struct *bcp;
typedef __m128i *bc;
typedef struct bcl_struct *bcl;


/* boolean cube problem, each function will require a pointer to this struct */
#define BCP_MAX_STACK_FRAME_DEPTH 500
struct bcp_struct
{
  int var_cnt;  // number of variables per cube
  int blk_cnt;  // number of blocks per cube, one block is one __m128i = 64 variables
  int vars_per_blk_cnt; // number of variables per block --> 64
  int bytes_per_cube_cnt; // number of bytes per cube, this is blk_cnt*sizeof(__m128i)
  char *cube_to_str;    // storage area for one visual representation of a cube
  bcl stack_cube_list;    // storage area for temp cubes
  int stack_frame_pos[BCP_MAX_STACK_FRAME_DEPTH];
  int stack_depth;
  bcl global_cube_list;    // storage area for temp cubes
};

/* one cube, the number of __m128i is (var_cnt / 64) */

struct bcl_struct
{
  int cnt;
  int max;
  int last_deleted;
  __m128i *list;        // max * var_cnt / 64 entries
  uint8_t *flags;       // bit 0 is the cube deleted flag
};

/*============================================================*/

/* bcp.c */
int bcp_GetVarCntFromString(const char *s);

bcp bcp_New(size_t var_cnt);
void bcp_Delete(bcp p);
//bc bcp_GetGlobalCube(bcp p, int pos);
#define bcp_GetGlobalCube(p, pos) \
  bcp_GetBCLCube((p), (p)->global_cube_list, (pos))  
void bcp_CopyGlobalCube(bcp p, bc r, int pos);
void bcp_StartCubeStackFrame(bcp p);
void bcp_EndCubeStackFrame(bcp p);
bc bcp_GetTempCube(bcp p);

/* bcube.c */
/* core functions */

void bcp_ClrCube(bcp p, bc c);
void bcp_CopyCube(bcp p, bc dest, bc src);
int bcp_CompareCube(bcp p, bc a, bc b);


void bcp_SetCubeVar(bcp p, bc c, unsigned var_pos, unsigned value);
//unsigned bcp_GetCubeVar(bcp p, bc c, unsigned var_pos);
#define bcp_GetCubeVar(p, c, var_pos) \
  ((((uint16_t *)(c))[(var_pos)/8] >> (((var_pos)&7)*2)) & 3)

const char *bcp_GetStringFromCube(bcp p, bc c);
void bcp_SetCubeByStringPointer(bcp p, bc c,  const char **s);
void bcp_SetCubeByString(bcp p, bc c, const char *s);

/* boolean functions */

int bcp_IsTautologyCube(bcp p, bc c);
int bcp_IntersectionCube(bcp p, bc r, bc a, bc b); // returns 0, if there is no intersection
int bcp_IsIntersectionCube(bcp p, bc a, bc b); // returns 0, if there is no intersection
int bcp_IsIllegal(bcp p, bc c);                                 // check whether "c" contains "00" codes
int bcp_GetCubeVariableCount(bcp p, bc cube);   // return the number of 01 or 10 codes in "cube"
int bcp_GetCubeDelta(bcp p, bc a, bc b);                // calculate the delta between a and b
int bcp_IsSubsetCube(bcp p, bc a, bc b);                // is "b" is a subset of "a"

/* bclcore.c */

#define bcp_GetBCLCnt(p, l) ((l)->cnt)

bcl bcp_NewBCL(bcp p);          // create new empty bcl
bcl bcp_NewBCLByBCL(bcp p, bcl l);      // create a new bcl as a copy of an existing bcl
int bcp_CopyBCL(bcp p, bcl a, bcl b);
void bcp_ClearBCL(bcp p, bcl l);
void bcp_DeleteBCL(bcp p, bcl l);
int bcp_ExtendBCL(bcp p, bcl l);
//bc bcp_GetBCLCube(bcp p, bcl l, int pos);
#define bcp_GetBCLCube(p, l, pos) \
  (bc)(((uint8_t *)((l)->list)) + (pos) * (p)->bytes_per_cube_cnt)
void bcp_ShowBCL(bcp p, bcl l);
void bcp_PurgeBCL(bcp p, bcl l);               /* purge deleted cubes */
int bcp_AddBCLCube(bcp p, bcl l); // add empty cube to list l, returns the position of the new cube or -1 in case of error
int bcp_AddBCLCubeByCube(bcp p, bcl l, bc c); // append cube c to list l, returns the position of the new cube or -1 in case of error
int bcp_AddBCLCubesByBCL(bcp p, bcl a, bcl b); // append cubes from b to a, does not do any simplification, returns 0 on error
int bcp_AddBCLCubesByString(bcp p, bcl l, const char *s); // add cube(s) described as a string, returns 0 in case of error
int *bcp_GetBCLVarCntList(bcp p, bcl l);

/* bccofactor.c */

void bcp_CalcBCLBinateSplitVariableTable(bcp p, bcl l);
int bcp_GetBCLMaxBinateSplitVariableSimple(bcp p, bcl l);
int bcp_GetBCLMaxBinateSplitVariable(bcp p, bcl l);
void bcp_DoBCLOneVariableCofactor(bcp p, bcl l, unsigned var_pos, unsigned value);
bcl bcp_NewBCLCofacterByVariable(bcp p, bcl l, unsigned var_pos, unsigned value);       // create a new list, which is the cofactor from "l"
void bcp_DoBCLCofactorByCube(bcp p, bcl l, bc c, int exclude);         
bcl bcp_NewBCLCofactorByCube(bcp p, bcl l, bc c, int exclude);          // don't use this fn, use bcp_IsBCLCubeRedundant() or bcp_IsBCLCubeCovered() instead
int bcp_IsBCLUnate(bcp p);  // requires call to bcp_CalcBCLBinateSplitVariableTable


/* bclcontainment.c */

void bcp_DoBCLSingleCubeContainment(bcp p, bcl l);
int bcp_IsBCLCubeCovered(bcp p, bcl l, bc c);           // is cube c a subset of l (is cube c covered by l)
int bcp_IsBCLCubeRedundant(bcp p, bcl l, int pos);      // is the cube at pos in l covered by all other cubes in l
void bcp_DoBCLMultiCubeContainment(bcp p, bcl l);


/* bcltautology.c */

int bcp_IsBCLTautology(bcp p, bcl l);


/* bclsubstract.c */

// oid bcp_DoBCLSharpOperation(bcp p, bcl l, bc a, bc b);
void bcp_SubtractBCL(bcp p, bcl a, bcl b, int is_mcc);

/* bclcomplement.c */

bcl bcp_NewBCLComplementWithSubtract(bcp p, bcl l);  // faster than with cofactor
bcl bcp_NewBCLComplementWithCofactor(bcp p, bcl l); // slow!
bcl bcp_NewBCLComplement(bcp p, bcl l);         // calls bcp_NewBCLComplementWithSubtract();


/* bclsubset.c */

int bcp_IsBCLSubsetWithCofactor(bcp p, bcl a, bcl b);   //   test, whether "b" is a subset of "a"
int bcp_IsBCLSubsetWithSubstract(bcp p, bcl a, bcl b);  // this fn seems to be much slower than bcp_IsBCLSubsetWithCofactor
int bcp_IsBCLSubset(bcp p, bcl a, bcl b);       //   test, whether "b" is a subset of "a", calls bcp_IsBCLSubsetWithCofactor()




void bcp_DoBCLSimpleExpand(bcp p, bcl l);
void bcp_DoBCLExpandWithOffSet(bcp p, bcl l, bcl off);  // this operation does not do any SCC or MCC
void bcp_DoBCLSubsetCubeMark(bcp p, bcl l, int pos);

int bcp_IntersectionBCLs(bcp p, bcl result, bcl a, bcl b); // result = a intersection with b
int bcp_IntersectionBCL(bcp p, bcl a, bcl b);   // a = a intersection with b 



void bcp_MinimizeBCL(bcp p, bcl l);

/*
static __m128i m128i_get_n_bit_mask(uint16_t val, unsigned bit_pos)
{
  uint16_t a[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  val <<= bit_pos & 0xf;
  a[7-(bit_pos>>4)] = val;
  return _mm_loadu_si128((__m128i *)a);
}
*/

/*
#define m128i_get_n_bit_mask(val, bit_pos) \
  _mm_slli_si128(_mm_set_epi32(0, 0, 0, (val)<<(bit_pos&0x1f)), (bit_pos)>>5)
*/

/* limitation: the bitpattern in val must not cross a 32 bit boundery, for example val=3 and  bit_pos=31 is not allowed */
/*
#define m128i_get_n_bit_mask(val, bit_pos) \
  _mm_set_epi32(  \
    0==((bit_pos)>>5)?(((uint32_t)(val))<<(bit_pos&0x1f)):0, \
    1==((bit_pos)>>5)?(((uint32_t)(val))<<(bit_pos&0x1f)):0, \
    2==((bit_pos)>>5)?(((uint32_t)(val))<<(bit_pos&0x1f)):0, \
    3==((bit_pos)>>5)?(((uint32_t)(val))<<(bit_pos&0x1f)):0 ) 
*/

/*
#define m128i_get_1_bit_mask(bit_pos) \
  m128i_get_n_bit_mask(1, bit_pos)
*/

/*
#define m128i_get_bit_mask(bit_pos) \
  _mm_slli_si128(_mm_set_epi32 (0, 0, 0, 1<<(bit_pos&0x1f)), (bit_pos)>>5)
*/

/*
#define m128i_not(m) \
  _mm_xor_si128(m, _mm_set1_epi16(0x0ffff))
*/

/*
#define m128i_is_zero(m) \
  ((_mm_movemask_epi8(_mm_cmpeq_epi16((m),_mm_setzero_si128())) == 0xFFFF)?1:0)
*/

#define m128i_is_equal(m1, m2) \
  ((_mm_movemask_epi8(_mm_cmpeq_epi16((m1),(m2))) == 0xFFFF)?1:0)

/* val=0..3, bit_pos=0..126 (must not be odd) */
/*
#define m128i_set_2_bit(m, val, bit_pos) \
 _mm_or_si128( m128i_get_n_bit_mask((val), (bit_pos)), _mm_andnot_si128(m128i_get_n_bit_mask(3, (bit_pos)), (m)) )
*/

/* bcselftest.c */

bcl bcp_NewBCLWithRandomTautology(bcp p, int size, int dc2one_conversion_cnt);
void internalTest(int var_cnt);



#endif

