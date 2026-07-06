#ifndef ROOT_TRucioResolver
#define ROOT_TRucioResolver

#include <string>
#include <vector>

#include "TRucioUrl.h"

struct RucioReplica {
  std::string pfn;
  std::string rse;
  std::string scheme;
  std::string domain;
  int priority = 0;
  bool available = true;
};

struct RucioResolverOptions {
  std::string rucioHost;
  std::string account;
  std::string x509Proxy;
  std::string caPath;
  std::string userAgent = "rucio-root-plugin/0.1";
  std::vector<std::string> schemes;
  std::string rseExpression;
  std::string domain = "wan";
  std::string sort;
  int limit = 0;
  long timeoutSeconds = 30;
  bool ignoreAvailability = false;
};

class TRucioResolver {
 public:
  explicit TRucioResolver(RucioResolverOptions options);

  std::vector<RucioReplica> Resolve(const std::string& scope,
                                    const std::string& name) const;

  static RucioResolverOptions OptionsFromEnvironment();
  static void ApplyUrlOptions(RucioResolverOptions& options,
                              const TRucioUrl& url);
  static std::vector<RucioReplica> ParseJsonStream(const std::string& body);

 private:
  RucioResolverOptions fOptions;

  // The Rucio REST token is obtained lazily from /auth/x509 and reused for the
  // lifetime of this resolver instance. Users should not need to export it.
  mutable std::string fAuthToken;

  std::string GetAuthToken() const;
  std::string BuildListReplicasBody(const std::string& scope,
                                    const std::string& name) const;
  std::string BuildListReplicasUrl() const;
};

#endif
