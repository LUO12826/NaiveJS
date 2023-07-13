#include <cstddef>
#include <iostream>
#include "RCObject.h"
#include "njs/global_var.h"
#include "njs/utils/helper.h"

namespace njs {

size_t JSSymbol::global_count = 0;

void RCObject::retain() { ref_count += 1; }

void RCObject::release() {
  assert(ref_count != 0);
  ref_count -= 1;
  if (ref_count == 0) {
    if (Global::show_gc_statistics) std::cout << "RC remove an RCObject" << std::endl;
    delete this;
  }
}

void RCObject::delete_temp_object() {
  assert(ref_count == 0);
  delete this;
}

u32 RCObject::get_ref_count() {
  return ref_count;
}

PrimitiveString::PrimitiveString(const std::u16string& str): str(str) {}

bool PrimitiveString::operator == (const PrimitiveString& other) const {
  return str == other.str;
}

int64_t PrimitiveString::convert_to_index() const {
  return scan_index_literal(this->str);
}

JSSymbol::JSSymbol(std::u16string name): name(std::move(name)) {
  seq = JSSymbol::global_count;
  JSSymbol::global_count += 1;
}

bool JSSymbol::operator == (const JSSymbol& other) const {
  return name == other.name && seq == other.seq;
}

std::string JSSymbol::to_string() {
  return "JSSymbol(" + to_utf8_string(name) + ")";
}

}