#ifndef NJS_RE_BYTECODE_H
#define NJS_RE_BYTECODE_H

#include <memory>
#include <cstdint>

namespace njs {

using std::unique_ptr;

struct REByteCode {
  int length;
  unique_ptr<uint8_t[]> code;
};

}

#endif // NJS_RE_BYTECODE_H