/* $Id: petscbccfe.h,v 1.1 2001/03/06 23:57:40 buschelm Exp $ */
#ifndef PETScBccFE_h
#define PETScBccFE_h

#include <map>
#include "petscfe.h"

namespace PETScFE {
  class bcc : public compiler {
  public:
    bcc();
    ~bcc() {}
    virtual void GetArgs(int,char *[]);
    virtual void Parse(void);
  protected:
    virtual void Compile(void);
    virtual void Link(void);

    virtual void FoundD(int &,string);
    virtual void FoundI(int &,string);
    virtual void FoundL(int &,string);
    virtual void Foundl(int &,string);
    virtual void Foundo(int &,string);
    virtual void FixOutput(void);
    int OutputFlag;
  };

}

#endif
