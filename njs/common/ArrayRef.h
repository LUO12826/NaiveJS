#ifndef NJS_ARRAY_REF_H
#define NJS_ARRAY_REF_H

#include <cstdlib>
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

 private:
  T* start;
  size_t length;
};

}

#endif // NJS_ARRAY_REF_H
