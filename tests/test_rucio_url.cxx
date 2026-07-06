#include <cassert>
#include <iostream>

#include "TRucioUrl.h"

int main() {
  {
    TRucioUrl url(
        "rucio://scope:name?scheme=root,davs&rse=RSE_A&limit=2#inner.root");
    assert(url.Scope() == "scope");
    assert(url.Name() == "name");
    assert(url.Anchor() == "inner.root");
    assert(url.QueryValues("scheme").size() == 2);
    assert(url.QueryValue("rse") == "RSE_A");
    assert(url.QueryValue("limit") == "2");
  }

  {
    TRucioUrl url("rucio:///scope:name?scheme=root");
    assert(url.Scope() == "scope");
    assert(url.Name() == "name");
    assert(url.QueryValue("scheme") == "root");
  }

  {
    TRucioUrl url("rucio://scope/path/to/file.root?schemes=root&schemes=davs");
    assert(url.Scope() == "scope");
    assert(url.Name() == "path/to/file.root");
    assert(url.QueryValues("schemes").size() == 2);
  }

  std::cout << "TRucioUrl tests passed\n";
  return 0;
}
