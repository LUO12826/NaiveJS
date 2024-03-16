#ifndef NJS_DEFER_H
#define NJS_DEFER_H

#include <memory>

namespace njs {

template <typename Func>
class Defer {
 public:
  explicit Defer(Func callback) : callback(std::move(callback)), dismissed(false) {}

  ~Defer() {
    if (!dismissed) {
      callback();
    }
  }

  void dismiss() {
    dismissed = true;
  }

 private:
  Func callback;
  bool dismissed;
};

}

#endif //NJS_DEFER_H
