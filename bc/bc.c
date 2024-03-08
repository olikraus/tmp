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


*/

#include <x86intrin.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>


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

void bcp_ClrCube(bcp p, bc c);
void bcp_SetCubeVar(bcp p, bc c, unsigned var_pos, unsigned value);
//unsigned bcp_GetCubeVar(bcp p, bc c, unsigned var_pos);
#define bcp_GetCubeVar(p, c, var_pos) \
  ((((uint16_t *)(c))[(var_pos)/8] >> (((var_pos)&7)*2)) & 3)

const char *bcp_GetStringFromCube(bcp p, bc c);
void bcp_SetCubeByString(bcp p, bc c, const char *s);
void bcp_CopyCube(bcp p, bc dest, bc src);
int bcp_CompareCube(bcp p, bc a, bc b);
int bcp_IsTautologyCube(bcp p, bc c);
int bcp_IntersectionCube(bcp p, bc r, bc a, bc b); // returns 0, if there is no intersection
int bcp_IsIntersectionCube(bcp p, bc a, bc b); // returns 0, if there is no intersection
int bcp_IsIllegal(bcp p, bc c);
int bcp_GetCubeVariableCount(bcp p, bc cube);
int bcp_GetCubeDelta(bcp p, bc a, bc b);

bcl bcp_NewBCL(bcp p);          // create new empty bcl
bcl bcp_NewBCLByBCL(bcp p, bcl l);      // create a new bcl as a copy of an existing bcl
int bcp_CopyBCL(bcp p, bcl a, bcl b);
void bcp_DeleteBCL(bcp p, bcl l);
bc bcp_GetBCLCube(bcp p, bcl l, int pos);
/*
#define bcp_GetBCLCube(p, l, pos) \
  (bc)(((uint8_t *)((l)->list)) + (pos) * (p)->bytes_per_cube_cnt)
  */
void bcp_ShowBCL(bcp p, bcl l);
void bcp_PurgeBCL(bcp p, bcl l);               /* purge deleted cubes */
void bcp_DoBCLSingleCubeContainment(bcp p, bcl l);
void bcp_DoBCLSimpleExpand(bcp p, bcl l);
void bcp_DoBCLExpandWithOffSet(bcp p, bcl l, bcl off);
void bcp_DoBCLSubsetCubeMark(bcp p, bcl l, int pos);
void bcp_DoBCLSharpOperation(bcp p, bcl l, bc a, bc b);

void bcp_SubtractBCL(bcp p, bcl a, bcl b);
int bcp_IntersectionBCLs(bcp p, bcl result, bcl a, bcl b);
int bcp_IntersectionBCL(bcp p, bcl a, bcl b);

void bcp_DoBCLOneVariableCofactor(bcp p, bcl l, unsigned var_pos, unsigned value);  // calculate cofactor for a list, called by "bcp_NewBCLCofacter"
bcl bcp_NewBCLCofacter(bcp p, bcl l, unsigned var_pos, unsigned value);         // create a new list, which is the cofactor from "l"
int bcp_AddBCLCube(bcp p, bcl l);
int bcp_AddBCLCubeByCube(bcp p, bcl l, bc c);
int bcp_AddBCLCubesByBCL(bcp p, bcl a, bcl b);
int bcp_AddBCLCubesByString(bcp p, bcl l, const char *s);
#define bcp_GetBCLCnt(p, l) ((l)->cnt)
void bcp_CalcBCLBinateSplitVariableTable(bcp p, bcl l);
int bcp_GetBCLBalancedBinateSplitVariable(bcp p, bcl l);
int bcp_GetBCLMaxBinateSplitVariable(bcp p, bcl l);
int bcp_IsBCLUnate(bcp p);

int bcp_IsBCLTautology(bcp p, bcl l);
bcl bcp_NewBCLComplementWithSubtract(bcp p, bcl l);
bcl bcp_NewBCLComplementWithCofactor(bcp p, bcl l);


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

static void print128_num(__m128i var)
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


int bcp_GetVarCntFromString(const char *s)
{
  int cnt = 0;
  for(;;)
  {
    while( *s == ' ' || *s == '\t' )            // skip white space
      s++;
    if ( *s == '\0' || *s == '\r' || *s == '\n' )       // stop looking at further chars if the line/string ends
	{
      return cnt;
	}
    s++;
    cnt++;
  }
  return cnt; // we should never reach this statement
}


/*============================================================*/

bcp bcp_New(size_t var_cnt)
{
  bcp p = (bcp)malloc(sizeof(struct bcp_struct));
  if ( p != NULL )
  {
      p->var_cnt = var_cnt;
      p->vars_per_blk_cnt = sizeof(__m128i)*4;
      p->blk_cnt = (var_cnt + p->vars_per_blk_cnt-1)/p->vars_per_blk_cnt;
      p->bytes_per_cube_cnt = p->blk_cnt*sizeof(__m128i);
      //printf("p->bytes_per_cube_cnt=%d\n", p->bytes_per_cube_cnt);
      p->stack_depth = 0;
      p->cube_to_str = (char *)malloc(p->var_cnt+1);
      if ( p->cube_to_str != NULL )
      {
        p->stack_cube_list = bcp_NewBCL(p);
        if ( p->stack_cube_list != NULL )
        {
          p->global_cube_list = bcp_NewBCL(p);
          if ( p->global_cube_list != NULL )
          {
            int i;
			/*
				0..3:	constant cubes for all illegal, all zero, all one and all don't care
				4..7:	uint8_t counters for zeros in a list
				8..11:	uint8_t counters for ones in a list
                                4..11: uint16_t counters
                                12..19: uint16_t counters
			*/
            for( i = 0; i < 4+8+8; i++ )
              bcp_AddBCLCube(p, p->global_cube_list);
            if ( p->global_cube_list->cnt >= 4 )
            {
              memset(bcp_GetBCLCube(p, p->global_cube_list, 0), 0, p->bytes_per_cube_cnt);  // all vars are illegal
              memset(bcp_GetBCLCube(p, p->global_cube_list, 1), 0x55, p->bytes_per_cube_cnt);  // all vars are zero
              memset(bcp_GetBCLCube(p, p->global_cube_list, 2), 0xaa, p->bytes_per_cube_cnt);  // all vars are one
              memset(bcp_GetBCLCube(p, p->global_cube_list, 3), 0xff, p->bytes_per_cube_cnt);  // all vars are don't care
              return p;
            }
            bcp_DeleteBCL(p, p->global_cube_list);
          }
          bcp_DeleteBCL(p, p->stack_cube_list);
        }
        free(p->cube_to_str);
      }
      free(p);
  }
  return NULL;
}

void bcp_Delete(bcp p)
{
  if ( p == NULL )
    return ;
  bcp_DeleteBCL(p, p->global_cube_list);
  bcp_DeleteBCL(p, p->stack_cube_list);
  free(p->cube_to_str);
  free(p);
}

#ifndef bcp_GetGlobalCube
bc bcp_GetGlobalCube(bcp p, int pos)
{
  assert(pos < p->global_cube_list->cnt && pos >= 0);
  return bcp_GetBCLCube(p, p->global_cube_list, pos);  
}
#endif
/*
#define bcp_GetGlobalCube(p, pos) \
  bcp_GetBCLCube((p), (p)->global_cube_list, (pos))  
*/

/* copy global cube at pos to cube provided in "r" */
void bcp_CopyGlobalCube(bcp p, bc r, int pos)
{
  bcp_CopyCube(p, r, bcp_GetGlobalCube(p, pos));
}


/* start a new stack frame for bcp_GetTempCube() */
void bcp_StartCubeStackFrame(bcp p)
{
  if ( p->stack_depth >= BCP_MAX_STACK_FRAME_DEPTH )
  {
    assert(p->stack_depth < BCP_MAX_STACK_FRAME_DEPTH);  // output error message
    exit(1); // just ensure, that we do exit, also incases if NDEBUG is active
  }
  p->stack_frame_pos[p->stack_depth] = p->stack_cube_list->cnt;
  p->stack_depth++;    
}

/* delete all cubes returned by bcp_GetTempCube() since corresponding call to bcp_StartCubeStackFrame() */
void bcp_EndCubeStackFrame(bcp p)
{
  assert(p->stack_depth > 0);
  p->stack_depth--;
  p->stack_cube_list->cnt = p->stack_frame_pos[p->stack_depth] ;  // reduce the stack list to the previous size
}

/* Return a temporary cube, which will deleted with bcp_EndCubeStackFrame(). Requires a call to bcp_StartCubeStackFrame(). */
/* this is NOT MT-SAFE, each thread requires its own bcp structure */
bc bcp_GetTempCube(bcp p)
{
  int i;
  assert(p->stack_depth > 0);
  i = bcp_AddBCLCube(p, p->stack_cube_list);
  if ( i < p->stack_frame_pos[p->stack_depth-1] )
  {
    assert( i >= p->stack_frame_pos[p->stack_depth-1] );
    exit(1);
  }
  return bcp_GetBCLCube(p, p->stack_cube_list, i);
}



/*============================================================*/

void bcp_ClrCube(bcp p, bc c)
{
  // printf("bcp_ClrCube: %p %d\n", c, p->bytes_per_cube_cnt);
  memset(c, 0xff, p->bytes_per_cube_cnt);       // assign don't care
}

void bcp_SetCubeVar(bcp p, bc c, unsigned var_pos, unsigned value)
{  
  unsigned idx = var_pos/8;
  uint16_t *ptr = (uint16_t *)c;
  uint16_t mask = ~(3 << ((var_pos&7)*2));
  assert( var_pos < p->var_cnt );
  ptr[idx] &= mask;
  ptr[idx] |= value  << ((var_pos&7)*2);
}

/*
unsigned bcp_GetCubeVar(bcp p, bc c, unsigned var_pos)
{
  //uint16_t *ptr = (uint16_t *)c;
  return (((uint16_t *)c)[var_pos/8] >> ((var_pos&7)*2)) & 3;
}
#define bcp_GetCubeVar(p, c, var_pos) \
  ((((uint16_t *)(c))[(var_pos)/8] >> (((var_pos)&7)*2)) & 3)
*/

const char *bcp_GetStringFromCube(bcp p, bc c)
{
  int i, var_cnt = p->var_cnt;
  char *s = p->cube_to_str; 
  for( i = 0; i < var_cnt; i++ )
  {
    s[i] = "x01-"[bcp_GetCubeVar(p, c, i)];
  }
  s[var_cnt] = '\0';
  return s;
}

/*
  use string "s" to fill the content of cube "c"
    '0' --> bit value 01
    '1' --> bit value 10
    '-' --> bit value 11
    'x' (or any other char > 32 --> bit value "00" (which is the illegal value)
    ' ', '\t' --> ignored
    '\0', '\r', '\n' --> reading from "s" will stop
  
*/
void bcp_SetCubeByStringPointer(bcp p, bc c,  const char **s)
{
  int i, var_cnt = p->var_cnt;
  unsigned v;
  for( i = 0; i < var_cnt; i++ )
  {
    while( **s == ' ' || **s == '\t' )            // skip white space
      (*s)++;
    if ( **s == '0' ) { v = 1; }
    else if ( **s == '1' ) { v = 2; }
    else if ( **s == '-' ) { v = 3; }
    else if ( **s == 'x' ) { v = 0; }
    else { v = 3; }
    if ( **s != '\0' && **s != '\r' && **s != '\n' )       // stop looking at further chars if the line/string ends
      (*s)++;
    bcp_SetCubeVar(p, c, i, v);
  }
}

void bcp_SetCubeByString(bcp p, bc c, const char *s)
{
  bcp_SetCubeByStringPointer(p, c, &s);
}


void bcp_CopyCube(bcp p, bc dest, bc src)
{
  memcpy(dest, src, p->bytes_per_cube_cnt);
}

int bcp_CompareCube(bcp p, bc a, bc b)
{
  return memcmp((void *)a, (void *)b, p->bytes_per_cube_cnt);
}


int bcp_IsTautologyCube(bcp p, bc c)
{
  // assumption: Unused vars are set to 3 (don't care)
  int i, cnt = p->blk_cnt;
  __m128i t = _mm_loadu_si128(bcp_GetBCLCube(p, p->global_cube_list, 3));
  
  for( i = 0; i < cnt; i++ )
    if ( m128i_is_equal(_mm_loadu_si128(c+i), t) == 0 )
      return 0;
  return 1;
}


/*
  calculate intersection of a and b, result is stored in r
  return 0, if there is no intersection
*/
int bcp_IntersectionCube(bcp p, bc r, bc a, bc b)
{
  int i, cnt = p->blk_cnt;
  __m128i z = _mm_loadu_si128(bcp_GetBCLCube(p, p->global_cube_list, 1));
  __m128i rr;
  uint16_t f = 0x0ffff;
  for( i = 0; i < cnt; i++ )
  {    
    rr = _mm_and_si128(_mm_loadu_si128(a+i), _mm_loadu_si128(b+i));      // calculate the intersection
    _mm_storeu_si128(r+i, rr);          // and store the intersection in the destination cube
    /*
      each value has the bits illegal:00, zero:01, one:10, don't care:11
      goal is to find, if there are any illegal variables bit pattern 00.
      this is done with the following steps:
        1. OR operation: r|r>>1 --> the lower bit will be 0 only, if we have the illegal var
        2. AND operation with the zero mask --> so there will be only 00 (illegal) or 01 (not illegal)
        3. CMP operation with the zero mask, the data size (epi16) does not matter here
        4. movemask operation to see, whether the result is identical to the zero mask (if so, then the intersection exists)
    
        Short form: 00? --> x0 --> 00 == 00?
    */    
    f &= _mm_movemask_epi8(_mm_cmpeq_epi16(_mm_and_si128(_mm_or_si128( rr, _mm_srai_epi16(rr,1)), z), z));
  }
  if ( f == 0xffff )
    return 1;
  return 0;
}

int bcp_IsIntersectionCube(bcp p, bc a, bc b)
{
  int i, cnt = p->blk_cnt;
  __m128i z = _mm_loadu_si128(bcp_GetBCLCube(p, p->global_cube_list, 1));
  __m128i rr;
  uint16_t f = 0x0ffff;
  for( i = 0; i < cnt; i++ )
  {    
    rr = _mm_and_si128(_mm_loadu_si128(a+i), _mm_loadu_si128(b+i));      // calculate the intersection
    /*
      each value has the bits illegal:00, zero:01, one:10, don't care:11
      goal is to find, if there are any illegal variables bit pattern 00.
      this is done with the following steps:
        1. OR operation: r|r>>1 --> the lower bit will be 0 only, if we have the illegal var
        2. AND operation with the zero mask --> so there will be only 00 (illegal) or 01 (not illegal)
        3. CMP operation with the zero mask, the data size (epi16) does not matter here
        4. movemask operation to see, whether the result is identical to the zero mask (if so, then the intersection exists)
    
        Short form: 00? --> x0 --> 00 == 00?
    */    
    f &= _mm_movemask_epi8(_mm_cmpeq_epi16(_mm_and_si128(_mm_or_si128( rr, _mm_srai_epi16(rr,1)), z), z));
    if ( f != 0xffff )
      return 0;
  }
  return 1;
}

/* returns 1, if cube "c" is illegal, return 0 if "c" is not illegal */
int bcp_IsIllegal(bcp p, bc c)
{
  int i, cnt = p->blk_cnt;
  __m128i z = _mm_loadu_si128(bcp_GetBCLCube(p, p->global_cube_list, 1));
  __m128i cc;
  uint16_t f = 0x0ffff;
  for( i = 0; i < cnt; i++ )
  {
    cc = _mm_loadu_si128(c+i);      // load one block
    //print128_num(cc);
    //print128_num(_mm_srai_epi16(cc,1));
    //print128_num(_mm_or_si128( cc, _mm_srai_epi16(cc,1)));
    f &= _mm_movemask_epi8(_mm_cmpeq_epi16(_mm_and_si128(_mm_or_si128( cc, _mm_srai_epi16(cc,1)), z), z));
  }
  if ( f == 0xffff )
    return 0;
  return 1;
}

/*
  return the number of 01 or 10 values in a legal cube.
*/
int bcp_GetCubeVariableCount(bcp p, bc cube)
{
  int i, cnt = p->blk_cnt;
  int delta = 0;
    __m128i c;
  for( i = 0; i < cnt; i++ )
  {
    c = _mm_loadu_si128(cube+i);      // load one block from cube
    delta += __builtin_popcountll(~_mm_cvtsi128_si64(_mm_unpackhi_epi64(c, c)));
    delta += __builtin_popcountll(~_mm_cvtsi128_si64(c));
  }  
  return delta;
}

int bcp_GetCubeDelta(bcp p, bc a, bc b)
{
  int i, cnt = p->blk_cnt;
  int delta = 0;
  __m128i zeromask = _mm_loadu_si128(bcp_GetGlobalCube(p, 1));
  __m128i c;

  for( i = 0; i < cnt; i++ )
  {
    c = _mm_loadu_si128(a+i);      // load one block from a
    c = _mm_and_si128(c,  _mm_loadu_si128(b+i)); // "and" between a&b: how often will there be 00 (=illegal)?
    c = _mm_or_si128( c, _mm_srai_epi16(c,1));  // how often will be there x0?
    c = _mm_andnot_si128(c, zeromask);          // invert c (look for x1) and mask with the zero mask to get 01
    delta += __builtin_popcountll(_mm_cvtsi128_si64(_mm_unpackhi_epi64(c, c)));
    delta += __builtin_popcountll(_mm_cvtsi128_si64(c));
  }
  
  return delta;
}


/*
  test, whether "b" is a subset of "a"
  returns:      
    1: yes, "b" is a subset of "a"
    0: no, "b" is not a subset of "a"
*/
int bcp_IsSubsetCube(bcp p, bc a, bc b)
{
  int i;
  __m128i bb;
  for( i = 0; i < p->blk_cnt; i++ )
  {    
      /* a&b == b ?*/
    bb = _mm_loadu_si128(b+i);
    if ( _mm_movemask_epi8(_mm_cmpeq_epi16(_mm_and_si128(_mm_loadu_si128(a+i), bb), bb )) != 0x0ffff )
      return 0;
  }
  return 1;
}



/*============================================================*/

bcl bcp_NewBCL(bcp p)
{
  bcl l = (bcl)malloc(sizeof(struct bcl_struct));
  if ( l != NULL )
  {
    l->cnt = 0;
    l->max = 0;
    l->last_deleted = -1;
    l->list = NULL;
    l->flags = NULL;
    return l;
  }
  return NULL;
}

bcl bcp_NewBCLByBCL(bcp p, bcl l)
{
  bcl n = bcp_NewBCL(p);
  if ( n != NULL )
  {
    n->list = (__m128i *)malloc(l->cnt*p->bytes_per_cube_cnt);
    if ( n->list != NULL )
    {
      n->flags = (uint8_t *)malloc(l->cnt*sizeof(uint8_t));
      if ( n->flags != NULL )
      {
        n->cnt = l->cnt;
        n->max = l->cnt;
        memcpy(n->list, l->list, l->cnt*p->bytes_per_cube_cnt);
        memcpy(n->flags, l->flags, l->cnt*sizeof(uint8_t));
        return n;
      }
      free(n->list);
    }
    free(n);
  }
  return NULL;
}

/* let a be a copy of b: copy content from bcl b into bcl a */
int bcp_CopyBCL(bcp p, bcl a, bcl b)
{
  if ( a->max < b->cnt )
  {
    __m128i *list;
    uint8_t *flags;
    if ( a->list == NULL )
      list = (__m128i *)malloc(b->cnt*p->bytes_per_cube_cnt);
    else
      list = (__m128i *)realloc(a->list, b->cnt*p->bytes_per_cube_cnt);
    if ( list == NULL )
      return 0;
    a->list = list;
    if ( a->flags == NULL )
      flags = (uint8_t *)malloc(b->cnt*sizeof(uint8_t));
    else
      flags = (uint8_t *)realloc(a->flags, b->cnt*sizeof(uint8_t));
    if ( flags == 0 )
      return 0;
    a->flags = flags;
    a->max = b->cnt;
  }
  a->cnt = b->cnt;
  memcpy(a->list, b->list, a->cnt*p->bytes_per_cube_cnt);
  memcpy(a->flags, b->flags, a->cnt*sizeof(uint8_t));
  return 1;
}

void bcp_ClearBCL(bcp p, bcl l)
{
  l->cnt = 0;
}


void bcp_DeleteBCL(bcp p, bcl l)
{
  if ( l->list != NULL )
    free(l->list);
  if ( l->flags != NULL )
    free(l->flags);
  free(l);
}

#define BCL_EXTEND 32
int bcp_ExtendBCL(bcp p, bcl l)
{
  __m128i *list;
  uint8_t *flags;
  if ( l->list == NULL )
    list = (__m128i *)malloc(BCL_EXTEND*p->bytes_per_cube_cnt);
  else
    list = (__m128i *)realloc(l->list, (BCL_EXTEND+l->max)*p->bytes_per_cube_cnt);
  if ( list == NULL )
    return 0;
  l->list = list;
  if ( l->flags == NULL )
    flags = (uint8_t *)malloc(BCL_EXTEND*sizeof(uint8_t));
  else
    flags = (uint8_t *)realloc(l->flags, (BCL_EXTEND+l->max)*sizeof(uint8_t));
  if ( flags == 0 )
    return 0;
  l->flags = flags;
  
  l->max += BCL_EXTEND;
  return 1;
}

#ifndef bcp_GetBCLCube
bc bcp_GetBCLCube(bcp p, bcl l, int pos)
{
  assert( pos < l->cnt);
  return (bc)(((uint8_t *)(l->list)) + pos * p->bytes_per_cube_cnt);
}
#endif

void bcp_ShowBCL(bcp p, bcl l)
{
  int i, list_cnt = l->cnt;
  for( i = 0; i < list_cnt; i++ )
  {
    printf("%04d %02x %s\n", i, l->flags[i], bcp_GetStringFromCube(p, bcp_GetBCLCube(p, l, i)) );
  }
}

/*
  remove cubes from the list, which are maked as deleted
*/
void bcp_PurgeBCL(bcp p, bcl l)
{
  int i = 0;
  int j = 0;
  int cnt = l->cnt;
  
  while( i < cnt )
  {
    if ( l->flags[i] != 0 )
      break;
    i++;
  }
  j = i;
  i++;  
  for(;;)
  {
    while( i < cnt )
    {
      if ( l->flags[i] == 0 )
        break;
      i++;
    }
    if ( i >= cnt )
      break;
    memcpy((void *)bcp_GetBCLCube(p, l, j), (void *)bcp_GetBCLCube(p, l, i), p->bytes_per_cube_cnt);
    j++;
    i++;
  }
  l->cnt = j;  
  memset(l->flags, 0, l->cnt);
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
  for( i = 0; i < cnt; i++ )
  {
    if ( l->flags[i] == 0 )
    {
      c = bcp_GetBCLCube(p, l, i);
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
            */
            if ( bcp_IsSubsetCube(p, c, bcp_GetBCLCube(p, l, j)) != 0 )
            {
              l->flags[j] = 1;      // mark the j cube as deleted
            }            
          } // j != i
        } // j cube not deleted
      } // j loop
    } // i cube not deleted
  } // i loop
  bcp_PurgeBCL(p, l);
}


/*
  try to expand cubes into another cube
*/
void bcp_DoBCLSimpleExpand(bcp p, bcl l)
{
  int i, j, v;
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
  The cubes, which are marked as subset are not deleted. This is done by a later call to bcp_BCLPurge()
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
  int i, j;
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
        /* todo: a will be smaller, so we need to check, whether a is already a subcube of any cube in l */
        for( j = 0; j < l->cnt; j++ )
        {
          /*
            test, whether second cobe is a subset of the first cube
            returns:      
              1: yes, second is a subset of first cube
              0: no, second cube is not a subset of first cube
          */
          if ( bcp_IsSubsetCube(p, bcp_GetBCLCube(p, l, j), a) != 0 )
            break;
        }
        if ( j == l->cnt )
          /*pos = */ bcp_AddBCLCubeByCube(p, l, a); // add the modified a cube to the list
        bcp_SetCubeVar(p, a, i, orig_aa);        // undo the modification
        //bcp_DoBCLSubsetCubeMark(p, l, pos);             // not sure, whether this will be required
      }
    }
  }
}


/* a = a - b */
void bcp_SubtractBCL(bcp p, bcl a, bcl b)
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

          /*
          for( j = 0; j < cnt; j++ )
          {
            if ( j != i && l->flags[j] == 0  )
            {
              if ( bcp_IsSubsetCube(p, c, bcp_GetBCLCube(p, l, j)) != 0 )
              {
                l->flags[j] = 1;      // mark the j cube as covered (to be deleted)
              }            
            }
          }
          */
          
        } // check for "becomes don't care'
      } // check for not don't care
    }
  } // i loop
  bcp_PurgeBCL(p, l);  // cleanup for bcp_DoBCLSubsetCubeMark()
  
  //printf("bcp_DoBCLOneVariableCofactor post\n");
  //bcp_ShowBCL(p, l);
}

bcl bcp_NewBCLCofacter(bcp p, bcl l, unsigned var_pos, unsigned value)
{
  bcl n = bcp_NewBCLByBCL(p, l);
  if ( n == NULL )
    return NULL;
  bcp_DoBCLOneVariableCofactor(p, n, var_pos, value);
  return n;
}



/* add a cube to the cube list and return the position of the new cube */
int bcp_AddBCLCube(bcp p, bcl l)
{
  //printf("bcp_AddBCLCube cnt = %d\n", l->cnt);
  /* ignore last_deleted for now */
  while ( l->max <= l->cnt )
    if ( bcp_ExtendBCL(p, l) == 0 )
      return -1;
  l->cnt++;
  bcp_ClrCube(p, bcp_GetBCLCube(p, l, l->cnt-1));
  l->flags[l->cnt-1] = 0;
  return l->cnt-1;
}

int bcp_AddBCLCubeByCube(bcp p, bcl l, bc c)
{
  while ( l->max <= l->cnt )
    if ( bcp_ExtendBCL(p, l) == 0 )
      return -1;
  l->cnt++;
  bcp_CopyCube(p, bcp_GetBCLCube(p, l, l->cnt-1), c);  
  l->flags[l->cnt-1] = 0;
  return l->cnt-1;
}

/*
  Adds the cubes from b to list a.
  Technically this is the union of a and b, which i stored in a
  This procedure does not do any simplification

  Note: 
    Maybe we need a bcp_UnionBCL() which also does in place SCC 
*/
int bcp_AddBCLCubesByBCL(bcp p, bcl a, bcl b)
{
  int i;
  for ( i = 0; i < b->cnt; i++ )
  {
    if ( b->flags[i] == 0 )
      if ( bcp_AddBCLCubeByCube(p, a, bcp_GetBCLCube(p, b, i)) < 0 )
        return 0;
  }
  return 1;
}

/*
  add cubes from the given string.
  multiple strings are added if separated by newline 
*/
int bcp_AddBCLCubesByString(bcp p, bcl l, const char *s)
{
  int cube_pos;
  for(;;)
  {
    for(;;)
    {
      if ( *s == '\0' )
        return 1;
      if ( *s > ' ' )
        break;
      s++;
    }
    cube_pos = bcp_AddBCLCube(p, l);
    if ( cube_pos < 0 )
      break;
    bcp_SetCubeByStringPointer(p, bcp_GetBCLCube(p, l, cube_pos),  &s);
  }
  return 0;     // memory error
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


/*
  For the sum calculation use signed 8 bit integer, because SSE2 only has a signed cmplt for 8 bit
  As a consequence, we are somehow loosing one bit in the sum, because saturation will be at 0x7f
*/
#define _mm_adds(a,b) _mm_adds_epi8((a),(b))

void bcp_CalcBCLBinateSplitVariableTable8(bcp p, bcl l)
{
	int i, blk_cnt = p->blk_cnt;
	int j, list_cnt = l->cnt;
	
	bc zero_cnt_cube[4];
	bc one_cnt_cube[4];
	

	__m128i c;  // current block from the current cube from the list
	__m128i t;	// temp block
	__m128i oc0, oc1, oc2, oc3;	// one count
	__m128i zc0, zc1, zc2, zc3;	// zero count
	__m128i mc; // mask cube for the lowest bit in each byte

	/* constuct the byte mask: we need the lowest bit of each byte */
	mc = _mm_setzero_si128();
	mc = _mm_insert_epi16(mc, 0x0101, 0);
	mc = _mm_insert_epi16(mc, 0x0101, 1);
	mc = _mm_insert_epi16(mc, 0x0101, 2);
	mc = _mm_insert_epi16(mc, 0x0101, 3);
	mc = _mm_insert_epi16(mc, 0x0101, 4);
	mc = _mm_insert_epi16(mc, 0x0101, 5);
	mc = _mm_insert_epi16(mc, 0x0101, 6);
	mc = _mm_insert_epi16(mc, 0x0101, 7);

	/* "misuse" the cubes as SIMD storage area for the counters */
	zero_cnt_cube[0] = bcp_GetGlobalCube(p, 4);
	zero_cnt_cube[1] = bcp_GetGlobalCube(p, 5);
	zero_cnt_cube[2] = bcp_GetGlobalCube(p, 6);
	zero_cnt_cube[3] = bcp_GetGlobalCube(p, 7);
	
	one_cnt_cube[0] = bcp_GetGlobalCube(p, 8);
	one_cnt_cube[1] = bcp_GetGlobalCube(p, 9);
	one_cnt_cube[2] = bcp_GetGlobalCube(p, 10);
	one_cnt_cube[3] = bcp_GetGlobalCube(p, 11);
	
	/* loop over the blocks */
	for( i = 0; i < blk_cnt; i++ )
	{
		/* clear all the conters for the current block */
		zc0 = _mm_setzero_si128();
		zc1 = _mm_setzero_si128();
		zc2 = _mm_setzero_si128();
		zc3 = _mm_setzero_si128();
		oc0 = _mm_setzero_si128();
		oc1 = _mm_setzero_si128();
		oc2 = _mm_setzero_si128();
		oc3 = _mm_setzero_si128();
		
		for( j = 0; j < list_cnt; j++ )
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
			oc0 = _mm_adds(oc0, t);		// sum the "one" value with saturation
			c = _mm_srai_epi16(c,1);			// shift right to proceed with the "zero" value
			t = _mm_andnot_si128(c, mc);
			zc0 = _mm_adds(zc0, t);
			
			c = _mm_srai_epi16(c,1);			// shift right to process the next variable

			/* handle variable at bits 2/3 */
			t = _mm_andnot_si128(c, mc);
			oc1 = _mm_adds(oc1, t);
			c = _mm_srai_epi16(c,1);
			t = _mm_andnot_si128(c, mc);
			zc1 = _mm_adds(zc1, t);

			c = _mm_srai_epi16(c,1);

			/* handle variable at bits 4/5 */
			t = _mm_andnot_si128(c, mc);
			oc2 = _mm_adds(oc2, t);
			c = _mm_srai_epi16(c,1);
			t = _mm_andnot_si128(c, mc);
			zc2 = _mm_adds(zc2, t);

			c = _mm_srai_epi16(c,1);

			/* handle variable at bits 6/7 */
			t = _mm_andnot_si128(c, mc);
			oc3 = _mm_adds(oc3, t);
			c = _mm_srai_epi16(c,1);
			t = _mm_andnot_si128(c, mc);
			zc3 = _mm_adds(zc3, t);
		}
		
		/* store the registers for counting zeros and ones */
		_mm_storeu_si128(zero_cnt_cube[0] + i, zc0);
		_mm_storeu_si128(zero_cnt_cube[1] + i, zc1);
		_mm_storeu_si128(zero_cnt_cube[2] + i, zc2);
		_mm_storeu_si128(zero_cnt_cube[3] + i, zc3);
		
		_mm_storeu_si128(one_cnt_cube[0] + i, oc0);
		_mm_storeu_si128(one_cnt_cube[1] + i, oc1);
		_mm_storeu_si128(one_cnt_cube[2] + i, oc2);
		_mm_storeu_si128(one_cnt_cube[3] + i, oc3);
	}
	
        /* 
          based on the calculated number of "one" and "zero" values, find a variable which fits best for splitting.
          According to R. Rudell in "Multiple-Valued Logic Minimization for PLA Synthesis" this should be the
          variable with the highest value of one_cnt + zero_cnt
        */
        
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
		}
		
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
	}
	
        /* 
          based on the calculated number of "one" and "zero" values, find a variable which fits best for splitting.
          According to R. Rudell in "Multiple-Valued Logic Minimization for PLA Synthesis" this should be the
          variable with the highest value of one_cnt + zero_cnt
        */
        
}

/*
  Precondition: call to 
    void bcp_CalcBCLBinateSplitVariableTable(bcp p, bcl l)  

  returns the binate variable for which the minimum of one's and zero's is max
*/
int bcp_GetBCLBalancedBinateSplitVariable8(bcp p, bcl l)
{
  int max_min_cnt = -1;
  int max_min_var = -1;
  
  int cube_idx;
  int blk_idx;
  int byte_idx;
  int one_cnt;
  int zero_cnt;
  int min_cnt;
  
  int i;
  
  //int j, oc, zc;
  
  bc zero_cnt_cube[4];
  bc one_cnt_cube[4];

  /* "misuse" the cubes as SIMD storage area for the counters */
  zero_cnt_cube[0] = bcp_GetGlobalCube(p, 4);
  zero_cnt_cube[1] = bcp_GetGlobalCube(p, 5);
  zero_cnt_cube[2] = bcp_GetGlobalCube(p, 6);
  zero_cnt_cube[3] = bcp_GetGlobalCube(p, 7);
  
  one_cnt_cube[0] = bcp_GetGlobalCube(p, 8);
  one_cnt_cube[1] = bcp_GetGlobalCube(p, 9);
  one_cnt_cube[2] = bcp_GetGlobalCube(p, 10);
  one_cnt_cube[3] = bcp_GetGlobalCube(p, 11);
  
  for( i = 0; i < p->var_cnt; i++ )
  {
          cube_idx = i & 3;
          blk_idx = i / 64;
          byte_idx = (i & 63)>>2;
          one_cnt = ((uint8_t *)(one_cnt_cube[cube_idx] + blk_idx))[byte_idx];
          zero_cnt = ((uint8_t *)(zero_cnt_cube[cube_idx] + blk_idx))[byte_idx];
    
          /*
          oc = 0;
          zc = 0;
          for( j = 0; j < l->cnt; j++ )
          {
            int value = bcp_GetCubeVar(p, bcp_GetBCLCube(p, l, j), i);
            if ( value == 1 ) zc++;
            else if ( value==2) oc++;
          }
          assert( one_cnt == oc );
          assert( zero_cnt == zc );
          */
          
          min_cnt = one_cnt > zero_cnt ? zero_cnt : one_cnt;
          
          // printf("%d: one_cnt=%u zero_cnt=%u\n", i, one_cnt, zero_cnt);
          /* if min_cnt is zero, then there are only "ones" and "don't cares" or "zero" and "don't case". This is usually called unate in that variable */
    
          if ( min_cnt > 0 )
          {
            if ( max_min_cnt < min_cnt )
            {
                    max_min_cnt = min_cnt;
                    max_min_var = i;
            }
          }
  }
  
  /* max_min_var is < 0, then the complete BCL is unate in all variables */
  
  //printf("best variable for split: %d\n", max_min_var);
  return max_min_var;
}


/*
  Precondition: call to 
    void bcp_CalcBCLBinateSplitVariableTable(bcp p, bcl l)  

  returns the binate variable for which the number of one's plus number of zero's is max under the condition, that both number of once's and zero's are >0 

  Implementation without SSE2
*/

int bcp_GetBCLMaxBinateSplitVariableSimple8(bcp p, bcl l)
{
  int max_sum_cnt = -1;
  int max_sum_var = -1;
  int cube_idx;
  int blk_idx;
  int byte_idx;
  int one_cnt;
  int zero_cnt;
  
  int i;
  
  bc zero_cnt_cube[4];
  bc one_cnt_cube[4];

  /* "misuse" the cubes as SIMD storage area for the counters */
  zero_cnt_cube[0] = bcp_GetGlobalCube(p, 4);
  zero_cnt_cube[1] = bcp_GetGlobalCube(p, 5);
  zero_cnt_cube[2] = bcp_GetGlobalCube(p, 6);
  zero_cnt_cube[3] = bcp_GetGlobalCube(p, 7);
  
  one_cnt_cube[0] = bcp_GetGlobalCube(p, 8);
  one_cnt_cube[1] = bcp_GetGlobalCube(p, 9);
  one_cnt_cube[2] = bcp_GetGlobalCube(p, 10);
  one_cnt_cube[3] = bcp_GetGlobalCube(p, 11);

  
  max_sum_cnt = -1;
  max_sum_var = -1;
  for( i = 0; i < p->var_cnt; i++ )
  {
          cube_idx = i & 3;
          blk_idx = i / 64;
          byte_idx = (i & 63)>>2;
          one_cnt = ((uint8_t *)(one_cnt_cube[cube_idx] + blk_idx))[byte_idx];
          zero_cnt = ((uint8_t *)(zero_cnt_cube[cube_idx] + blk_idx))[byte_idx];
          printf("%02x/%02x ", one_cnt, zero_cnt);
    
          if ( one_cnt > 0 && zero_cnt > 0 )
          {
            if ( max_sum_cnt < (one_cnt + zero_cnt) )
            {
                    max_sum_cnt = one_cnt + zero_cnt;
                    max_sum_var = i;
            }
          }
  }  
  printf("\n");
  return max_sum_var;
}

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
int bcp_GetBCLMaxBinateSplitVariable8(bcp p, bcl l)
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
  
  uint8_t m_base_idx[16] = { 0, 4, 8, 12,  16, 20, 24, 28,  32, 36, 40, 44,  48, 52, 56, 60 };
  uint8_t m_base_inc[16] = { 1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1 };
  
  uint8_t m_idx[16];
  uint8_t m_max[16];
  
  bc zero_cnt_cube[4];
  bc one_cnt_cube[4];

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
      c_idx = _mm_loadu_si128((__m128i *)m_base_idx);
      c_max = _mm_setzero_si128();
      c_max_idx = _mm_setzero_si128();
    
      for( i = 0; i < 4; i++ )
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
        c = _mm_cmpeq_epi8(z, _mm_setzero_si128());     // check for zero count of zeros
        o = _mm_andnot_si128 (c, o);                                    // and clear the one count if the zero count is zero
        c = _mm_cmpeq_epi8(o, _mm_setzero_si128());     // check for zero count of ones
        z = _mm_andnot_si128 (c, z);                                    // and clear the zero count if the one count is zero

        /*
        printf("b     %d: ", i);
        print128_num(o);
        
        printf("b     %d: ", i);
        print128_num(z);
        */
        
        // at this point either both o and z are zero or both are not zero        
        // now, calculate the sum of both counts and store the sum in z, o is not required any more
        z = _mm_adds(z, o);

        /*
        printf("z     %d: ", i);
        print128_num(z);
        */
        
        c_cmp = _mm_cmplt_epi8( c_max, z );

        c_max = _mm_or_si128( _mm_andnot_si128(c_cmp, c_max), _mm_and_si128(c_cmp, z) );                        // update max value if required
        c_max_idx = _mm_or_si128( _mm_andnot_si128(c_cmp, c_max_idx), _mm_and_si128(c_cmp, c_idx) );    // update index value if required        
        
        c_idx = _mm_adds_epu8(c_idx, _mm_loadu_si128((__m128i *)m_base_inc));   // we just add 1 to the value, so the highest value per byte is 64
      }
      
      _mm_storeu_si128( (__m128i *)m_max, c_max );
      _mm_storeu_si128( (__m128i *)m_idx, c_max_idx );
      for( i = 0; i < 16; i++ )
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
  f1 = bcp_NewBCLCofacter(p, l, var_pos, 1);
  f2 = bcp_NewBCLCofacter(p, l, var_pos, 2);
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


bcl bcp_NewBCLComplementWithSubtract(bcp p, bcl l)
{
    bcl result = bcp_NewBCL(p);
    bcp_AddBCLCubeByCube(p, result, bcp_GetGlobalCube(p, 3));  // "result" contains the universal cube
    bcp_SubtractBCL(p, result, l);             // "result" contains the negation of "l"
    return result;
}


bcl bcp_NewBCLComplementWithCofactor(bcp p, bcl l)
{
  int var_pos;
  int i, j;
  bcl f1;
  bcl f2;
  bcl cf1;
  bcl cf2;
  
  bcp_CalcBCLBinateSplitVariableTable(p, l);
  var_pos = bcp_GetBCLMaxBinateSplitVariable(p, l);
    
  if ( var_pos < 0 )
  {
    return bcp_NewBCLComplementWithSubtract(p, l);
  }
  
  f1 = bcp_NewBCLCofacter(p, l, var_pos, 1);
  assert(f1 != NULL);
  bcp_DoBCLSimpleExpand(p, f1);
  bcp_DoBCLSingleCubeContainment(p, f1);
  
  f2 = bcp_NewBCLCofacter(p, l, var_pos, 2);
  assert(f2 != NULL);
  bcp_DoBCLSimpleExpand(p, f2);
  bcp_DoBCLSingleCubeContainment(p, f2);
  
  cf1 = bcp_NewBCLComplementWithCofactor(p, f1);
  assert(cf1 != NULL);
  cf2 = bcp_NewBCLComplementWithCofactor(p, f2);
  assert(cf2 != NULL);
  bcp_DeleteBCL(p, f1);
  bcp_DeleteBCL(p, f2);
  
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
    
  if ( bcp_AddBCLCubesByBCL(p, cf1, cf2) == 0 )
    return  bcp_DeleteBCL(p, cf1), bcp_DeleteBCL(p, cf2), NULL;

  bcp_DeleteBCL(p, cf2);

  //bcp_DoBCLSimpleExpand(p, cf1);
  bcp_DoBCLExpandWithOffSet(p, cf1, l);
  bcp_DoBCLSingleCubeContainment(p, cf1);

  
  
  return cf1;
}

/*============================================================*/


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

/*
  error with var_cnt == 20
*/
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
  bcp_SubtractBCL(p, l, t);
  assert( l->cnt == 0 );

  printf("tautology test 3\n");
  tautology = bcp_IsBCLTautology(p, r);
  assert(tautology == 0);

  printf("subtract test 2\n");
  bcp_ClearBCL(p, l);
  bcp_AddBCLCubeByCube(p, l, bcp_GetGlobalCube(p, 3));  // "l" contains the universal cube
  bcp_SubtractBCL(p, l, r);             // "l" contains the negation of "r"
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
  bcp_SubtractBCL(p, l, r);             // "l" contains the negation of "r"
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
  bcp_DoBCLSingleCubeContainment(p, n);
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
  bcp_SubtractBCL(p, m, l);
  
  puts("original:");
  bcp_ShowBCL(p, l);
  puts("complement:");
  bcp_ShowBCL(p, m);

  bcp_IntersectionBCL(p, m, l);
  printf("intersction cube count %d\n", m->cnt);

  bcp_ClearBCL(p, m);
  bcp_AddBCLCubeByCube(p, m, bcp_GetGlobalCube(p, 3));
  bcp_SubtractBCL(p, m, l);
  
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
"-0-1\n"
"1-0-\n"
"-1--\n"
"0--1\n"
  ;
  
  bcp p = bcp_New(bcp_GetVarCntFromString(s));
  bcl l = bcp_NewBCL(p);
  bcl n;
  bcl m;

  bcp_AddBCLCubesByString(p, l, s);

  puts("original:");
  bcp_ShowBCL(p, l);

  n = bcp_NewBCLComplementWithSubtract(p, l);
  puts("complement with subtract:");
  bcp_ShowBCL(p, n);
  
  m = bcp_NewBCLComplementWithCofactor(p, l);
  puts("complement with cofactor:");
  bcp_ShowBCL(p, m);
  
  bcp_DeleteBCL(p,  l);
  bcp_DeleteBCL(p,  n);

  bcp_Delete(p);  
  return 0;
}

#include <time.h>

int main(void)
{
  
  int cnt = 18;
  clock_t t0, t1, t2;
  bcp p = bcp_New(cnt);
  bcl l = bcp_NewBCLWithRandomTautology(p, cnt+2, cnt+10);
  bcl n;
  bcl m;
  
  //puts("original:");
  //bcp_ShowBCL(p, l);

  t0 = clock();
  n = bcp_NewBCLComplementWithSubtract(p, l);
  
  t1 = clock();
  printf("complement with subtract: cnt=%d clock=%ld\n", n->cnt, t1-t0);
  //puts("complement with subtract:");
  //bcp_ShowBCL(p, n);
  
  m = bcp_NewBCLComplementWithCofactor(p, l);
  t2 = clock();
  printf("complement with cofactor: cnt=%d clock=%ld\n", m->cnt, (t2-t1));
  //bcp_ShowBCL(p, m);
  
  bcp_DeleteBCL(p,  l);
  bcp_DeleteBCL(p,  n);
  bcp_DeleteBCL(p,  m);

  bcp_Delete(p);  
  
  internalTest(19);
  
  return 0;
}
