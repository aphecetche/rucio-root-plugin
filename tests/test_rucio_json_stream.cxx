#include <cassert>
#include <iostream>
#include <string>

#include "TRucioResolver.h"

int main() {
  const std::string body =
      "{\"scope\":\"scope\",\"name\":\"name\",\"pfns\":{\"root://host//"
      "a.root\":{\"rse\":\"RSE_A\",\"domain\":\"wan\",\"priority\":1},\"davs://"
      "host/a.root\":{\"rse\":\"RSE_B\",\"domain\":\"wan\",\"priority\":2}}}\n";

  const auto replicas = TRucioResolver::ParseJsonStream(body);
  assert(replicas.size() == 2);

  bool foundRoot = false;
  bool foundDavs = false;
  for (const auto& replica : replicas) {
    if (replica.pfn == "root://host//a.root") {
      foundRoot = true;
      assert(replica.scheme == "root");
      assert(replica.rse == "RSE_A");
      assert(replica.priority == 1);
    }
    if (replica.pfn == "davs://host/a.root") {
      foundDavs = true;
      assert(replica.scheme == "davs");
      assert(replica.rse == "RSE_B");
    }
  }
  assert(foundRoot);
  assert(foundDavs);

  std::cout << "TRucioResolver JSON-stream tests passed\n";
  return 0;
}
