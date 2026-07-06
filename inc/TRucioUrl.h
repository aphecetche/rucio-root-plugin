#ifndef ROOT_TRucioUrl
#define ROOT_TRucioUrl

#include <map>
#include <string>
#include <vector>

class TRucioUrl {
 public:
  explicit TRucioUrl(std::string url);

  const std::string& Original() const { return fOriginal; }
  const std::string& Scope() const { return fScope; }
  const std::string& Name() const { return fName; }
  const std::string& Anchor() const { return fAnchor; }
  const std::map<std::string, std::vector<std::string>>& Query() const {
    return fQuery;
  }

  bool HasQueryValue(const std::string& key) const;
  std::string QueryValue(const std::string& key,
                         const std::string& fallback = "") const;
  std::vector<std::string> QueryValues(const std::string& key) const;

 private:
  std::string fOriginal;
  std::string fScope;
  std::string fName;
  std::string fAnchor;
  std::map<std::string, std::vector<std::string>> fQuery;

  static std::string UrlDecode(const std::string& value);
  static std::vector<std::string> SplitCommaList(const std::string& value);
};

#endif
