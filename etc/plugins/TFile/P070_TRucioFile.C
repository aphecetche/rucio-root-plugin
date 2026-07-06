void P070_TRucioFile() {
  // ROOT reads this macro from Root.PluginPath before libRucioROOT is loaded.
  // The handler is what makes plain TFile::Open("rucio:///scope:name") work.
  gPluginMgr->AddHandler("TFile", "^rucio:?", "TRucioFile", "RucioROOT",
                         "TRucioFile(const char*,Option_t*,const char*,Int_t)");
}
