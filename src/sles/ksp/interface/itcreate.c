#ifdef PETSC_RCS_HEADER
static char vcid[] = "$Id: itcreate.c,v 1.106 1997/11/03 04:43:19 bsmith Exp bsmith $";
#endif
/*
     The basic KSP routines, Create, View etc. are here.
*/
#include "petsc.h"
#include "src/ksp/kspimpl.h"      /*I "ksp.h" I*/
#include "src/sys/nreg.h"     /*I "src/sys/nreg.h" I*/
#include "sys.h"
#include "viewer.h"       /*I "viewer.h" I*/
#include "pinclude/pviewer.h"

int KSPRegisterAllCalled = 0;

#undef __FUNC__  
#define __FUNC__ "KSPView"
/*@ 
   KSPView - Prints the KSP data structure.

   Input Parameters:
.  ksp - the Krylov space context
.  viewer - visualization context

   Note:
   The available visualization contexts include
$     VIEWER_STDOUT_SELF - standard output (default)
$     VIEWER_STDOUT_WORLD - synchronized standard
$       output where only the first processor opens
$       the file.  All other processors send their 
$       data to the first processor to print. 

   The user can open alternative vistualization contexts with
$    ViewerFileOpenASCII() - output to a specified file

.keywords: KSP, view

.seealso: PCView(), ViewerFileOpenASCII()
@*/
int KSPView(KSP ksp,Viewer viewer)
{
  FILE        *fd;
  char        *method;
  int         ierr;
  ViewerType  vtype;

  PetscFunctionBegin;
  ierr = ViewerGetType(viewer,&vtype); CHKERRQ(ierr);
  if (vtype == ASCII_FILE_VIEWER || vtype == ASCII_FILES_VIEWER) {
    ierr = ViewerASCIIGetPointer(viewer,&fd); CHKERRQ(ierr);
    PetscFPrintf(ksp->comm,fd,"KSP Object:\n");
    KSPGetType(ksp,PETSC_NULL,&method);
    PetscFPrintf(ksp->comm,fd,"  method: %s\n",method);
    if (ksp->view) (*ksp->view)((PetscObject)ksp,viewer);
    if (ksp->guess_zero) PetscFPrintf(ksp->comm,fd,
      "  maximum iterations=%d, initial guess is zero\n",ksp->max_it);
    else PetscFPrintf(ksp->comm,fd,"  maximum iterations=%d\n", ksp->max_it);
    PetscFPrintf(ksp->comm,fd,
      "  tolerances:  relative=%g, absolute=%g, divergence=%g\n",
      ksp->rtol, ksp->atol, ksp->divtol);
    if (ksp->pc_side == PC_RIGHT) PetscFPrintf(ksp->comm,fd,"  right preconditioning\n");
    else if (ksp->pc_side == PC_SYMMETRIC) 
      PetscFPrintf(ksp->comm,fd,"  symmetric preconditioning\n");
    else PetscFPrintf(ksp->comm,fd,"  left preconditioning\n");
  } else if (vtype == STRING_VIEWER) {
    KSPType type;
    KSPGetType(ksp,&type,&method);
    if (type != KSPPREONLY) {    
      ierr = ViewerStringSPrintf(viewer," %-7.7s",method); CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

static NRList *__KSPList = 0;
#undef __FUNC__  
#define __FUNC__ "KSPCreate"
/*@C
   KSPCreate - Creates the default KSP context.

   Output Parameter:
.  ksp - location to put the KSP context
.  comm - MPI communicator

   Notes:
   The default KSP type is GMRES with a restart of 30.

.keywords: KSP, create, context

.seealso: KSPSetUp(), KSPSolve(), KSPDestroy()
@*/
int KSPCreate(MPI_Comm comm,KSP *ksp)
{
  KSP ctx;
  int ierr;

  PetscFunctionBegin;
  *ksp = 0;
  PetscHeaderCreate(ctx,_p_KSP,KSP_COOKIE,KSPGMRES,comm,KSPDestroy,KSPView);
  PLogObjectCreate(ctx);
  *ksp               = ctx;
  ctx->view          = 0;

  ctx->type          = (KSPType) -1;
  ctx->max_it        = 10000;
  ctx->pc_side       = PC_LEFT;
  ctx->use_pres      = 0;
  ctx->rtol          = 1.e-5;
  ctx->atol          = 1.e-50;
  ctx->divtol        = 1.e4;

  ctx->guess_zero          = 1;
  ctx->calc_sings          = 0;
  ctx->calc_res            = 0;
  ctx->residual_history    = 0;
  ctx->res_hist_size       = 0;
  ctx->res_act_size        = 0;
  ctx->numbermonitors      = 0;
  ctx->adjust_work_vectors = 0;
  ctx->converged           = KSPDefaultConverged;
  ctx->buildsolution       = KSPDefaultBuildSolution;
  ctx->buildresidual       = KSPDefaultBuildResidual;

  ctx->vec_sol   = 0;
  ctx->vec_rhs   = 0;
  ctx->B         = 0;

  ctx->solver    = 0;
  ctx->setup     = 0;
  ctx->destroy   = 0;
  ctx->adjustwork= 0;

  ctx->data          = 0;
  ctx->nwork         = 0;
  ctx->work          = 0;

  ctx->cnvP          = 0;

  ctx->setupcalled   = 0;
  /* this violates our rule about seperating abstract from implementations*/
  ierr = KSPSetType(*ksp,KSPGMRES);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
 
#undef __FUNC__  
#define __FUNC__ "KSPSetType"
/*@
   KSPSetType - Builds KSP for a particular solver. 

   Input Parameter:
.  ctx      - the Krylov space context
.  itmethod - a known method

   Options Database Command:
$  -ksp_type  <method>
$      Use -help for a list of available methods
$      (for instance, cg or gmres)

   Notes:  
   See "petsc/include/ksp.h" for available methods (for instance,
   KSPCG or KSPGMRES).

  Normally, it is best to use the SLESSetFromOptions() command and
  then set the KSP type from the options database rather than by using
  this routine.  Using the options database provides the user with
  maximum flexibility in evaluating the many different Krylov methods.
  The KSPSetType() routine is provided for those situations where it
  is necessary to set the iterative solver independently of the command
  line or options database.  This might be the case, for example, when
  the choice of iterative solver changes during the execution of the
  program, and the user's application is taking responsibility for
  choosing the appropriate method.  In other words, this routine is
  for the advanced user.

.keywords: KSP, set, method
@*/
int KSPSetType(KSP ksp,KSPType itmethod)
{
  int ierr,(*r)(KSP);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ksp,KSP_COOKIE);
  if (ksp->type == (int) itmethod) PetscFunctionReturn(0);

  if (ksp->setupcalled) {
    /* destroy the old private KSP context */
    ierr = (*(ksp)->destroy)((PetscObject)ksp); CHKERRQ(ierr);
    ksp->data = 0;
  }
  /* Get the function pointers for the iterative method requested */
  if (!KSPRegisterAllCalled) {ierr = KSPRegisterAll(); CHKERRQ(ierr);}
  r =  (int (*)(KSP))NRFindRoutine( __KSPList, (int)itmethod, (char *)0 );
  if (!r) {SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,0,"Unknown method");}
  if (ksp->data) PetscFree(ksp->data);
  ksp->data = 0;
  ierr = (*r)(ksp); CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "KSPRegister"
/*@C
   KSPRegister - Adds the iterative method to the KSP package,  given
   an iterative name (KSPType) and a function pointer.

   Input Parameters:
.  name   - either a predefined name such as KSPCG, or KSPNEW
            to indicate a new user-defined iterative method
.  sname  - corresponding string for name
.  create - routine to create method context

   Output Parameter:
.  oname - type for new method

   Notes:
   Multiple user-defined iterative methods can be added by calling
   KSPRegister() with the input parameter "name" set to be KSPNEW; 
   each call will return a unique iterative method type in the output
   parameter "oname".

.keywords: KSP, register

.seealso: KSPRegisterAll(), KSPRegisterDestroy()
@*/
int  KSPRegister(KSPType name, KSPType *oname,char *sname, int  (*create)(KSP))
{
  int ierr;
  static int numberregistered = 0;

  PetscFunctionBegin;
  if (name == KSPNEW) name = (KSPType) ((int) KSPNEW + numberregistered++);

  if (oname) *oname = name;
  if (!__KSPList) {ierr = NRCreate(&__KSPList); CHKERRQ(ierr);}
  ierr = NRRegister( __KSPList, (int) name, sname, (int (*)(void*))create );CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "KSPRegisterDestroy"
/*@C
   KSPRegisterDestroy - Frees the list of KSP methods that were
   registered by KSPRegister().

.keywords: KSP, register, destroy

.seealso: KSPRegister(), KSPRegisterAll()
@*/
int KSPRegisterDestroy()
{
  PetscFunctionBegin;
  if (__KSPList) {
    NRDestroy( __KSPList );
    __KSPList = 0;
  }
  KSPRegisterAllCalled = 0;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "KSPGetType"
/*@C
   KSPGetType - Gets the KSP type and method name (as a string) from 
   the method type.

   Input Parameter:
.  ksp - Krylov context 

   Output Parameters:
.  itmeth - KSP method (or use PETSC_NULL)
.  name - name of KSP method (or use PETSC_NULL)

.keywords: KSP, get, method, name
@*/
int KSPGetType(KSP ksp,KSPType *type,char **name)
{
  int ierr;
  PetscFunctionBegin;
  if (!__KSPList) {ierr = KSPRegisterAll(); CHKERRQ(ierr);}
  if (type) *type = (KSPType) ksp->type;
  if (name)  *name = NRFindName( __KSPList, (int) ksp->type);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "KSPPrintTypes_Private"
/*
   KSPPrintTypes_Private - Prints the KSP methods available from the options 
   database.

   Input Parameters:
.  comm   - The communicator (usually MPI_COMM_WORLD)
.  prefix - prefix (usually "-")
.  name   - the options database name (by default "ksp_type") 
*/
int KSPPrintTypes_Private(MPI_Comm comm,char* prefix,char *name)
{
  FuncList *entry;
  int      count = 0;

  PetscFunctionBegin;
  if (!__KSPList) {KSPRegisterAll();}
  entry = __KSPList->head;
  PetscPrintf(comm," %s%s (one of)",prefix,name);
  while (entry) {
    PetscPrintf(comm," %s",entry->name);
    entry = entry->next;
    count++;
    if (count == 8) PetscPrintf(comm,"\n    ");
  }
  PetscPrintf(comm,"\n");
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "KSPPrintHelp"
/*@ 
   KSPPrintHelp - Prints all options for the KSP component.

   Input Parameter:
.  ksp - the KSP context

   Options Database Keys:
$  -help, -h

.keywords: KSP, help

.seealso: KSPSetFromOptions()
@*/
int KSPPrintHelp(KSP ksp)
{
  char p[64];
  int  ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ksp,KSP_COOKIE);
  PetscStrcpy(p,"-");
  if (ksp->prefix)  PetscStrcat(p,ksp->prefix);

  PetscPrintf(ksp->comm,"KSP options -------------------------------------------------\n");
  ierr = NRPrintTypes(ksp->comm,stdout,ksp->prefix,"ksp_type",__KSPList);CHKERRQ(ierr);
  PetscPrintf(ksp->comm," %sksp_rtol <tol>: relative tolerance, defaults to %g\n",
                   p,ksp->rtol);
  PetscPrintf(ksp->comm," %sksp_atol <tol>: absolute tolerance, defaults to %g\n",
                   p,ksp->atol);
  PetscPrintf(ksp->comm," %sksp_divtol <tol>: divergence tolerance, defaults to %g\n",
                     p,ksp->divtol);
  PetscPrintf(ksp->comm," %sksp_max_it <maxit>: maximum iterations, defaults to %d\n",
                     p,ksp->max_it);
  PetscPrintf(ksp->comm," %sksp_preres: use preconditioned residual norm in convergence test\n",p);
  PetscPrintf(ksp->comm," %sksp_right_pc: use right preconditioner instead of left\n",p);
  PetscPrintf(ksp->comm," KSP Monitoring Options: Choose any of the following\n");
  PetscPrintf(ksp->comm,"   %sksp_cancelmonitors: cancel all monitors hardwired in code\n",p);
  PetscPrintf(ksp->comm,"   %sksp_monitor: at each iteration print (usually preconditioned) \n\
  residual norm to stdout\n",p);
  PetscPrintf(ksp->comm,"   %sksp_smonitor: same as the above, but prints fewer digits of the\n\
    residual norm for small residual norms. This is useful to conceal\n\
    meaningless digits that may be different on different machines.\n",p);
  PetscPrintf(ksp->comm,"   %sksp_xmonitor [x,y,w,h]: use X graphics monitor of (usually \n\
    preconditioned) residual norm\n",p);
  PetscPrintf(ksp->comm,"   %sksp_truemonitor: at each iteration print true and preconditioned\n",p);
  PetscPrintf(ksp->comm,"                      residual norms to stdout\n");
  PetscPrintf(ksp->comm,"   %sksp_xtruemonitor [x,y,w,h]: use X graphics monitor of true\n",p);
  PetscPrintf(ksp->comm,"                                 residual norm\n");
  PetscPrintf(ksp->comm,"   %sksp_singmonitor: calculate singular values during linear solve\n",p);
  PetscPrintf(ksp->comm,"       (only for CG and GMRES)\n");
  PetscPrintf(ksp->comm,"   %sksp_bsmonitor: at each iteration print the unscaled and \n",p);
  PetscPrintf(ksp->comm,"       (only for ICC and ILU in BlockSolve95)\n");
  PetscPrintf(ksp->comm,"   %sksp_plot_eigenvalues_explicitly\n",p);
  PetscPrintf(ksp->comm,"   %sksp_plot_eigenvalues\n",p);
  PetscPrintf(ksp->comm," GMRES Options:\n");
  PetscPrintf(ksp->comm,"   %sksp_gmres_restart <num>: GMRES restart, defaults to 30\n",p);
  PetscPrintf(ksp->comm,"   %sksp_gmres_unmodifiedgramschmidt: use alternative orthogonalization\n",p);
  PetscPrintf(ksp->comm,"   %sksp_gmres_irorthog: use iterative refinement in orthogonalization\n",p);
  PetscPrintf(ksp->comm,"   %sksp_gmres_preallocate: preallocate GMRES work vectors\n",p);
#if defined(USE_PETSC_COMPLEX)
  PetscPrintf(ksp->comm," CG Options:\n");
  PetscPrintf(ksp->comm,"   %sksp_cg_Hermitian: use CG for complex, Hermitian matrix (default)\n",p);
  PetscPrintf(ksp->comm,"   %sksp_cg_symmetric: use CG for complex, symmetric matrix\n",p);
#endif
  PetscFunctionReturn(1);
}

extern int KSPMonitor_MPIRowbs(KSP,int,double,void *);

#define MAXSETFROMOPTIONS 5
extern int numberofsetfromoptions;
extern int (*othersetfromoptions[MAXSETFROMOPTIONS])(KSP);

#undef __FUNC__  
#define __FUNC__ "KSPSetFromOptions"
/*@
   KSPSetFromOptions - Sets KSP options from the options database.
   This routine must be called before KSPSetUp() if the user is to be 
   allowed to set the Krylov type. 

   Input Parameters:
.  ksp - the Krylov space context

   Notes:  To see all options, run your program with the -help option;
           or consult the users manual.

.keywords: KSP, set, from, options, database

.seealso: KSPPrintHelp()
@*/
int KSPSetFromOptions(KSP ksp)
{
  KSPType   method;
  int       restart, flg, ierr,loc[4], nmax = 4,i;
  double    tmp;

  PetscFunctionBegin;
  loc[0] = PETSC_DECIDE; loc[1] = PETSC_DECIDE; loc[2] = 300; loc[3] = 300;

  PetscValidHeaderSpecific(ksp,KSP_COOKIE);
  ierr = OptionsHasName(PETSC_NULL,"-help", &flg); CHKERRQ(ierr);
  if (flg) { ierr = KSPPrintHelp(ksp); CHKERRQ(ierr);  }

  if (!__KSPList) {ierr = KSPRegisterAll();CHKERRQ(ierr);}
  ierr = NRGetTypeFromOptions(ksp->prefix,"-ksp_type",__KSPList,&method,&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = KSPSetType(ksp,method); CHKERRQ(ierr);
  }
  ierr = OptionsGetInt(ksp->prefix,"-ksp_max_it",&ksp->max_it, &flg); CHKERRQ(ierr);
  ierr = OptionsGetDouble(ksp->prefix,"-ksp_rtol",&ksp->rtol, &flg); CHKERRQ(ierr);
  ierr = OptionsGetDouble(ksp->prefix,"-ksp_atol",&ksp->atol, &flg); CHKERRQ(ierr);
  ierr = OptionsGetDouble(ksp->prefix,"-ksp_divtol",&ksp->divtol, &flg); CHKERRQ(ierr);
  ierr = OptionsHasName(ksp->prefix,"-ksp_gmres_preallocate", &flg); CHKERRQ(ierr);
  if (flg) {
    ierr = KSPGMRESSetPreAllocateVectors(ksp); CHKERRQ(ierr);
  }
  /* -----------------------------------------------------------------------*/
  /*
     Cancels all monitors hardwired into code before call to KSPSetFromOptions()
  */
  ierr = OptionsHasName(ksp->prefix,"-ksp_cancelmonitors",&flg); CHKERRQ(ierr);
  if (flg) {
    KSPSetMonitor(ksp,0,(void *)0);
  }
  /*
     Prints preconditioned residual norm at each iteration
  */
  ierr = OptionsHasName(ksp->prefix,"-ksp_monitor",&flg); CHKERRQ(ierr);
  if (flg) {
    int rank = 0;
    MPI_Comm_rank(ksp->comm,&rank);
    if (!rank) {
      KSPSetMonitor(ksp,KSPDefaultMonitor,(void *)0);
    }
  }
  /*
     Prints preconditioned and true residual norm at each iteration
  */
  ierr = OptionsHasName(ksp->prefix,"-ksp_truemonitor",&flg); CHKERRQ(ierr);
  if (flg) {
    KSPSetMonitor(ksp,KSPTrueMonitor,(void *)0); 
  }
  /*
     Prints extreme eigenvalue estimates at each iteration
  */
  ierr = OptionsHasName(ksp->prefix,"-ksp_singmonitor",&flg); CHKERRQ(ierr);
  if (flg) {
    ierr = KSPSetComputeSingularValues(ksp); CHKERRQ(ierr);
    KSPSetMonitor(ksp,KSPSingularValueMonitor,(void *)0); 
  }
  /*
     Prints true residual for BlockSolve95 preconditioners
  */
#if defined(HAVE_BLOCKSOLVE) && !defined(__cplusplus)
  ierr = OptionsHasName(ksp->prefix,"-ksp_bsmonitor",&flg); CHKERRQ(ierr);
  if (flg) {
    ierr = KSPSetMonitor(ksp,KSPMonitor_MPIRowbs,(void *)0);CHKERRQ(ierr);
  }
#endif
  /*
     Prints preconditioned residual norm with fewer digits
  */
  ierr = OptionsHasName(ksp->prefix,"-ksp_smonitor",&flg); CHKERRQ(ierr); 
  if (flg) {
    int rank = 0;
    MPI_Comm_rank(ksp->comm,&rank);
    if (!rank) {
      ierr = KSPSetMonitor(ksp,KSPDefaultSMonitor,(void *)0);CHKERRQ(ierr);
    }
  }
  /*
     Graphically plots preconditioned residual norm
  */
  nmax = 4;
  ierr = OptionsGetIntArray(ksp->prefix,"-ksp_xmonitor",loc,&nmax,&flg); CHKERRQ(ierr);
  if (flg) {
    int    rank = 0;
    DrawLG lg;
    MPI_Comm_rank(ksp->comm,&rank);
    if (!rank) {
      ierr = KSPLGMonitorCreate(0,0,loc[0],loc[1],loc[2],loc[3],&lg); CHKERRQ(ierr);
      PLogObjectParent(ksp,(PetscObject) lg);
      ierr = KSPSetMonitor(ksp,KSPLGMonitor,(void *)lg);CHKERRQ(ierr);
      ksp->xmonitor = lg; 
    }
  }
  /*
     Graphically plots preconditioned and true residual norm
  */
  nmax = 4;
  ierr = OptionsGetIntArray(ksp->prefix,"-ksp_xtruemonitor",loc,&nmax,&flg);CHKERRQ(ierr);
  if (flg){
    int    rank = 0;
    DrawLG lg;
    MPI_Comm_rank(ksp->comm,&rank);
    if (!rank) {
      ierr = KSPLGTrueMonitorCreate(ksp->comm,0,0,loc[0],loc[1],loc[2],loc[3],&lg);CHKERRQ(ierr);
      PLogObjectParent(ksp,(PetscObject) lg);
      ierr = KSPSetMonitor(ksp,KSPLGTrueMonitor,(void *)lg);CHKERRQ(ierr);
      ksp->xmonitor = lg; 
    } 
  }
  /* -----------------------------------------------------------------------*/
  ierr = OptionsHasName(ksp->prefix,"-ksp_preres",&flg); CHKERRQ(ierr);
  if (flg) { ierr = KSPSetUsePreconditionedResidual(ksp); CHKERRQ(ierr);}
  ierr = OptionsHasName(ksp->prefix,"-ksp_left_pc",&flg); CHKERRQ(ierr);
  if (flg) { ierr = KSPSetPreconditionerSide(ksp,PC_LEFT);CHKERRQ(ierr); }
  ierr = OptionsHasName(ksp->prefix,"-ksp_right_pc",&flg); CHKERRQ(ierr);
  if (flg) { ierr = KSPSetPreconditionerSide(ksp,PC_RIGHT); CHKERRQ(ierr);}
  ierr = OptionsHasName(ksp->prefix,"-ksp_symmetric_pc",&flg); CHKERRQ(ierr);
  if (flg) { ierr = KSPSetPreconditionerSide(ksp,PC_SYMMETRIC); CHKERRQ(ierr);}

  ierr = OptionsGetInt(ksp->prefix,"-ksp_gmres_restart",&restart,&flg); CHKERRQ(ierr);
  if (flg) { ierr = KSPGMRESSetRestart(ksp,restart);CHKERRQ(ierr); }

  ierr = OptionsHasName(ksp->prefix,"-ksp_gmres_unmodifiedgramschmidt",&flg);CHKERRQ(ierr);
  if (flg) { ierr = KSPGMRESSetOrthogonalization(ksp,KSPGMRESUnmodifiedGramSchmidtOrthogonalization);CHKERRQ(ierr); }
  ierr = OptionsHasName(ksp->prefix,"-ksp_gmres_irorthog",&flg);CHKERRQ(ierr);
  if (flg) { ierr = KSPGMRESSetOrthogonalization(ksp, KSPGMRESIROrthogonalization);CHKERRQ(ierr);}
  ierr = OptionsHasName(ksp->prefix,"-ksp_gmres_dgksorthog",&flg);CHKERRQ(ierr);
  if (flg) { ierr = KSPGMRESSetOrthogonalization(ksp, KSPGMRESDGKSOrthogonalization);CHKERRQ(ierr);}

  ierr = OptionsHasName(ksp->prefix,"-ksp_compute_singularvalues",&flg); CHKERRQ(ierr);
  if (flg) { ierr = KSPSetComputeSingularValues(ksp);CHKERRQ(ierr); }
  ierr = OptionsHasName(ksp->prefix,"-ksp_compute_eigenvalues",&flg);CHKERRQ(ierr);
  if (flg) { ierr = KSPSetComputeSingularValues(ksp);CHKERRQ(ierr); }
  ierr = OptionsHasName(ksp->prefix,"-ksp_plot_eigenvalues",&flg);CHKERRQ(ierr);
  if (flg) { ierr = KSPSetComputeSingularValues(ksp);CHKERRQ(ierr); }

  ierr = OptionsHasName(ksp->prefix,"-ksp_cg_Hermitian",&flg);CHKERRQ(ierr);
  if (flg) { ierr = KSPCGSetType(ksp,KSP_CG_HERMITIAN);CHKERRQ(ierr); }
  ierr = OptionsHasName(ksp->prefix,"-ksp_cg_symmetric",&flg);CHKERRQ(ierr);
  if (flg) { ierr = KSPCGSetType(ksp,KSP_CG_SYMMETRIC);CHKERRQ(ierr); }

  ierr = OptionsGetDouble(ksp->prefix,"-ksp_richardson_scale",&tmp,&flg);CHKERRQ(ierr);
  if (flg) { ierr = KSPRichardsonSetScale(ksp,tmp); CHKERRQ(ierr); }

  for ( i=0; i<numberofsetfromoptions; i++ ) {
    ierr = (*othersetfromoptions[i])(ksp); CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}
