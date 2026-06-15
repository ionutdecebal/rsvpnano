#pragma once
// Minimal Arduino shim for host-side unit tests.
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

class String {
  std::string s_;

 public:
  String() = default;
  String(const char *cs) : s_(cs ? cs : "") {}
  String(char c) : s_(1, c) {}
  String(const std::string &s) : s_(s) {}
  String(int value) : s_(std::to_string(value)) {}
  String(unsigned int value) : s_(std::to_string(value)) {}
  String(long value) : s_(std::to_string(value)) {}
  String(unsigned long value) : s_(std::to_string(value)) {}
  String(const String &) = default;
  String(String &&) = default;
  String &operator=(const String &) = default;
  String &operator=(String &&) = default;
  String &operator=(const char *cs) {
    s_ = cs ? cs : "";
    return *this;
  }

  size_t length() const { return s_.size(); }
  bool isEmpty() const { return s_.empty(); }

  char operator[](size_t i) const { return s_[i]; }
  char &operator[](size_t i) { return s_[i]; }

  String &operator+=(char c) {
    s_ += c;
    return *this;
  }
  String &operator+=(const char *cs) {
    if (cs) s_ += cs;
    return *this;
  }
  String &operator+=(const String &o) {
    s_ += o.s_;
    return *this;
  }

  friend String operator+(String lhs, const String &rhs) {
    lhs.s_ += rhs.s_;
    return lhs;
  }
  friend String operator+(String lhs, const char *rhs) {
    if (rhs) lhs.s_ += rhs;
    return lhs;
  }
  friend String operator+(String lhs, char rhs) {
    lhs.s_ += rhs;
    return lhs;
  }
  friend String operator+(const char *lhs, const String &rhs) {
    String out(lhs);
    out.s_ += rhs.s_;
    return out;
  }

  bool operator==(const char *cs) const { return s_ == (cs ? cs : ""); }
  bool operator==(const String &o) const { return s_ == o.s_; }
  bool operator!=(const char *cs) const { return !(*this == cs); }
  bool operator!=(const String &o) const { return !(*this == o); }

  void reserve(size_t n) { s_.reserve(n); }

  bool startsWith(const char *prefix) const {
    if (!prefix) return false;
    const size_t pl = std::strlen(prefix);
    if (pl > s_.size()) return false;
    return s_.compare(0, pl, prefix) == 0;
  }
  bool startsWith(const String &prefix) const { return startsWith(prefix.s_.c_str()); }

  bool endsWith(const char *suffix) const {
    if (!suffix) return false;
    const size_t sl = std::strlen(suffix);
    if (sl > s_.size()) return false;
    return s_.compare(s_.size() - sl, sl, suffix) == 0;
  }
  bool endsWith(const String &suffix) const { return endsWith(suffix.s_.c_str()); }

  int indexOf(char c, unsigned int from = 0) const {
    const size_t pos = s_.find(c, from);
    return pos == std::string::npos ? -1 : static_cast<int>(pos);
  }
  int indexOf(const char *needle, unsigned int from = 0) const {
    if (!needle) return -1;
    const size_t pos = s_.find(needle, from);
    return pos == std::string::npos ? -1 : static_cast<int>(pos);
  }
  int indexOf(const String &needle, unsigned int from = 0) const {
    return indexOf(needle.s_.c_str(), from);
  }
  int lastIndexOf(char c) const {
    const size_t pos = s_.rfind(c);
    return pos == std::string::npos ? -1 : static_cast<int>(pos);
  }

  String substring(unsigned int beginIndex) const {
    if (beginIndex >= s_.size()) return String();
    return String(s_.substr(beginIndex));
  }
  String substring(unsigned int beginIndex, unsigned int endIndex) const {
    if (beginIndex > endIndex) std::swap(beginIndex, endIndex);
    if (beginIndex >= s_.size()) return String();
    endIndex = std::min(endIndex, static_cast<unsigned int>(s_.size()));
    return String(s_.substr(beginIndex, endIndex - beginIndex));
  }

  void replace(const char *find, const char *repl) {
    if (!find || !*find) return;
    const std::string f(find);
    const std::string r(repl ? repl : "");
    size_t pos = 0;
    while ((pos = s_.find(f, pos)) != std::string::npos) {
      s_.replace(pos, f.size(), r);
      pos += r.size();
    }
  }
  void replace(const String &find, const String &repl) {
    replace(find.s_.c_str(), repl.s_.c_str());
  }

  void remove(unsigned int index, unsigned int count) {
    if (index >= s_.size()) return;
    s_.erase(index, count);
  }
  void remove(unsigned int index) {
    if (index >= s_.size()) return;
    s_.erase(index);
  }

  void trim() {
    size_t begin = 0;
    while (begin < s_.size() && std::isspace(static_cast<unsigned char>(s_[begin]))) ++begin;
    size_t end = s_.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(s_[end - 1]))) --end;
    s_ = s_.substr(begin, end - begin);
  }

  void toLowerCase() {
    for (char &c : s_) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  void toUpperCase() {
    for (char &c : s_) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }

  const char *c_str() const { return s_.c_str(); }
};
