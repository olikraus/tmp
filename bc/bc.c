/*

  bc.c
  
  boolean cube 
  
  Suggested article: R. Rudell "Multiple-Valued Logic Minimization for PLA Synthesis"
    https://www2.eecs.berkeley.edu/Pubs/TechRpts/1986/734.html
  This code will only use the binary case (and not the multiple value case) which is partly described in the above article.  

  https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#ssetechs=SSE,SSE2&ig_expand=80

  gcc -g -Wall -fsanitize=address bc.c
  
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
bc bcp_GetGlobalCube(bcp p, int pos);
void bcp_CopyGlobalCube(bcp p, bc r, int pos);
void bcp_StartCubeStackFrame(bcp p);
void bcp_EndCubeStackFrame(bcp p);
bc bcp_GetTempCube(bcp p);

void bcp_ClrCube(bcp p, bc c);
void bcp_SetCubeVar(bcp p, bc c, unsigned var_pos, unsigned value);
unsigned bcp_GetCubeVar(bcp p, bc c, unsigned var_pos);
const char *bcp_GetStringFromCube(bcp p, bc c);
void bcp_SetCubeByString(bcp p, bc c, const char *s);
void bcp_CopyCube(bcp p, bc dest, bc src);
int bcp_IsTautologyCube(bcp p, bc c);
int bcp_IntersectionCube(bcp p, bc r, bc a, bc b);
int bcp_IsIllegal(bcp p, bc c);

bcl bcp_NewBCL(bcp p);          // create new empty bcl
bcl bcp_NewBCLByBCL(bcp p, bcl l);      // create a new bcl as a copy of an existing bcl
void bcp_DeleteBCL(bcp p, bcl l);
bc bcp_GetBCLCube(bcp p, bcl l, int pos);
void bcp_ShowBCL(bcp p, bcl l);
void bcp_PurgeBCL(bcp p, bcl l);               /* purge deleted cubes */
void bcp_DoBCLSingleCubeContainment(bcp p, bcl l);
void bcl_DoBCLOneVariableCofactor(bcp p, bcl l, unsigned var_pos, unsigned value);  // calculate cofactor for a list, called by "bcp_NewBCLCofacter"
bcl bcp_NewBCLCofacter(bcp p, bcl l, unsigned var_pos, unsigned value);         // create a new list, which is the cofactor from "l"
int bcp_AddBCLCube(bcp p, bcl l);
int bcp_AddBCLCubesByString(bcp p, bcl l, const char *s);
#define bcp_GetBCLCnt(p, l) ((l)->cnt)
int bcp_GetBCLBestBinateSplitVariable(bcp p, bcl l);


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
			*/
            for( i = 0; i < 4+4+4+1; i++ )
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

bc bcp_GetGlobalCube(bcp p, int pos)
{
  return bcp_GetBCLCube(p, p->global_cube_list, pos);  
}

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

unsigned bcp_GetCubeVar(bcp p, bc c, unsigned var_pos)
{
  uint16_t *ptr = (uint16_t *)c;
  return (ptr[var_pos/8] >> ((var_pos&7)*2)) & 3;
}

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
  test, whether "b" is a subset of "a"
  returns:      
    1: yes, "b" is a subset of "a"
    0: no, "b" is not a subset of "a"
*/
int bcp_IsSubsetCube(bcp p, bc a, bc b)
{
  int i, cnt = p->blk_cnt;
  uint16_t f;
  __m128i bb;
  for( i = 0; i < cnt; i++ )
  {    
      /* a&b == b ?*/
    bb = _mm_loadu_si128(b+i);
    f = _mm_movemask_epi8(_mm_cmpeq_epi16(_mm_and_si128(_mm_loadu_si128(a+i), bb), bb ));
    if ( f != 0x0ffff )
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
  l->flags = flags;
  
  l->max += BCL_EXTEND;
  return 1;
}

bc bcp_GetBCLCube(bcp p, bcl l, int pos)
{
  return (bc)(((uint8_t *)(l->list)) + pos * p->bytes_per_cube_cnt);
}

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
  cubes are marked as deletable by 
    bcp_DoBCLSingleCubeContainment()
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
  Calculate the cofactor of the given list "l" with respect to the variable at "var_pos" and the given "value".
  "value" must be either 1 (zero) or 2 (one)

*/
void bcl_DoBCLOneVariableCofactor(bcp p, bcl l, unsigned var_pos, unsigned value)
{
  int i, j;
  int cnt = l->cnt;
  unsigned v;
  bc c;
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
          for( j = 0; j < cnt; j++ )
          {
            if ( j != i && l->flags[j] == 0  )
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
        } // check for "becomes don't care'
      } // check for not don't care
    }
  } // i loop
  bcp_PurgeBCL(p, l);
}

bcl bcp_NewBCLCofacter(bcp p, bcl l, unsigned var_pos, unsigned value)
{
  bcl n = bcp_NewBCLByBCL(p, l);
  if ( n == NULL )
    return NULL;
  bcl_DoBCLOneVariableCofactor(p, n, var_pos, value);
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

int bcp_GetBCLBestBinateSplitVariable(bcp p, bcl l)
{
	int i, blk_cnt = p->blk_cnt;
	int j, list_cnt = l->cnt;
	int max_min_cnt = -1;
	int max_min_var = -1;
	
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
			oc0 = _mm_adds_epu8(oc0, t);		// sum the "one" value with saturation
			c = _mm_srai_epi16(c,1);			// shift right to proceed with the "zero" value
			t = _mm_andnot_si128(c, mc);
			zc0 = _mm_adds_epu8(zc0, t);
			
			c = _mm_srai_epi16(c,1);			// shift right to process the next variable

			/* handle variable at bits 2/3 */
			t = _mm_andnot_si128(c, mc);
			oc1 = _mm_adds_epu8(oc1, t);
			c = _mm_srai_epi16(c,1);
			t = _mm_andnot_si128(c, mc);
			zc1 = _mm_adds_epu8(zc1, t);

			c = _mm_srai_epi16(c,1);

			/* handle variable at bits 4/5 */
			t = _mm_andnot_si128(c, mc);
			oc2 = _mm_adds_epu8(oc2, t);
			c = _mm_srai_epi16(c,1);
			t = _mm_andnot_si128(c, mc);
			zc2 = _mm_adds_epu8(zc2, t);

			c = _mm_srai_epi16(c,1);

			/* handle variable at bits 6/7 */
			t = _mm_andnot_si128(c, mc);
			oc3 = _mm_adds_epu8(oc3, t);
			c = _mm_srai_epi16(c,1);
			t = _mm_andnot_si128(c, mc);
			zc3 = _mm_adds_epu8(zc3, t);
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
        
	for( i = 0; i < p->var_cnt; i++ )
	{
		int cube_idx = i & 3;
		int blk_idx = i / 64;
		int byte_idx = (i & 63)>>2;
		int one_cnt = ((uint8_t *)(one_cnt_cube[cube_idx] + blk_idx))[byte_idx];
		int zero_cnt = ((uint8_t *)(zero_cnt_cube[cube_idx] + blk_idx))[byte_idx];
		
		int min_cnt = one_cnt > zero_cnt ? zero_cnt : one_cnt;
		
		// printf("%d: one_cnt=%u zero_cnt=%u\n", i, one_cnt, zero_cnt);
		
                if ( min_cnt > 0 )
                {
                  if ( max_min_cnt < min_cnt )
                  {
                          max_min_cnt = min_cnt;
                          max_min_var = i;
                  }
                }
	}
        //printf("best variable for split: %d\n", max_min_var);
	return max_min_var;
}



int bcp_IsBCLTautology(bcp p, bcl l)
{
  int var_pos = bcp_GetBCLBestBinateSplitVariable(p, l);
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
  bcl f2 = bcp_NewBCLCofacter(p, l, var_pos, 2);
  bcl f3 = bcp_NewBCLCofacter(p, l, var_pos, 3);
  assert( f2 != NULL );
  assert( f3 != NULL );
  
  if ( bcp_IsBCLTautology(p, f2) == 0 )
    return bcp_DeleteBCL(p,  f2), bcp_DeleteBCL(p,  f3), 0;
  if ( bcp_IsBCLTautology(p, f3) == 0 )
    return bcp_DeleteBCL(p,  f2), bcp_DeleteBCL(p,  f3), 0;
  
  return bcp_DeleteBCL(p,  f2), bcp_DeleteBCL(p,  f3), 1;
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
"------\n"
"------\n"
;



int mainy(void)
{
  int i;
  bcp p = bcp_New(bcp_GetVarCntFromString(cubes_string));
  bcl l = bcp_NewBCL(p);
  bcp_AddBCLCubesByString(p, l, cubes_string);
  
  
  bcp_ShowBCL(p, l);
  for( i = 0; i < bcp_GetBCLCnt(p, l); i++ )
  {
    bc c = bcp_GetBCLCube(p, l, i);
    printf("%s t:%d, illegal:%d\n", 
      bcp_GetStringFromCube(p, c), 
      bcp_IsTautologyCube(p, c), 
      bcp_IsIllegal(p, c) );
  }

  bcp_GetBCLBestBinateSplitVariable(p, l);
  // bcl_DoBCLOneVariableCofactor(bcp p, bcl l, unsigned var_pos, unsigned value)

  bcp_DoBCLSingleCubeContainment(p, l);
  bcp_ShowBCL(p, l);
  bcp_DeleteBCL(p,  l);
  bcp_Delete(p); 
  return 0;
}



int main(void)
{
  char *s = 
"----1\n"
"---10\n"
"---00\n"
  ;
  bcp p = bcp_New(bcp_GetVarCntFromString(s));
  bcl l = bcp_NewBCL(p);
  bcp_AddBCLCubesByString(p, l, s);
  bcp_ShowBCL(p, l);
  printf("tautology=%d\n", bcp_IsBCLTautology(p, l));
  bcp_DeleteBCL(p,  l);
  bcp_Delete(p);  
  return 0;
}
