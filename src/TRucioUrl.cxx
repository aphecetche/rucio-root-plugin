#include "TRucioUrl.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

namespace {
std::string TrimSlashes(std::string value) {
  while (!value.empty() && value.front() == '/') {
    value.erase(value.begin());
  }
  return value;
}

std::string ToLower(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}
}  // namespace

TRucioUrl::TRucioUrl(std::string url) : fOriginal(std::move(url)) {
  constexpr const char* prefix = "rucio://";
  if (fOriginal.rfind(prefix, 0) != 0) {
    throw std::invalid_argument("Rucio URL must start with rucio://");
  }

  std::string rest = fOriginal.substr(std::char_traits<char>::length(prefix));

  const auto anchorPos = rest.find('#');
  if (anchorPos != std::string::npos) {
    fAnchor = UrlDecode(rest.substr(anchorPos + 1));
    rest = rest.substr(0, anchorPos);
  }

  std::string query;
  const auto queryPos = rest.find('?');
  if (queryPos != std::string::npos) {
    query = rest.substr(queryPos + 1);
    rest = rest.substr(0, queryPos);
  }

  rest = TrimSlashes(rest);
  const auto colonPos = rest.find(':');
  const auto slashPos = rest.find('/');
  // Support both DID spellings used by users and ROOT-safe plugin URLs:
  //   rucio:///scope:name
  //   rucio://scope/name
  // The standalone parser also accepts rucio://scope:name, but ROOT's TUrl
  // treats that form as authority syntax before this class sees it.
  if (colonPos != std::string::npos &&
      (slashPos == std::string::npos || colonPos < slashPos)) {
    fScope = UrlDecode(rest.substr(0, colonPos));
    fName = UrlDecode(rest.substr(colonPos + 1));
  } else if (slashPos != std::string::npos) {
    fScope = UrlDecode(rest.substr(0, slashPos));
    fName = UrlDecode(rest.substr(slashPos + 1));
  } else {
    throw std::invalid_argument(
        "Rucio URL must contain a DID as scope:name or scope/name");
  }

  if (fScope.empty() || fName.empty()) {
    throw std::invalid_argument("Rucio URL has an empty scope or name");
  }

  std::stringstream stream(query);
  std::string pair;
  while (std::getline(stream, pair, '&')) {
    if (pair.empty()) {
      continue;
    }
    const auto eqPos = pair.find('=');
    std::string key = UrlDecode(pair.substr(0, eqPos));
    std::string value =
        eqPos == std::string::npos ? "" : UrlDecode(pair.substr(eqPos + 1));
    key = ToLower(key);
    for (const auto& item : SplitCommaList(value)) {
      fQuery[key].push_back(item);
    }
    if (value.empty()) {
      fQuery[key].push_back("");
    }
  }
}

bool TRucioUrl::HasQueryValue(const std::string& key) const {
  return fQuery.find(ToLower(key)) != fQuery.end();
}

std::string TRucioUrl::QueryValue(const std::string& key,
                                  const std::string& fallback) const {
  const auto values = QueryValues(key);
  if (values.empty()) {
    return fallback;
  }
  return values.front();
}

std::vector<std::string> TRucioUrl::QueryValues(const std::string& key) const {
  const auto iter = fQuery.find(ToLower(key));
  if (iter == fQuery.end()) {
    return {};
  }
  return iter->second;
}

std::string TRucioUrl::UrlDecode(const std::string& value) {
  std::string decoded;
  decoded.reserve(value.size());
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '%' && i + 2 < value.size()) {
      const auto hex = value.substr(i + 1, 2);
      char* end = nullptr;
      const auto ch = static_cast<char>(std::strtol(hex.c_str(), &end, 16));
      if (end && *end == '\0') {
        decoded.push_back(ch);
        i += 2;
        continue;
      }
    }
    decoded.push_back(value[i] == '+' ? ' ' : value[i]);
  }
  return decoded;
}

std::vector<std::string> TRucioUrl::SplitCommaList(const std::string& value) {
  std::vector<std::string> values;
  std::stringstream stream(value);
  std::string item;
  while (std::getline(stream, item, ',')) {
    if (!item.empty()) {
      values.push_back(item);
    }
  }
  return values;
}
