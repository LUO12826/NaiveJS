#ifndef NJS_JSDATE_H
#define NJS_JSDATE_H

#include <chrono>
#include <cmath>
#include <string>
#include "JSObject.h"
#include "qjs_date.h"
#include "njs/vm/NjsVM.h"

namespace njs {

using namespace std::chrono;
using std::string;
using std::u16string;

enum class DateStringFormat {
  STR,            // "Wed Jan 03 2018 00:05:22 GMT+0100 (CET)"
  UTC_STR,        // "Tue, 02 Jan 2018 23:04:46 GMT"
  ISO_STR,        // "2018-01-02T23:02:56.927Z"
  LOCALE_STR,     // "1/2/2018, 11:40:40 PM"
};

enum class DateStringPart {
  DATE,
  TIME,
  ALL,
};

class JSDate : public JSObject {
 public:

  static double get_curr_millis() {
    auto duration = system_clock::now().time_since_epoch();
    auto millis = duration_cast<milliseconds>(duration).count();
    return time_clip(millis);
  }

  JSDate(NjsVM& vm) : JSObject(CLS_DATE, vm.date_prototype) {}

  u16string_view get_class_name() override {
    return u"Date";
  }

  void set_to_now() {
    timestamp = get_curr_millis();
  }

  void parse_date_str(const String& str) {
    // TODO
    timestamp = 0;
  }

  double timestamp;
};


} // namespace njs

#endif // NJS_JSDATE_H
