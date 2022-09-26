// Copyright 2022 Henner Zeller <h.zeller@acm.org>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Simple single-header parser for FPGA assembly file format.

#ifndef SIMPLE_FASM_PARSE_H
#define SIMPLE_FASM_PARSE_H

#include <stdio.h>

#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <functional>
#include <string_view>

namespace fasm {
// Parse callback for FASM lines. The "feature" found in line number "line"
// is set the values given in "bits", starting from lowest "start_bit" (lsb)
// with given "width"
using ParseCallback =
    std::function<void(uint32_t line, std::string_view feature, int start_bit,
                       int width, uint64_t bits)>;

enum class ParseResult {
  kSuccess,     // Successful parse
  kInfo,        // Got info messages, mostly FYI
  kNonCritical, // Found strange values, but mostly non-critical FYI
  kSkipped,     // There were lines that had to be skipped.
  kError        // Errornous input
};

// Parse FPGA assembly file, send parsed values to "parse_callback".
// The "content" is the buffer to parse; last line needs to end with a newline.
// Errors/Warnings are reported to "errstream".
// Attributes are not handled and skipped gracefully.
//
// The "feature" string_view passed into the callback function is not ephemeral
// but backed by the original content, so it is valid for the lifetime of
// "content".
//
// If there are warnings or errors, parsing will continue if possible.
// The most severe issue found is returned.
//
// Spec: https://fasm.readthedocs.io/en/latest/specification/syntax.html
inline ParseResult parse(std::string_view content, FILE *errstream,
                         const ParseCallback &parse_callback);

// -- End of API interface; rest is implementation details

namespace internal {
// -1    : digit separator ('_') -> ignore, but continue reading number
// 0..15 : valid digit (can be used for conversions of all bases)
// > 15  : not a valid digit, number parsing is finished.
// Since we need a number smaller than the smallest digit, need signed values.
inline constexpr int8_t kDigitSeparator = -1;  // _less_ than any digit.
inline constexpr int8_t kDigitToInt[256] = {
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    00,  1,  2,  3,  4,  5,  6,  7,  8,  9, 99, 99, 99, 99, 99, 99,
    99, 10, 11, 12, 13, 14, 15, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, kDigitSeparator,
    99, 10, 11, 12, 13, 14, 15, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
};
} // namespace internal

// [[unlikely]] only available since c++20, s use gcc/clang extension here.
#ifndef unlikely
#define unlikely(x) __builtin_expect((x), 0)
#endif

#define skip_white() while (*it == ' ' || *it == '\t') ++it
#define skip_to_eol() while (*it != '\n') ++it

#define read_decimal(v)                                                        \
  skip_white();                                                                \
  for (int8_t d; (d = internal::kDigitToInt[(uint8_t)*it]) <= 9; ++it)         \
    if (d == internal::kDigitSeparator) {                                      \
    } else                                                                     \
      v = v * 10 + d

#define read_hex(v)                                                            \
  skip_white();                                                                \
  for (int8_t d; (d = internal::kDigitToInt[(uint8_t)*it]) <= 15; ++it)        \
    if (d == internal::kDigitSeparator) {                                      \
    } else                                                                     \
      v = v * 16 + d

#define read_binary(v)                                                         \
  skip_white();                                                                \
  for (int8_t d; (d = internal::kDigitToInt[(uint8_t)*it]) <= 1; ++it)         \
    if (d == internal::kDigitSeparator) {                                      \
    } else                                                                     \
      v = v * 2 + d

#define read_octal(v)                                                          \
  skip_white();                                                                \
  for (int8_t d; (d = internal::kDigitToInt[(uint8_t)*it]) <= 7; ++it)         \
    if (d == internal::kDigitSeparator) {                                      \
    } else                                                                     \
      v = v * 8 + d

inline ParseResult parse(std::string_view content, FILE *errstream,
                         const ParseCallback &parse_callback) {
  if (content.empty()) {
    return ParseResult::kSuccess;
  }
  if (content[content.size() - 1] != '\n') {
    fprintf(errstream, "content does not end with a newline\n");
    return ParseResult::kError;
  }

  ParseResult result = ParseResult::kSuccess;
  const char *it = content.data();
  const char *const end = content.data() + content.size();
  uint32_t line_number = 0;
  uint64_t bitset;
  while (it < end) {
    ++line_number;
    skip_white();
    if (*it == '\n') {
      ++it;
      continue;
    }
    if (*it == '#') {
      skip_to_eol();
      ++it;
      continue;
    }

    // Read feature name; starting with letter or underscore
    const char *const start_feature = it;
    while (*it == '_' || (*it >= 'a' && *it <= 'z') ||
           (*it >= 'A' && *it <= 'Z')) {
      ++it;
    }
    // Remaining identifier can contain dots and numbers.
    while (*it == '.' || *it == '_' || (*it >= '0' && *it <= '9') ||
           (*it >= 'a' && *it <= 'z') || (*it >= 'A' && *it <= 'Z')) {
      ++it;
    }
    std::string_view feature{start_feature, size_t(it - start_feature)};

    skip_white();

    // Read optional bit width.
    int max_bit = 0;
    int min_bit = 0;
    if (*it == '[') {
      ++it;
      read_decimal(max_bit);
      skip_white();
      if (*it == ':') {
        ++it;
        read_decimal(min_bit);
        skip_white();
      } else {
        min_bit = max_bit;
      }
      if (unlikely(*it != ']')) {
        fprintf(errstream, "%u: ERR expected ']' : '%.*s'\n", line_number,
                int(it + 1 - start_feature), start_feature);
        skip_to_eol(); // skip to next line.
        ++it;
        result = std::max(result, ParseResult::kError);
        continue;
      }
      ++it;
      if (unlikely(max_bit < min_bit)) {
        fprintf(errstream, "%u: SKIP inverted range %.*s[%d:%d]\n", line_number,
                (int)feature.size(), feature.data(), max_bit, min_bit);
        skip_to_eol(); // skip to next line.
        result = std::max(result, ParseResult::kSkipped);
        continue;
      }
    }
    skip_white();

    uint32_t width = (max_bit - min_bit + 1);
    if (unlikely(width > 64)) {
      // TODO: if this happens in practice, then parse in multiple steps and
      // call back multiple times with parts of the number.
      fprintf(errstream,
              "%u: ERR: Sorry, can only deal with ranges <= 64 bit currently "
              "%.*s[%d:%d]; trimming width %u to 64\n",
              line_number, (int)feature.size(), feature.data(), max_bit,
              min_bit, width);
      result = std::max(result, ParseResult::kError);
      width = 64;  // Clamp number of bits we report.
      // Move on with best effort parsing of lower 64 bits.
    }

    // Assignment.
    if (*it == '=') {
      ++it;
      skip_white();
      bitset = 0;
      if (internal::kDigitToInt[(uint8_t)*it] <= 9) {
        read_decimal(bitset);   // Could be precision or decimal value
      }
      skip_white();
      if (*it == '\'') {
        // Ok, that was actually precision. simple test, but ignore it mostly.
        if (unlikely(bitset > width)) {
          fprintf(errstream,
                  "%u: WARN Attempt to assign more bits (%" PRIu64 "') for "
                  "%.*s[%d:%d] with supported bit width of %u\n",
                  line_number, bitset, (int)feature.size(), feature.data(),
                  max_bit, min_bit, width);
          result = std::max(result, ParseResult::kNonCritical);
        }
        bitset = 0;
        ++it;
        skip_white();
        const char format_type = *it;
        ++it;
        switch (format_type) {
        case 'h':
          read_hex(bitset);
          break;
        case 'b':
          read_binary(bitset);
          break;
        case 'o':
          read_octal(bitset);
          break;
        case 'd':
          read_decimal(bitset);
          break;
        default:
          fprintf(errstream, "%u: unknown base signifier '%c'\n", line_number,
                  format_type);
          result = std::max(result, ParseResult::kError);
          skip_to_eol();
          bitset = 0x01;
          break;
        }
        skip_white();
      }
    } else {
      bitset = 0x1; // assignment fallback: default assumption 1 bit set.
      if (unlikely(min_bit != max_bit)) {
        fprintf(errstream,
                "%u: INFO Range of bits %.*s[%d:%d], but no assignment\n",
                line_number, (int)feature.size(), feature.data(), max_bit,
                min_bit);
        result = std::max(result, ParseResult::kInfo);
      }
    }

    bitset &= uint64_t(-1) >> (64 - width); // Clamp bits.

    if (*it == '{') {
      fprintf(errstream, "%u: INFO ignored attributes for '%.*s'\n",
              line_number, (int)feature.size(), feature.data());
      result = std::max(result, ParseResult::kInfo);
      skip_to_eol(); // attributes; not implemented yet.
      if (feature.empty()) continue;  // Global file annotation
      // Move on: as we still might have assigned bits to report in callback
    }

    if (*it == '#') {
      skip_to_eol();
    }
    if (unlikely(*it != '\n')) {
      fprintf(errstream, "%d: unexpected non-newline: '%c'\n", line_number,
              *it);
      result = std::max(result, ParseResult::kError);
      skip_to_eol(); // skip to next line.
    }
    ++it;

    parse_callback(line_number, feature, min_bit, width, bitset);
  }
  return result;
}

#undef read_octal
#undef read_binary
#undef read_hex
#undef read_decimal
#undef skip_to_eol
#undef skip_white
#undef unlikely

}  // namespace fasm
#endif  // SIMPLE_FASM_PARSE_H
