#include "TRucioResolver.h"

#include <curl/curl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>

using json = nlohmann::json;

namespace {
std::string GetEnv(const char* name) {
  const char* value = std::getenv(name);
  return value ? std::string(value) : std::string();
}

bool ParseBool(const std::string& value) {
  return value == "1" || value == "true" || value == "TRUE" || value == "yes" ||
         value == "YES";
}

std::string Trim(const std::string& value) {
  const auto first =
      std::find_if_not(value.begin(), value.end(),
                       [](unsigned char c) { return std::isspace(c); });
  const auto last =
      std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
        return std::isspace(c);
      }).base();
  return first < last ? std::string(first, last) : std::string();
}

std::string ToLower(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::string EnsureNoTrailingSlash(std::string value) {
  while (!value.empty() && value.back() == '/') {
    value.pop_back();
  }
  return value;
}

std::string SchemeFromPfn(const std::string& pfn) {
  const auto pos = pfn.find("://");
  if (pos == std::string::npos) {
    return {};
  }
  return pfn.substr(0, pos);
}

std::size_t WriteCallback(char* ptr, std::size_t size, std::size_t nmemb,
                          void* userdata) {
  auto* body = static_cast<std::string*>(userdata);
  body->append(ptr, size * nmemb);
  return size * nmemb;
}

std::size_t HeaderCallback(char* ptr, std::size_t size, std::size_t nmemb,
                           void* userdata) {
  const std::size_t total = size * nmemb;
  auto* token = static_cast<std::string*>(userdata);
  const std::string header(ptr, total);
  const auto colon = header.find(':');
  if (colon == std::string::npos) {
    return total;
  }

  if (ToLower(Trim(header.substr(0, colon))) == "x-rucio-auth-token") {
    *token = Trim(header.substr(colon + 1));
  }
  return total;
}

bool IsDirectory(const std::string& path) {
  struct stat info{};
  return !path.empty() && stat(path.c_str(), &info) == 0 &&
         S_ISDIR(info.st_mode);
}

// libcurl uses different options for a CA bundle file and an OpenSSL-style CA
// directory. Treating both cases explicitly avoids the earlier CAINFO/CAPATH
// ambiguity when X509_CERT_DIR points at a directory.
void ApplyTlsOptions(CURL* curl, const RucioResolverOptions& options) {
  if (!options.x509Proxy.empty()) {
    curl_easy_setopt(curl, CURLOPT_SSLCERT, options.x509Proxy.c_str());
    curl_easy_setopt(curl, CURLOPT_SSLKEY, options.x509Proxy.c_str());
  }
  if (!options.caPath.empty()) {
    if (IsDirectory(options.caPath)) {
      curl_easy_setopt(curl, CURLOPT_CAPATH, options.caPath.c_str());
    } else {
      curl_easy_setopt(curl, CURLOPT_CAINFO, options.caPath.c_str());
    }
  }
}

// Only the [client] keys needed by the plugin are parsed. This keeps the plugin
// independent from the Python Rucio client while still honoring normal user
// setup via RUCIO_CONFIG.
std::map<std::string, std::string> ReadRucioClientConfig(
    const std::string& path) {
  std::map<std::string, std::string> values;
  if (path.empty()) {
    return values;
  }

  std::ifstream input(path);
  if (!input) {
    return values;
  }

  bool inClientSection = false;
  std::string line;
  while (std::getline(input, line)) {
    line = Trim(line);
    if (line.empty() || line.front() == '#' || line.front() == ';') {
      continue;
    }
    if (line.front() == '[' && line.back() == ']') {
      inClientSection =
          ToLower(Trim(line.substr(1, line.size() - 2))) == "client";
      continue;
    }
    if (!inClientSection) {
      continue;
    }

    const auto equals = line.find('=');
    if (equals == std::string::npos) {
      continue;
    }
    values[ToLower(Trim(line.substr(0, equals)))] =
        Trim(line.substr(equals + 1));
  }
  return values;
}
}  // namespace

TRucioResolver::TRucioResolver(RucioResolverOptions options)
    : fOptions(std::move(options)) {
  fOptions.rucioHost = EnsureNoTrailingSlash(fOptions.rucioHost);
}

std::vector<RucioReplica> TRucioResolver::Resolve(
    const std::string& scope, const std::string& name) const {
  if (fOptions.rucioHost.empty()) {
    throw std::runtime_error(
        "Rucio host is not set; set RUCIO_HOST or configure rucio_host in "
        "RUCIO_CONFIG");
  }

  CURL* curl = curl_easy_init();
  if (!curl) {
    throw std::runtime_error("Could not initialize libcurl");
  }

  std::string responseBody;
  std::string requestBody = BuildListReplicasBody(scope, name);
  const std::string requestUrl = BuildListReplicasUrl();
  const std::string authToken = GetAuthToken();

  // /replicas/list expects the short-lived REST token. Do not send
  // X-Rucio-Account here; on the KM3NeT deployment the token already encodes
  // the account and an extra account header can make authentication fail.
  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Accept: application/x-json-stream");
  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers =
      curl_slist_append(headers, ("X-Rucio-Auth-Token: " + authToken).c_str());

  curl_easy_setopt(curl, CURLOPT_URL, requestUrl.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                   static_cast<long>(requestBody.size()));
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, fOptions.userAgent.c_str());
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, fOptions.timeoutSeconds);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  ApplyTlsOptions(curl, fOptions);

  const CURLcode code = curl_easy_perform(curl);
  long httpStatus = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (code != CURLE_OK) {
    throw std::runtime_error(std::string("Rucio REST request failed: ") +
                             curl_easy_strerror(code));
  }
  if (httpStatus < 200 || httpStatus >= 300) {
    std::ostringstream error;
    error << "Rucio REST request failed with HTTP " << httpStatus << ": "
          << responseBody;
    throw std::runtime_error(error.str());
  }

  return ParseJsonStream(responseBody);
}

RucioResolverOptions TRucioResolver::OptionsFromEnvironment() {
  RucioResolverOptions options;
  const auto config = ReadRucioClientConfig(GetEnv("RUCIO_CONFIG"));
  options.rucioHost = GetEnv("RUCIO_HOST");
  if (options.rucioHost.empty() && config.count("rucio_host")) {
    options.rucioHost = config.at("rucio_host");
  }
  options.account = GetEnv("RUCIO_ACCOUNT");
  if (options.account.empty() && config.count("account")) {
    options.account = config.at("account");
  }
  options.x509Proxy = GetEnv("X509_USER_PROXY");
  if (options.x509Proxy.empty()) {
    options.x509Proxy = "/tmp/x509up_u" + std::to_string(getuid());
  }
  options.caPath = GetEnv("RUCIO_CA_PATH");
  if (options.caPath.empty()) {
    options.caPath = GetEnv("X509_CERT_DIR");
  }
  return options;
}

void TRucioResolver::ApplyUrlOptions(RucioResolverOptions& options,
                                     const TRucioUrl& url) {
  auto schemes = url.QueryValues("scheme");
  const auto schemesAlias = url.QueryValues("schemes");
  schemes.insert(schemes.end(), schemesAlias.begin(), schemesAlias.end());
  if (!schemes.empty()) {
    options.schemes = schemes;
  }
  options.rseExpression = url.QueryValue(
      "rse", url.QueryValue("rse_expression", options.rseExpression));
  options.domain = url.QueryValue("domain", options.domain);
  options.sort = url.QueryValue("sort", options.sort);
  if (url.HasQueryValue("limit")) {
    options.limit = std::stoi(url.QueryValue("limit"));
  }
  if (url.HasQueryValue("ignore_availability")) {
    options.ignoreAvailability =
        ParseBool(url.QueryValue("ignore_availability"));
  }
}

std::vector<RucioReplica> TRucioResolver::ParseJsonStream(
    const std::string& body) {
  std::vector<RucioReplica> replicas;
  std::stringstream lines(body);
  std::string line;

  // Rucio streams one JSON object per line. Each record may contain multiple
  // PFNs, keyed by PFN string, with RSE/domain metadata as values.
  while (std::getline(lines, line)) {
    if (line.empty()) {
      continue;
    }
    const auto record = json::parse(line);
    if (!record.contains("pfns") || !record["pfns"].is_object()) {
      continue;
    }

    for (const auto& item : record["pfns"].items()) {
      RucioReplica replica;
      replica.pfn = item.key();
      replica.scheme = SchemeFromPfn(replica.pfn);
      if (item.value().is_object()) {
        const auto& meta = item.value();
        if (meta.contains("rse") && meta["rse"].is_string()) {
          replica.rse = meta["rse"].get<std::string>();
        }
        if (meta.contains("domain") && meta["domain"].is_string()) {
          replica.domain = meta["domain"].get<std::string>();
        }
        if (meta.contains("priority") && meta["priority"].is_number_integer()) {
          replica.priority = meta["priority"].get<int>();
        }
        if (meta.contains("available") && meta["available"].is_boolean()) {
          replica.available = meta["available"].get<bool>();
        }
      }
      replicas.push_back(std::move(replica));
    }
  }
  return replicas;
}

std::string TRucioResolver::GetAuthToken() const {
  if (!fAuthToken.empty()) {
    return fAuthToken;
  }
  if (fOptions.rucioHost.empty()) {
    throw std::runtime_error(
        "Rucio host is not set; set RUCIO_HOST or configure rucio_host in "
        "RUCIO_CONFIG");
  }
  if (fOptions.account.empty()) {
    throw std::runtime_error(
        "Rucio account is not set; set RUCIO_ACCOUNT or configure account in "
        "RUCIO_CONFIG");
  }
  if (fOptions.x509Proxy.empty()) {
    throw std::runtime_error("X509_USER_PROXY is not set");
  }

  CURL* curl = curl_easy_init();
  if (!curl) {
    throw std::runtime_error("Could not initialize libcurl");
  }

  std::string responseBody;
  std::string token;
  const std::string authUrl = fOptions.rucioHost + "/auth/x509";

  // X509 authentication is the only place where the account header belongs.
  // The returned X-Rucio-Auth-Token is then used for catalogue requests.
  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers,
                              ("X-Rucio-Account: " + fOptions.account).c_str());

  curl_easy_setopt(curl, CURLOPT_URL, authUrl.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &token);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, fOptions.userAgent.c_str());
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, fOptions.timeoutSeconds);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  ApplyTlsOptions(curl, fOptions);

  const CURLcode code = curl_easy_perform(curl);
  long httpStatus = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (code != CURLE_OK) {
    throw std::runtime_error(std::string("Rucio X509 authentication failed: ") +
                             curl_easy_strerror(code));
  }
  if (httpStatus < 200 || httpStatus >= 300) {
    std::ostringstream error;
    error << "Rucio X509 authentication failed with HTTP " << httpStatus << ": "
          << responseBody;
    throw std::runtime_error(error.str());
  }
  if (token.empty()) {
    throw std::runtime_error(
        "Rucio X509 authentication did not return X-Rucio-Auth-Token");
  }

  fAuthToken = token;
  return fAuthToken;
}

std::string TRucioResolver::BuildListReplicasBody(
    const std::string& scope, const std::string& name) const {
  json body;
  body["dids"] = json::array({{{"scope", scope}, {"name", name}}});
  if (!fOptions.schemes.empty()) {
    body["schemes"] = fOptions.schemes;
  }
  if (!fOptions.rseExpression.empty()) {
    body["rse_expression"] = fOptions.rseExpression;
  }
  if (!fOptions.domain.empty()) {
    body["domain"] = fOptions.domain;
  }
  if (!fOptions.sort.empty()) {
    body["sort"] = fOptions.sort;
  }
  if (fOptions.ignoreAvailability) {
    body["ignore_availability"] = true;
  }
  return body.dump();
}

std::string TRucioResolver::BuildListReplicasUrl() const {
  std::string url = fOptions.rucioHost + "/replicas/list";
  if (fOptions.limit > 0) {
    url += "?limit=" + std::to_string(fOptions.limit);
  }
  return url;
}
