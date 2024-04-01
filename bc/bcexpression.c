/*

  bcexpression.c

*/
#include "bc.h"
#include <ctype.h>
#include <string.h>


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

static void bcp_skip_space(bcp p, const char **s)
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
static const char *bcp_get_identifier(bcp p, const char **s)
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

static int bcp_get_value(bcp p, const char **s)
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

void bcp_ShowBCX(bcp p, bcx x)
{
  if ( x->is_not )
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
      printf("%d", x->val);
      x = x->down;
      while( x != NULL )
      {
        bcp_ShowBCX(p, x);
        printf("%c", p->x_and);
        x = x->next;
      }
      break;
    case BCX_TYPE_OR:
      printf("%d", x->val);
      x = x->down;
      while( x != NULL )
      {
        bcp_ShowBCX(p, x);
        printf("%c", p->x_and);
        x = x->next;
      }
      break;
    case BCX_TYPE_BCL:
      printf("BCL");
      break;
    default:
      break;
  }
  if ( x->is_not )
  {
    printf(")");
  }
}


/*============================================================*/


bcx bcp_Parse(bcp p, const char *s)
{
  const char **t = &s; 
  bcp_skip_space(p, t);    
  return bcp_ParseOR(p, t);
}


