#ifndef NJS_LEXING_HELPER_H
#define NJS_LEXING_HELPER_H

#include <cstdint>
#include <string>
#include <optional>

namespace njs {

using std::u16string;
using std::optional;
using u32 = uint32_t;

// Scan index literal. Can only be decimal natural numbers
int64_t scan_index_literal(const u16string& str);

// Scan decimal number. Only allows decimal digits.
optional<uint64_t> scan_decimal_literal(const char16_t *str, u32 str_len, u32& cursor);

// Scan integer. Allows decimal digits and hexadecimal digits.
optional<uint64_t> scan_integer_literal(const char16_t *str, u32 str_len, u32& cursor, int base = 10);

optional<double> scan_numeric_literal(const char16_t *str, u32 str_len, u32& cursor);

double scan_fractional_part(const char16_t *str, u32 str_len, u32& cursor);

}

#endif // NJS_LEXING_HELPER_H
