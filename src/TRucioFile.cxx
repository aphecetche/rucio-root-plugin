#include "TRucioFile.h"

#include <stdexcept>
#include <string>

#include "TError.h"
#include "TRucioResolver.h"
#include "TRucioUrl.h"
#include "TString.h"

namespace {
TString NormalizeMode(Option_t* option) {
  TString mode = option ? option : "";
  mode.ToUpper();
  if (mode.IsNull()) {
    mode = "READ";
  }
  return mode;
}

std::string ResolveFirstPfn(const char* url, Option_t* option) {
  const TString mode = NormalizeMode(option);
  if (mode != "READ") {
    throw std::runtime_error(
        std::string(
            "The Rucio ROOT plugin MVP is read-only; requested mode was ") +
        mode.Data());
  }

  TRucioUrl rucioUrl(url ? url : "");
  auto options = TRucioResolver::OptionsFromEnvironment();
  TRucioResolver::ApplyUrlOptions(options, rucioUrl);
  TRucioResolver resolver(options);

  const auto replicas = resolver.Resolve(rucioUrl.Scope(), rucioUrl.Name());
  if (replicas.empty()) {
    throw std::runtime_error("Rucio returned no PFNs for " + rucioUrl.Scope() +
                             ":" + rucioUrl.Name());
  }

  TString pfn = replicas.front().pfn.c_str();
  if (!rucioUrl.Anchor().empty()) {
    pfn += "#";
    pfn += rucioUrl.Anchor().c_str();
  }

  ::Info("TRucioFile", "Opening PFN %s%s%s", pfn.Data(),
         replicas.front().rse.empty() ? "" : " from RSE ",
         replicas.front().rse.empty() ? "" : replicas.front().rse.c_str());

  return pfn.Data();
}
}  // namespace

// Constructor used by ROOT's plugin manager. It resolves the DID before calling
// the TNetXNGFile base constructor, so ROOT only ever sees a normal root://
// PFN.
TRucioFile::TRucioFile(const char* url, Option_t* option, const char* title,
                       Int_t compress)
    : TNetXNGFile(ResolveFirstPfn(url, option).c_str(),
                  NormalizeMode(option).Data(), title, compress) {}
