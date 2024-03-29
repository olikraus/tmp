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

  Note: slot always defaults to 0

  bcl2slot
    load the provided bcl into the given slot
    { "cmd":"bcl2slot", "bcl":"110-", "slot":0 }

  show
    if bcl is present, then show bcl
      { "cmd":"show", "slot":0 }
    if slot is present, then show the slot content
      { "cmd":"show", "bcl":"0011" }

  intersection0
    Calculate intersection and store the result in slot 0
    if bcl is present, then calculate intersection between slot 0 and bcl.
      { "cmd":"intersection0", "bcl":"11-0" }
    if slot is present, then calculate intersection between slot 0 and the provided slot
      { "cmd":"intersection0", "slot":1 }
  
  
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

#define SLOT_CNT 9

int bc_ExecuteVector(cco in)
{
  bcp p = NULL;
  bcl slot_list[SLOT_CNT];
  int var_cnt = -1;
  long cnt = coVectorSize(in);
  long i;
  cco o;
  const char *cmd = NULL;
  int slot = 0;
  const char *bclstr = NULL;
  bcl l = NULL;
  bcl arg;              // argument.. either l or a slot
  int result = 0;
  
  for( i = 0; i < SLOT_CNT; i++ )
    slot_list[i] = NULL;
  
  assert( coIsVector(in) );
  for( i = 0; i < cnt; i++ )
  {
    cco cmdmap = coVectorGet(in, i);
    cmd = NULL;
    slot = 0;
    if ( l != NULL )
      bcp_DeleteBCL(p, l);
    l = NULL;
    if ( coIsMap(cmdmap) )
    {
      
      // STEP 1: Read the member variables from a cmd block: "cmd", "slot", "bcl"
      
      o = coMapGet(cmdmap, "cmd");
      if (coIsStr(o))
        cmd = coStrGet(o);

      o = coMapGet(cmdmap, "slot");
      if (coIsDbl(o))
      {
        slot = (int)coDblGet(o);
        if ( slot >= SLOT_CNT || slot < 0 )
          slot = 0;
      }
      
      o = coMapGet(cmdmap, "bcl");
      if (coIsStr(o))
      {
        bclstr = coStrGet(o);
        if ( p == NULL )
        {
          var_cnt = bcp_GetVarCntFromString(bclstr);
          if ( var_cnt > 0 )
          {
            p = bcp_New(var_cnt);
            assert( p != NULL );
          }
        }
        l = bcp_NewBCLByString(p, bclstr);
        assert( l != NULL );
      }
      else if ( coIsVector(o) )
      {
        long i;
        for( i = 0; i < coVectorSize(o); i++ )
        {
          cco so = coVectorGet(o, i);
          if (coIsStr(so))
          {
            bclstr = coStrGet(so);
            if ( p == NULL )
            {
              var_cnt = bcp_GetVarCntFromString(bclstr);
              if ( var_cnt > 0 )
              {
                p = bcp_New(var_cnt);
                assert( p != NULL );
              }
            } // p == NULL
            if ( l == NULL )
            {
              l = bcp_NewBCL(p);
              assert( l != NULL );
            } // l == NULL
            result = bcp_AddBCLCubesByString(p, l, bclstr);
            assert( result != 0 );
            
          } // coIsStr
        } // for
      } // bcl is vector

      // STEP 2: Execute the command
      arg = (l!=NULL)?l:slot_list[slot];

      // "bcl2slot"  "bcl" into "slot"
      if ( p != NULL && strcmp(cmd, "bcl2slot") == 0 )
      {
        if ( l != NULL )
        {
          if ( slot_list[slot] != NULL )
            bcp_DeleteBCL(p, slot_list[slot]);
          slot_list[slot] = l;
          l = NULL;
        }
      }
      // "show"  "bcl" or "show" bcl from "slot"
      else if ( p != NULL &&  strcmp(cmd, "show") == 0 )
      {
        assert(arg != NULL);
        bcp_ShowBCL(p, arg);
      }
      // intersection0: calculate intersection with slot 0
      // result is stored in slot 0
      else if ( p != NULL &&  strcmp(cmd, "intersection0") == 0 )
      {
        int intersection_result;
        assert(slot_list[0] != NULL);
        assert(arg != NULL);
        bcp_IntersectionBCL(p, slot_list[0], arg);   // a = a intersection with b 
        assert(intersection_result != 0);
      }
      

      
    } // isMap
    
  } // for all cmd maps
  
  // Memory cleanup
  
  for( i = 0; i < SLOT_CNT; i++ )
    if ( slot_list[i] != NULL )
      bcp_DeleteBCL(p, slot_list[i]);
    
  if ( l != NULL )
      bcp_DeleteBCL(p, l);

  bcp_Delete(p);
  
  return 1;
}

int bc_ExecuteJSON(FILE *fp)
{
  co in = coReadJSONByFP(fp);
  if ( in == NULL )
    return puts("JSON read errror"), 0;
  bc_ExecuteVector(in);
  coDelete(in);
  return 1;
}

