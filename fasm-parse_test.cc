#include <iostream>
#include <string_view>

#include "fasm-parse.h"

static int expect_mismatch_count = 0;
#define EXPECT_EQ(a, b)                                                        \
  if (a == b) {                                                                \
  } else                                                                       \
    (++expect_mismatch_count, std::cerr)                                       \
        << "**EXPECT fail " << #a << " == " << #b << " (" << a << " vs. " << b \
        << ") "

struct TestCase {
  std::string_view input;
  // Expected outputs
  std::string_view feature_name;
  int min_bit;
  int width;
  uint64_t bits;
};

void Test() {
  constexpr TestCase tests[] = {
      // Names
      {"DOTS.IN.FEATURE\n", "DOTS.IN.FEATURE", 0, 1, 1},
      {"D_1_G1TS\n", "D_1_G1TS", 0, 1, 1},
      {"   \tINDENTED # foo\n", "INDENTED", 0, 1, 1},

      // We don't validate if the start of an identifier is actually in the
      // allowed set that won't include digits.
      // The receiver will verify if the feature exists.
      {"0valid\n", "0valid", 0, 1, 1},

      // Empty lines and comments
      {"\n", "", 0, 0, 0},          // Callback never called
      {" # hello \n", "", 0, 0, 0}, // dito
      {"COMMENT # more stuff\n", "COMMENT", 0, 1, 1},
      {"COMMENT[3:0] = 12 # ok\n", "COMMENT", 0, 4, 12},

      // Implicit set with no assign; explicit zero assign.
      {"IMPLICIT_ONE\n", "IMPLICIT_ONE", 0, 1, 1},
      {"EXPLICIT_ZERO = 0\n", "EXPLICIT_ZERO", 0, 1, 0},

      // An equal assignment without any value followed is interpreted as
      // zero. Maybe too lenient, so maybe should be kError ?
      {"IMPLICIT_ZERO[8:0] =  # no value assigned\n", "IMPLICIT_ZERO", 0, 9, 0},

      // Parsing numbers with included underscores
      {"UNDERSCORE_BITPOS[ _8_ ]\n", //
       "UNDERSCORE_BITPOS", 8, 1, 1},
      {"UNDERSCORE_DECIMAL[15:0] = 1_234\n", //
       "UNDERSCORE_DECIMAL", 0, 16, 1234},
      {"UNDERSCORE_HEXVALUE[15:0] = 'hAB_CD\n", //
       "UNDERSCORE_HEXVALUE", 0, 16, 0xabcd},

      // Decimal, hex, binary and octal
      {"ASSIGN_DECIMAL[3:0] = 5\n", //
       "ASSIGN_DECIMAL", 0, 4, 5},
      {"ASSIGN_DECIMAL[3:0] = 4'd5\n", //
       "ASSIGN_DECIMAL", 0, 4, 5},
      {"ASSIGN_HEX1[15:0] = 16'hCa_Fe\n", //
       "ASSIGN_HEX1", 0, 16, 0xcafe},
      {"ASSIGN_HEX2[31:0] = 32'h_dead_beef\n", //
       "ASSIGN_HEX2", 0, 32, 0xdeadbeef},
      {"ASSIGN_HEX3[31:0] = 32 ' h _dead_beef \n", //
       "ASSIGN_HEX3", 0, 32, 0xdeadbeef},

      {"BINARY[63:48] = 16'b1111_0000_1111_0000\n", "BINARY", 48, 16, 0xF0F0},
      {"ASSIGN_OCT[8:0] = 9'o644\n", //
       "ASSIGN_OCT", 0, 9, 0644},
      {"UNKNOWN_BASE[7:0] = 8'y123\n", //
       "UNKNOWN_BASE", 0, 8, 1},       // fallback to default on bit.

      // Unannounced hex value. Only expect decimal value or width here.
      {"ASSIGN_INVALID[8:0] = beef # hex not expected\n", //
       "ASSIGN_INVALID", 0, 9, 0},
      {"ASSIGN_INVALID[8:0] = 5beef # starts valid dec\n", //
       "ASSIGN_INVALID", 0, 9, 5},

      // Error: inverted ranges or plain old parse errors.
      {"INVERTED_RANGE[0:8]\n", "", 0, 0, 0},    // Callback never called
      {"BRACKET_MISSING[4:0xyz\n", "", 0, 0, 0}, // Callback never called

      // Numbers longer than 64 bit can not be dealt with. Callback not called.
      {"VERY_LONG_NOT_SUPPORTED[255:0] = 256'h1\n", "", 0, 0, 0},

      // Attempt to assign too wide number; warn but comes back properly shaved
      {"ASSIGN_HEX[15:0] = 32'hcafebabe\n", "ASSIGN_HEX", 0, 16, 0xbabe},
      {"ASSIGN_DECIMAL[3:0] = 255\n", //
       "ASSIGN_DECIMAL", 0, 4, 0x0F}, // Shaved down

      // Attributes are acknowledged, but ignored.
      // Global attribute, no feature. Callback never called.
      {"{.foo = \"bar\"}\n", "", 0, 0, 0},

      // Even though attributes are ignored, the values are still parsed.
      {"HELLO {.foo = \"bar\"}\n", "HELLO", 0, 1, 1},
      {"HELLO[5:0] = 42{.foo = \"bar\"}\n", //
       "HELLO", 0, 6, 42},
      {"EXPLICIT_ZERO = 0 {.foo = \"bar\"}\n", "EXPLICIT_ZERO", 0, 1, 0},
  };

  for (const TestCase &expected : tests) {
    fasm_parse(expected.input, stderr,
               [&expected](uint32_t, std::string_view n, int min_bit, int width,
                           uint64_t bits) {
                 EXPECT_EQ(n, expected.feature_name) << expected.input;
                 EXPECT_EQ(min_bit, expected.min_bit) << expected.input;
                 EXPECT_EQ(width, expected.width) << expected.input;
                 EXPECT_EQ(bits, expected.bits) << expected.input;
               });
  }
}

int main() {
  Test();

  if (expect_mismatch_count == 0) {
    printf("\nPASS, all expectations met.\n");
  } else {
    printf("\nFAIL, %d expectations **not** met.\n", expect_mismatch_count);
  }

  return expect_mismatch_count;
}
