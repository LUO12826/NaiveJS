#ifndef NJS_ARRAY_REF_H
#define NJS_ARRAY_REF_H

#include <cassert>
#include <vector>

namespace njs {

using std::vector;

template <typename T>
class ArrayRef {
 public:
  ArrayRef(): ArrayRef(nullptr, 0) {}
  ArrayRef(T* start_ptr, size_t len): start(start_ptr), length(len) {}
  ArrayRef(vector<T>& vec): start(vec.data()), length(vec.size()) {}

  T& operator[](size_t index) const {
    assert(index < length);
    return start[index];
  }

  size_t size() const { return length; }

  T* data() { return start; }

  ArrayRef subarray(size_t begin) {
    if (begin >= length) [[unlikely]] {
      return ArrayRef(start + begin, 0);
    }
    return ArrayRef(start + begin, length - begin);
  }

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
