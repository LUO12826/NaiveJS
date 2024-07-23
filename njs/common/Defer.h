#ifndef NJS_DEFER_H
#define NJS_DEFER_H

#include <memory>

#define CONCATENATE_DETAIL(x, y) x##y
#define CONCATENATE(x, y) CONCATENATE_DETAIL(x, y)
#define UNIQUE_NAME(prefix) CONCATENATE(prefix, __COUNTER__)
#define defer Defer UNIQUE_NAME(_defer_) = [&]() -> void

namespace njs {

template <typename Func> requires std::invocable<Func>
class Defer {
 public:
  Defer(Func callback) : callback(std::move(callback)) {}

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
  bool dismissed {false};
};

}

#endif //NJS_DEFER_H
