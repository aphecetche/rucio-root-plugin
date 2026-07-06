#ifndef ROOT_TRucioFile
#define ROOT_TRucioFile

#include "Rtypes.h"
#include "TNetXNGFile.h"

// ROOT instantiates TFile plugins through a constructor registered by the
// plugin manager. Inheriting from TNetXNGFile lets the resolved root:// PFN use
// ROOT's existing XRootD transport instead of trying to implement file I/O
// here.
class TRucioFile : public TNetXNGFile {
 public:
  TRucioFile(const char* url, Option_t* option = "", const char* title = "",
             Int_t compress = 1);

  ClassDefOverride(TRucioFile, 0)
};

#endif
