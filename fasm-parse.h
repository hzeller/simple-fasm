#ifndef SIMPLE_FASM_PARSE_H
#define SIMPLE_FASM_PARSE_H

#include <stdio.h>

#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <functional>
#include <string_view>

// Parse callback for FASM lines. The "feature" found in line number "line"
// is set the values given in "bits", starting from lowest "start_bit" (lsb)
// with given "width"
using FasmParseCallback =
    std::function<void(uint32_t line, std::string_view feature, int start_bit,
                       int width, uint64_t bits)>;

enum class ParseResult {
  kSuccess,
  kInfo,
  kNonCritical,
  kSkipped,
  kError
};

#define skip_white() while (*it == ' ' || *it == '\t') ++it
#define skip_to_eol() while (*it != '\n') ++it

#define read_decimal(v)                                                        \
  skip_white();                                                                \
  for (/**/; (*it >= '0' && *it <= '9') || *it == '_'; ++it)                   \
    if (*it == '_') {                                                          \
    } else                                                                     \
      v = v * 10 + (*it - '0')

#define read_hex(v)                                                            \
  skip_white();                                                                \
  for (/**/; (*it >= '0' && *it <= '9') || (*it >= 'a' && *it <= 'f') ||       \
             (*it >= 'A' && *it <= 'F') || *it == '_';                         \
       ++it)                                                                   \
    if (*it == '_') {                                                          \
    } else                                                                     \
      v = v * 16 +                                                             \
          (*it - ((*it <= '9') ? '0' : (((*it <= 'F') ? 'A' : 'a') - 10)))

#define read_binary(v)                                                         \
  skip_white();                                                                \
  for (/**/; (*it >= '0' && *it <= '1') || *it == '_'; ++it)                   \
    if (*it == '_') {                                                          \
    } else                                                                     \
      v = v * 2 + (*it - '0')

#define read_octal(v)                                                          \
  skip_white();                                                                \
  for (/**/; (*it >= '0' && *it <= '7') || *it == '_'; ++it)                   \
    if (*it == '_') {                                                          \
    } else                                                                     \
      v = v * 8 + (*it - '0')

// Parse FPGA assembly file, send parsed values to "parse_callback".
// The "content" is the buffer to parse; last line needs to end with a newline.
// Errors/Warnings are reported to "errstream".
// Attributes are not handled and skipped gracefully.
//
// If there are warnings or errors, parsing will continue if possible.
// The most severe issue found is returned.
inline ParseResult fasm_parse(std::string_view content, FILE *errstream,
                              const FasmParseCallback &parse_callback) {
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
      if (*it != ']') {
        fprintf(errstream, "%u: ERR expected ']' : '%.*s'\n", line_number,
                int(it + 1 - start_feature), start_feature);
        skip_to_eol(); // skip to next line.
        ++it;
        result = std::max(result, ParseResult::kError);
        continue;
      }
      ++it;
      if (max_bit < min_bit) {
        fprintf(errstream, "%u: SKIP inverted range %.*s[%d:%d]\n", line_number,
                (int)feature.size(), feature.data(), max_bit, min_bit);
        skip_to_eol(); // skip to next line.
        result = std::max(result, ParseResult::kSkipped);
        continue;
      }
    }
    skip_white();

    const uint32_t width = (max_bit - min_bit + 1);
    if (width > 64) {
      // TODO: if this happens in practice, then parse in multiple steps and
      // call back multiple times with parts of the number.
      fprintf(errstream,
              "%u: ERR: Sorry, can only deal with ranges <= 64 bit currently "
              "%.*s[%d:%d]; width %u\n",
              line_number, (int)feature.size(), feature.data(), max_bit,
              min_bit, width);
      result = std::max(result, ParseResult::kError);
      skip_to_eol();
      continue;
    }

    // Assignment.
    if (*it == '=') {
      ++it;
      skip_white();
      bitset = 0;
      if (*it >= '0' && *it <= '9') { // Could be precision or decimal value
        read_decimal(bitset);
      }
      skip_white();
      if (*it == '\'') {
        // Ok, that was actually precision. simple test, but ignore it mostly.
        if (bitset > width) {
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
      if (min_bit != max_bit) {
        fprintf(errstream,
                "%u: INFO Range of bits %.*s[%d:%d], but no assignment\n",
                line_number, (int)feature.size(), feature.data(), max_bit,
                min_bit);
        result = std::max(result, ParseResult::kInfo);
      }
    }

    bitset &= (uint64_t(1) << width) - 1;  // Clamp bits

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
    if (*it != '\n') {
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

#endif  // SIMPLE_FASM_PARSE_H
