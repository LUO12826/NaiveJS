#ifndef NJS_DEFER_H
#define NJS_DEFER_H

#include <memory>

namespace njs {

template <typename Func> requires std::invocable<Func>
class Defer {
 public:
  explicit Defer(Func callback) : callback(std::move(callback)) {}

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
