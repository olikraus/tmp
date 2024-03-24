/*


  bcljson.c
  
  [
    {
      label:"",
      cmd:"",
      bcl:"",
      var:""   or slot:n
    }     
  ]

  load
    if bcl is provided, then load bcl into accu
    if var is provided, then load bcl from variable into accu
  intersection
    accu = accu intersection var
  subtract
    accu = accu subtract var
  
  
  store
    variable: store content of accu into variable var
  minimize
    minimize content of accu
  
  

*/

#include "co.h"
#include "bc.h"
#include <string.h>
#include <assert.h>

co bc_ExecuteVector(cco in)
{
  bcp p = NULL;
  long cnt = coVectorSize(in);
  long i;
  
  assert( coIsVector(in) );
  for( i = 0; i < cnt; i++ )
  {
    cco cmdmap = coVectorGet(in, i);
    if ( coIsMap(cmdmap) )
    {
      cco cmdname = coMapGet(cmdmap, "cmd");
      if ( cmdname != NULL )
      {  
        if (coIsStr(cmdname))
        {
          const char *cmd = coStrGet(cmdname);
          if ( strcmp(cmd, "load" ) )
          {
            
          }
        }
      }
    }
  } // for all cmd maps
  return NULL;
}

co bc_ExecuteJSON(FILE *fp)
{
  cco in = coReadJSONByFP(fp);
  return NULL;
}

