#ifndef NJS_CONVERSION_H
#define NJS_CONVERSION_H

#include <string>
#include <algorithm>
#include <limits>
#include "njs/vm/ErrorOr.h"
#include "njs/vm/Completion.h"
#include "JSValue.h"

namespace njs {

using std::u16string;
using u32 = uint32_t;

double u16string_to_double(const u16string &str);
ErrorOr<double> to_number(NjsVM &vm, JSValue val);
ErrorOr<u32> to_uint32(NjsVM &vm, JSValue val);
ErrorOr<int32_t> to_int32(NjsVM &vm, JSValue val);
Completion to_string(NjsVM &vm, JSValue val, bool to_prop_key = false);
Completion to_object(NjsVM &vm, JSValue val);
/// return atom or symbol
Completion to_property_key(NjsVM &vm, JSValue val);
u16string double_to_string(double n);

}

#endif //NJS_CONVERSION_H
