Simple FPGA assembler file format parser
----------------------------------------

Very simple [single-header](./fasm-parse.h) implementation of a parser for the
[fasm] FPGA assembly format.

Attributes are not supported yet and skipped gracefully. Also, bit range
span in a feature assignment is limited to 64 bits currently
(e.g. `FOO[255:192]` is ok, `BAR[255:0]` is too wide).

## API

```c++
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
ParseResult parse(std::string_view content, FILE *errstream,
                  const ParseCallback &parse_callback);
```

## Build and Test

The build builds the test, a testfile generator and a `fasm-validation-parse`
utility using the parser.

```
make
make test
```

## Benchmark parsing a larger file

There is a little `fasm-generate-testfile` utility that creates a dummy
fasm file. By default it creates 100M lines, about 3.6GiB of data:

```
./fasm-generate-testfile > /tmp/dummy.fasm
```

The implementation is not optimized yet, but the parse speed looks
promising:

On an old i7-7500U laptop this parses at about 400MiB/s on a single core:

```
$ ./fasm-validation-parse /tmp/dummy.fasm
Parsing /tmp/dummy.fasm with 3784055505 Bytes.
100000000 lines. XOR of all values: 230C2BE345D5860A
8.781 seconds wall time. 411.0 MiB/s
```

[fasm]: https://fasm.readthedocs.io/
