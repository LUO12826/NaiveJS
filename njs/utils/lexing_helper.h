#ifndef NJS_LEXING_HELPER_H
#define NJS_LEXING_HELPER_H

#include <cstdint>
#include <string>

namespace njs {

using std::u16string;

// Scan index literal. Can only be decimal natural numbers
int64_t scan_index_literal(const u16string& str);

}

#endif // NJS_LEXING_HELPER_H
