/*

  bcexpression.c

*/
#include "bc.h"
#include <ctyle.h>

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
  bcp_DeleteBCX(x->down);
  bcp_DeleteBCX(x->next);
  x->type = BCX_TYPE_NONE;
  if ( x->identifier != NULL )
  {
    free(x->identifier);
    x->identifier = NULL;
  }
  if ( x->cube_list != NULL )
  {
    bcp_DeleteBCL(x->cube_list);
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



void bcp_ParseError(bcp p, const char *error_msg);
{
  puts(error_msg);
}


/*============================================================*/

static void bcp_skip_space(bcp p, char **s)
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
static void bcp_get_identifier(bcp p, char **s)
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

static int bcp_get_value(bcp p, char **s)
{
  int v = 0;
  if ( isdigit(**s) )
  {
    for(;;)
    {
      if ( **s == '\0' || **s == p->x_end )
        break;
      if ( isdigit(**s) )
        v = (v*10) + (**s) - '0';
    }
  }
  bcp_skip_space(p, s);
  return v;
}

bcx bcp_ParseAtom(bcp p, char **s)
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

bcx bcp_ParseAND(bcp p, char **s)
{
  bcx x = bcp_ParseAtom(p, s);
  bcx binop, xx;
  if ( x == NULL )
    return NULL;
  if ( **s != p->x_or )
    return x;
  
  binop = bcp_NewBCX(p);
  binop->type = BCX_TYPE_OR;
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


bcx bcp_ParseOR(bcp p, char **s)
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
