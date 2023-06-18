#ifndef NJS_UTILS_HELPER_H
#define NJS_UTILS_HELPER_H

#include <math.h>
#include <string.h>

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <codecvt>
#include <locale>

#include "njs/utils/macros.h"

namespace njs {
namespace debug {

class Debugger {
 public:
  static Debugger* Instance() {
    static Debugger debug;
    return &debug;
  }

  static void Turn() { Debugger::Instance()->on_ = !Debugger::Instance()->on_; }
  static void TurnOff() { Debugger::Instance()->on_ = false; }
  static bool On() { return Debugger::Instance()->on_; }

 private:
#ifdef TEST
  bool on_ = true;
#else
  bool on_ = false;
#endif
};

// For exact debugging
class Tracker {
 public:
  static Tracker* Instance() {
    static Tracker tracker;
    return &tracker;
  }

  static void TurnOn() { Tracker::Instance()->on_ = true; }
  static bool On() { return Tracker::Instance()->on_; }

 private:
  bool on_ = false;
};

void PrintSource(std::string comment, std::u16string str = u"", std::string postfix = "") {
  std::cout << comment;
  for (const auto& c: str)
    std::cout << static_cast<char>(c);
  std::cout << postfix << "\n";
}

std::string to_utf8_string(std::u16string str) {
  static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
  return convert.to_bytes(str);
}

std::string to_utf8_string(const std::u16string_view& u16view) {
  std::u16string u16str(u16view);
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
  return converter.to_bytes(u16str);
}

std::string to_utf8_string(bool b) {
  return b ? "true" : "false";
}

std::string to_utf8_string(const void *ptr) {
  std::stringstream ss;
  ss << ptr;  
  return ss.str();
}

}  // namespace test

std::u16string StrCat(std::vector<std::u16string> vals) {
  u32 size = 0;
  for (auto val : vals) {
    size += val.size();
  }
  std::u16string res(size, 0);
  u32 offset = 0;
  for (auto val : vals) {
    memcpy((void*)(res.c_str() + offset), (void*)(val.data()), val.size() * 2);
    offset += val.size();
  }
  return res;
}

bool HaveDuplicate(std::vector<std::u16string> vals) {
  for (u32 i = 0; i < vals.size(); i++) {
    for (u32 j = 0; j < vals.size(); j++) {
      if (i != j && vals[i] == vals[j])
        return true;
    }
  }
  return false;
}

// From Knuth https://stackoverflow.com/a/253874/5163915
static constexpr double kEpsilon = 1e-15;
bool ApproximatelyEqual(double a, double b) {
  return fabs(a - b) <= ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * kEpsilon);
}

bool EssentiallyEqual(double a, double b) {
  return fabs(a - b) <= ((fabs(a) > fabs(b) ? fabs(b) : fabs(a)) * kEpsilon);
}

bool DefinitelyGreaterThan(double a, double b) {
  return (a - b) > ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * kEpsilon);
}

bool DefinitelyLessThan(double a, double b) {
  return (b - a) > ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * kEpsilon);
}

}  // namespace njs



#endif  // NJS_UTILS_HELPER_H