#ifndef NJS_CONVERSION_H
#define NJS_CONVERSION_H

#include "njs/vm/ErrorOr.h"
#include "njs/vm/Completion.h"
#include "JSValue.h"

namespace njs {

using u32 = uint32_t;

bool                    js_to_boolean(JSValue val);
ErrorOr<double>         js_to_number(NjsVM &vm, JSValue val);
ErrorOr<int64_t>        js_to_int64sat(NjsVM &vm, JSValue val);
ErrorOr<u32>            js_to_uint32(NjsVM &vm, JSValue val);
ErrorOr<int32_t>        js_to_int32(NjsVM &vm, JSValue val);
ErrorOr<uint16_t>       js_to_uint16(NjsVM &vm, JSValue val);
ErrorOr<int16_t>        js_to_int16(NjsVM &vm, JSValue val);
Completion              js_to_string(NjsVM &vm, JSValue val, bool to_prop_key = false);
Completion              js_to_object(NjsVM &vm, JSValue val);
Completion              js_to_primitive(NjsVM &vm, JSValue val);
/// return atom or symbol
Completion              js_to_property_key(NjsVM &vm, JSValue val);
Completion              js_require_object_coercible(NjsVM &vm, JSValue val);

}

#endif //NJS_CONVERSION_H
