#ifdef PETSC_RCS_HEADER
static char vcid[] = "$Id: gtype.c,v 1.11 1997/10/19 03:23:45 bsmith Exp bsmith $";
#endif
/*
     Provides utility routines for manulating any type of PETSc object.
*/
#include "petsc.h"  /*I   "petsc.h"    I*/

#undef __FUNC__  
#define __FUNC__ "PetscObjectGetType"
/*@C
   PetscObjectGetType - Gets the object type of any PetscObject.

   Input Parameter:
.  obj - any PETSc object, for example a Vec, Mat or KSP.

   Output Parameter:
.  type - the object type

.keywords: object, get, type
@*/
int PetscObjectGetType(PetscObject obj,int *type)
{
  PetscFunctionBegin;
  if (!obj) SETERRQ(PETSC_ERR_ARG_CORRUPT,0,"Null object");
  *type = obj->type;
  PetscFunctionReturn(0);
}



