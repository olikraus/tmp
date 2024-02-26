/*

  bc.c
  
  boolean cube 

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
const char *bcp_GetStringFromCube(bcp p, bc c);
void bcp_SetCubeByString(bcp p, bc c, const char *s);
void bcp_CopyCube(bcp p, bc dest, bc src);
int bcp_IsTautologyCube(bcp p, bc c);
int bcp_IntersectionCube(bcp p, bc r, bc a, bc b);
int bcp_IsIllegal(bcp p, bc c);

bcl bcp_NewBCL(bcp p);
void bcp_DeleteBCL(bcp p, bcl l);
bc bcp_GetBCLCube(bcp p, bcl l, int pos);
void bcp_ShowBCL(bcp p, bcl l);
int bcp_AddBCLCube(bcp p, bcl l);
int bcp_AddBCLCubesByString(bcp p, bcl l, const char *s);
#define bcp_GetBCLCnt(p, l) ((l)->cnt)

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

/*
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
*/

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
  int cnt;
  for(;;)
  {
    while( *s == ' ' || *s == '\t' )            // skip white space
      s++;
    if ( *s == '\0' || *s == '\r' || *s == '\n' )       // stop looking at further chars if the line/string ends
      return cnt;
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
            for( i = 0; i < 4; i++ )
              bcp_AddBCLCube(p, p->global_cube_list);
            if ( p->global_cube_list->cnt == 4 )
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
    flags = (uint8_t *)realloc(l->list, (BCL_EXTEND+l->max)*sizeof(uint8_t));
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
"x---\n"
"1100\n"
"1-0-\n"
"1x01\n"
"----\n"
;

int main(void)
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

  bcp_DeleteBCL(p,  l);
  bcp_Delete(p);  
}
