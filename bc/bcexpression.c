/*

  bcexpression.c

*/
#include "bc.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


/*============================================================*/
/* forward declaration */
bcx bcp_ParseOR(bcp p, const char **s);

/*============================================================*/

bcx bcp_NewBCX(bcp p)
{
  bcx x = (bcx)malloc(sizeof(struct bcx_struct));
  if ( x != NULL )
  {
    x->type = BCX_TYPE_NONE;
    x->is_not = 0;
    x->next = NULL;
    x->down = NULL;
    x->val = 0;
    x->identifier = 0;
    x->cube_list = NULL;
    return x;
  }
  return NULL;
}

void bcp_DeleteBCX(bcp p, bcx x)
{
  if ( x == NULL )
    return;
  bcp_DeleteBCX(p, x->down);
  bcp_DeleteBCX(p, x->next);
  x->type = BCX_TYPE_NONE;
  if ( x->identifier != NULL )
  {
    free(x->identifier);
    x->identifier = NULL;
  }
  if ( x->cube_list != NULL )
  {
    bcp_DeleteBCL(p, x->cube_list);
    x->cube_list = NULL;
  }
  free(x);
}


bcx bcp_NewBCXValue(bcp p, int v)
{
  bcx x = bcp_NewBCX(p);
  if ( x != NULL )
  {
    x->type = BCX_TYPE_NUM;
    x->val = v;
    return x;
  }
  return NULL;
}

bcx bcp_NewBCXIdentifier(bcp p, const char *identifier)
{
  bcx x = bcp_NewBCX(p);
  if ( x != NULL )
  {
    x->type = BCX_TYPE_ID;
    x->identifier = strdup(identifier);
    if ( x->identifier != NULL )
    {
      return x;
    }
    bcp_DeleteBCX(p, x);
  }
  return NULL;
}



void bcp_ParseError(bcp p, const char *error_msg)
{
  puts(error_msg);
}


/*============================================================*/

void bcp_skip_space(bcp p, const char **s)
{
  for(;;)
  {
    if ( **s == '\0' )
      break;
    if ( **s == p->x_end )
      break;
    if ( **s > 32 )
      break;
    (*s)++;
  }
}

#define BCP_IDENTIFIER_MAX 1024
const char *bcp_get_identifier(bcp p, const char **s)
{
  static char identifier[BCP_IDENTIFIER_MAX];
  int i = 0;
  identifier[0] = '\0';
  if ( isalpha(**s) || **s == '_' )
  {
    for(;;)
    {
      if ( **s == '\0' || **s == p->x_end )
        break;
      if ( isalnum(**s) || **s == '_' )
      {
        if ( i+1 < BCP_IDENTIFIER_MAX )
          identifier[i++] = **s;
        (*s)++;
      }
      else
      {
        break;
      }
    }
  }
  identifier[i] = '\0';
  bcp_skip_space(p, s);
  return identifier;
}

int bcp_get_value(bcp p, const char **s)
{
  int v = 0;
  if ( isdigit(**s) )
  {
    for(;;)
    {
      if ( **s == '\0' || **s == p->x_end )
        break;
      if ( isdigit(**s) )
      {
        v = (v*10) + (**s) - '0';
        (*s)++;
      }
      else
      {
        break;
      }
    } // for
  }
  bcp_skip_space(p, s);
  return v;
}

/*============================================================*/

bcx bcp_ParseAtom(bcp p, const char **s)
{
  static char msg[32];
  bcx x;
  
  if ( **s == '(' )
  {
    (*s)++;
    bcp_skip_space(p, s);
    x = bcp_ParseOR(p, s);
    if ( x == NULL )
      return NULL;
    if ( **s != ')' )
    {
      bcp_ParseError(p, "Missing ')'");
      bcp_DeleteBCX(p, x);
      return NULL;
    }
    (*s)++;
    bcp_skip_space(p, s);
    return x;
  }
  else if ( isdigit(**s) )
  {
    return bcp_NewBCXValue(p, bcp_get_value(p, s));
  }
  else if ( isalpha(**s) || **s == '_' )
  {
    return bcp_NewBCXIdentifier(p, bcp_get_identifier(p, s) );
  }
  else if ( **s == p->x_not )
  {
    (*s)++;
    bcp_skip_space(p, s);
    x = bcp_ParseAtom(p, s);
    if ( x == NULL )
      return NULL;
    x->is_not = !x->is_not;
    return x;
  }
  sprintf(msg, "Unknown char '%c'", **s);       // 17
  bcp_ParseError(p, msg);
  return NULL;
}

bcx bcp_ParseAND(bcp p, const char **s)
{
  bcx x = bcp_ParseAtom(p, s);
  bcx binop, xx;
  if ( x == NULL )
    return NULL;
  if ( **s != p->x_and )
    return x;
  
  binop = bcp_NewBCX(p);
  binop->type = BCX_TYPE_AND;
  binop->down = x;
  
  while ( **s == p->x_and )
  {
    (*s)++;
    bcp_skip_space(p, s);
    xx = bcp_ParseAtom(p, s);
    if ( xx == NULL )
    {
      bcp_DeleteBCX(p, binop);
      return NULL;
    }
    x->next = xx;
    x = xx;
  }
  
  return binop;
}


bcx bcp_ParseOR(bcp p, const char **s)
{
  bcx x = bcp_ParseAND(p, s);
  bcx binop, xx;
  if ( x == NULL )
    return NULL;
  if ( **s != p->x_or )
    return x;
  
  binop = bcp_NewBCX(p);
  binop->type = BCX_TYPE_OR;
  binop->down = x;
  
  while ( **s == p->x_or )
  {
    (*s)++;
    bcp_skip_space(p, s);
    xx = bcp_ParseAND(p, s);
    if ( xx == NULL )
    {
      bcp_DeleteBCX(p, binop);
      return NULL;
    }
    x->next = xx;
    x = xx;
  }
  
  return binop;
}


/*============================================================*/

/*
  add variable "s" to p->var_map and assign a position number has value to the variable name
    key: variable name
    value: position number, starting with 0
*/
int bcp_AddVar(bcp p, const char *s)
{
  if ( p->var_map == NULL )
  {
    p->var_map = coNewMap(CO_STRDUP|CO_STRFREE|CO_FREE_VALS);
    if ( p->var_map == NULL )
      return 0;
  }  
  if ( coMapExists(p->var_map, s) )
    return 1;
  return coMapAdd(p->var_map, s, coNewDbl(p->x_var_cnt++));
}

int bcp_AddVarsFromBCX(bcp p, bcx x)
{
  if ( x == NULL )
    return 1;
  if ( x->type == BCX_TYPE_ID )
    bcp_AddVar(p, x->identifier);
  if ( bcp_AddVarsFromBCX(p, x->down) == 0 )
    return 0;
  return bcp_AddVarsFromBCX(p, x->next);
}

/* 
  build var_list from var_map 
  The value from var_map is the index position for var_list.
  This means:
    for a given string s
      var_list[var_map[s]] == s
    and for a given index:
      var_map[var_list[index]] == index
*/
int bcp_BuildVarList(bcp p)
{
  int i;
  coMapIterator iter;
  if ( p->var_list == NULL )
  {
    p->var_list = coNewVector(CO_FREE_VALS);
    if ( p->var_list == NULL )
      return 0;
  }
  coVectorClear(p->var_list);
  for( i = 0; i < p->x_var_cnt; i++ )
  {
    if ( coVectorAdd(p->var_list, NULL) < 0 )
      return 0;
  }
  

  if ( coMapLoopFirst(&iter, p->var_map) )
  {
    do 
    {
      i = coDblGet(coMapLoopValue(&iter));
      coVectorSet( p->var_list, i, coNewStr(0, coMapLoopKey(&iter)) );
    } while( coMapLoopNext(&iter) );
  }
  
  return 1;
}

/*============================================================*/


bcx bcp_Parse(bcp p, const char *s)
{
  const char **t = &s; 
  bcp_skip_space(p, t);    
  return bcp_ParseOR(p, t);
}

/*============================================================*/

void bcp_ShowBCX(bcp p, bcx x)
{
  int is_not;
  if ( x == NULL )
    return;
  is_not = x->is_not;
  if ( is_not )
  {
    printf("%c(", p->x_not);
  }
  switch(x->type)
  {
    case BCX_TYPE_ID:
      printf("%s", x->identifier);
      break;
    case BCX_TYPE_NUM:
      printf("%d", x->val);
      break;
    case BCX_TYPE_AND:
      x = x->down;
      printf("(");
      while( x != NULL )
      {
        bcp_ShowBCX(p, x);
        if ( x->next == NULL )
          break;
        printf("%c", p->x_and);
        x = x->next;
      }
      printf(")");
      break;
    case BCX_TYPE_OR:
      x = x->down;
      printf("(");
      while( x != NULL )
      {
        bcp_ShowBCX(p, x);
        if ( x->next == NULL )
          break;
        printf("%c", p->x_or);
        x = x->next;
      }
      printf(")");
      break;
    case BCX_TYPE_BCL:
      printf("BCL");
      break;
    default:
      break;
  }
  if ( is_not )
  {
    printf(")");
  }
}

/*============================================================*/

void bcp_PrintBCX(bcp p, bcx x)
{
  if ( x == NULL )
    return;
  printf("%p: t=%d id=%s d=%p n=%p\n", x, x->type, x->identifier,  x->down, x->next);
  bcp_PrintBCX(p, x->down);
  bcp_PrintBCX(p, x->next);
}

bcl bcp_NewBCLById(bcp p, int is_not, const char *identifier)
{
  bcl l;
  int cube_pos;
  cco var_pos_co;
  unsigned var_pos;
  
  l = bcp_NewBCL(p);
  if ( l != NULL )
  {
    cube_pos = bcp_AddBCLCube(p, l);
    if ( cube_pos >= 0 )
    {
      var_pos_co = coMapGet(p->var_map, identifier);
      if ( var_pos_co != NULL )
      {
        assert( coIsDbl(var_pos_co) );
        var_pos = (int)coDblGet(var_pos_co);
        assert( var_pos < p->var_cnt );
        bcp_SetCubeVar(p, bcp_GetBCLCube(p, l, cube_pos), var_pos, is_not?1:2);
        return l;
      }
    }
    bcp_DeleteBCL(p, l);
  }
  return NULL; // error
}

bcl bcp_NewBCLByBCX(bcp p, bcx x)
{
  bcl l;
  int is_not = x->is_not;
  switch(x->type)
  {
    case BCX_TYPE_ID:
      assert( x->down == NULL );
      return bcp_NewBCLById(p, is_not, x->identifier);
    case BCX_TYPE_NUM:
      assert( x->down == NULL );
      if ( (is_not == 0 && x->val == 0) ||  (is_not != 0 && x->val != 0) )
          return bcp_NewBCL(p); // empty list
      return bcp_NewBCLWithCube(p, 3);  // tautology list
    case BCX_TYPE_AND:
      x = x->down;
      l = bcp_NewBCLByBCX(p, x);
      x = x->next;
      while( x != NULL )
      {
        bcl ll = bcp_NewBCLByBCX(p, x);
        assert( ll != NULL );
        bcp_IntersectionBCL(p, l, ll);
        bcp_DeleteBCL(p, ll);
        x = x->next;
      }
      if ( is_not )
      {
        bcl ll = bcp_NewBCLComplement(p, l);
        bcp_DeleteBCL(p, l);
        return ll;
      }
      return l;
    case BCX_TYPE_OR:
      x = x->down;
      l = bcp_NewBCLByBCX(p, x);
      x = x->next;
      while( x != NULL )
      {
        bcl ll = bcp_NewBCLByBCX(p, x);
        assert( ll != NULL );
        bcp_AddBCLCubesByBCL(p, l, ll);
        bcp_DeleteBCL(p, ll);
        bcp_DoBCLSingleCubeContainment(p, l);
        x = x->next;
      }
      if ( is_not )
      {
        bcl ll = bcp_NewBCLComplement(p, l);
        bcp_DeleteBCL(p, l);
        return ll;
      }
      return l;
    default:
      printf("illegal type %d\n", x->type);
      return NULL;
  }
  assert(0);
  return NULL;
}

