
static char help[] = 
"This example solves a tridiagonal linear system with SLES.  Input\n\
arguments are:\n\
  -n <size> : problem size\n\n";

#include "sles.h"
#include <stdio.h>
#include "petsc.h"

int main(int argc,char **args)
{
  int    ierr,i,n = 10, col[3], its;
  Scalar none = -1.0, one = 1.0, value[3];
  Vec    x,b,u;
  Mat    A;
  SLES   sles;
  double norm;

  PetscInitialize(&argc,&args,0,0);
  if (OptionsHasName(0,"-help")) fprintf(stderr,"%s",help);
  OptionsGetInt(0,"-n",&n);

  /* Create vectors */
  ierr = VecCreate(MPI_COMM_WORLD,n,&x); CHKERRA(ierr);
  ierr = VecDuplicate(x,&b); CHKERRA(ierr);
  ierr = VecDuplicate(x,&u); CHKERRA(ierr);
  ierr = VecSet(&one,u); CHKERRA(ierr);

  /* Assemble matrix */
  ierr = MatCreate(MPI_COMM_WORLD,n,n,&A); CHKERRA(ierr);
  value[0] = -1.0; value[1] = 2.0; value[2] = -1.0;
  for (i=1; i<n-1; i++ ) {
    col[0] = i-1; col[1] = i; col[2] = i+1;
    ierr = MatSetValues(A,1,&i,3,col,value,INSERTVALUES); CHKERRA(ierr);
  }
  i = n - 1; col[0] = n - 2; col[1] = n - 1;
  ierr = MatSetValues(A,1,&i,2,col,value,INSERTVALUES); CHKERRA(ierr);
  i = 0; col[0] = 0; col[1] = 1; value[0] = 2.0; value[1] = -1.0;
  ierr = MatSetValues(A,1,&i,2,col,value,INSERTVALUES); CHKERRA(ierr);
  ierr = MatAssemblyBegin(A,FINAL_ASSEMBLY); CHKERRA(ierr);
  ierr = MatAssemblyEnd(A,FINAL_ASSEMBLY); CHKERRA(ierr);

  /* Compute the right-hand-side */
  ierr = MatMult(A,u,b); CHKERRA(ierr);

  /* Solve linear system, Ax = b */
  ierr = SLESCreate(MPI_COMM_WORLD,&sles); CHKERRA(ierr);
  ierr = SLESSetOperators(sles,A,A,0); CHKERRA(ierr);
  ierr = SLESSetFromOptions(sles); CHKERRA(ierr);
  ierr = SLESSolve(sles,b,x,&its); CHKERRA(ierr);

  /* Check error */
  ierr = VecAXPY(&none,u,x); CHKERRA(ierr);
  ierr  = VecNorm(x,&norm); CHKERRA(ierr);
  MPE_printf(MPI_COMM_WORLD,"Norm of error %g Iterations %d\n",norm,its);
 
  /* Destroy data structures */
  ierr = VecDestroy(x); CHKERRA(ierr);
  ierr = VecDestroy(u); CHKERRA(ierr);
  ierr = VecDestroy(b); CHKERRA(ierr);
  ierr = MatDestroy(A); CHKERRA(ierr);
  ierr = SLESDestroy(sles); CHKERRA(ierr);
  PetscFinalize();
  return 0;
}
    


