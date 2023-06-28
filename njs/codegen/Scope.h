#ifndef NJS_SCOPE_H
#define NJS_SCOPE_H

namespace njs {

class Scope {
 public:
  enum Type {
    GLOBAL_SCOPE,
    FUNC_SCOPE,
    BLOCK_SCOPE
  };

};

} // namespace njs

#endif // NJS_SCOPE_H