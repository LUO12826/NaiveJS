#ifndef NJS_SPAN_H
#define NJS_SPAN_H

#include <cassert>
#include <vector>

namespace njs {

using std::vector;

template <typename T>
class Span {
 public:

  using iterator = T*;
  using const_iterator = const T*;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  Span(): Span(nullptr, 0) {}
  Span(T* start_ptr, size_t len): start(start_ptr), length(len) {}
  Span(vector<T>& vec): start(vec.data()), length(vec.size()) {}

  T& operator[](size_t index) const {
    assert(index < length);
    return start[index];
  }

  size_t size() const { return length; }
  bool empty() const { return length == 0; }

  T* data() { return start; }

  Span subarray(size_t begin) {
    if (begin >= length) [[unlikely]] {
      return Span(start + begin, 0);
    }
    return Span(start + begin, length - begin);
  }

  Span subarray(size_t begin, size_t len) {
    if (begin >= length) [[unlikely]] {
      return Span(start + begin, 0);
    }
    size_t num_left = length - begin;
    return Span(start + begin, std::min(num_left, len));
  }

  iterator begin() { return start; }
  const_iterator begin() const { return start; }
  iterator end() { return start + length; }
  const_iterator end() const { return start + length; }

  reverse_iterator rbegin() { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }

  const_iterator cbegin() const { return start; }
  const_iterator cend() const { return start + length; }
  const_reverse_iterator crbegin() const { return const_reverse_iterator(end()); }
  const_reverse_iterator crend() const { return const_reverse_iterator(begin()); }


 private:
  T* start;
  size_t length;
};

}

#endif // NJS_SPAN_H
