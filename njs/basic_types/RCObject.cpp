#include <iostream>
#include "RCObject.h"
#include "njs/global_var.h"

namespace njs {

void RCObject::retain() { ref_count += 1; }

void RCObject::release() {
  assert(ref_count != 0);
  ref_count -= 1;
  if (ref_count == 0) {
    if (Global::show_gc_statistics) std::cout << "RC remove an RCObject" << std::endl;
    delete this;
  }
}

void RCObject::mark_as_temp() {
  ref_count -= 1;
  assert(ref_count >= 0);
}

void RCObject::delete_temp_object() {
  assert(ref_count == 0);
  if (Global::show_gc_statistics) std::cout << "RC remove an temporary RCObject" << std::endl;
  delete this;
}

u32 RCObject::get_ref_count() const {
  return ref_count;
}

}