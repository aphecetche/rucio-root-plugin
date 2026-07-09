#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

#include "TRucioUrl.h"

int main() {
  {
    TRucioUrl url(
        "rucio:///scope:name?scheme=root,davs&rse=RSE_A&limit=2#inner.root");
    assert(url.Scope() == "scope");
    assert(url.Name() == "name");
    assert(url.Anchor() == "inner.root");
    assert(url.QueryValues("scheme").size() == 2);
    assert(url.QueryValue("rse") == "RSE_A");
    assert(url.QueryValue("limit") == "2");
  }

  {
    bool rejected = false;
    try {
      TRucioUrl url("rucio://scope:name?scheme=root");
    } catch (const std::invalid_argument& error) {
      rejected = true;
      assert(std::string(error.what()).find("third slash") !=
             std::string::npos);
    }
    assert(rejected);
  }

  {
    bool rejected = false;
    try {
      TRucioUrl url("rucio://scope/path/to/file.root");
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }

  std::cout << "TRucioUrl tests passed\n";
  return 0;
}
