#ifndef NJS_ARRAY_REF_H
#define NJS_ARRAY_REF_H

#include <cassert>

namespace njs {

template <typename T>
class ArrayRef {
 public:
  ArrayRef(T* start_ptr, size_t len): start(start_ptr), length(len) {}

  T& operator[](size_t index) const {
    assert(index < length);
    return start[index];
  }

  size_t size() const { return length; }

  T* data() { return start; }

  ArrayRef subarray(size_t begin, size_t len) {
    if (begin >= length) [[unlikely]] {
      return ArrayRef(start + begin, 0);
    }
    size_t num_left = length - begin;
    return ArrayRef(start + begin, std::min(num_left, len));
  }

 private:
  T* start;
  size_t length;
};

}

#endif // NJS_ARRAY_REF_H
