#pragma once
// Minimal Arduino shim for host-side unit tests.
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

class String {
  std::string s_;

 public:
  String() = default;
  String(const char *cs) : s_(cs ? cs : "") {}
  String(char c) : s_(1, c) {}
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
  String &operator+=(const String &o) {
    s_ += o.s_;
    return *this;
  }

  bool operator==(const char *cs) const { return s_ == (cs ? cs : ""); }
  bool operator==(const String &o) const { return s_ == o.s_; }
  bool operator!=(const char *cs) const { return !(*this == cs); }
  bool operator!=(const String &o) const { return !(*this == o); }

  void reserve(size_t n) { s_.reserve(n); }

  bool endsWith(const char *suffix) const {
    if (!suffix) return false;
    const size_t sl = std::strlen(suffix);
    if (sl > s_.size()) return false;
    return s_.compare(s_.size() - sl, sl, suffix) == 0;
  }
  bool endsWith(const String &suffix) const { return endsWith(suffix.s_.c_str()); }

  void toLowerCase() {
    for (char &c : s_) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  const char *c_str() const { return s_.c_str(); }
};
