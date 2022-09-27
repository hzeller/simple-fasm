Simple FPGA assembler file format parser
----------------------------------------

Simple to use and fast [single-header](./fasm-parse.h) implementation of a
parser for the [fasm] FPGA assembly format.

This is _not_ related to the implementation found at [chipsalliance-fasm] [^1].

Bit address ranges span is limited to 64 bits currently.
(e.g. `FOO[255:192]` is ok, `BAR[255:0]` is too wide). Looks like at most
32 bit wide address ranges are used in the wild anyway.

Attributes are not reported in the callback yet, but skipped gracefully.

The parser header file is about 300 lines of well-documented simple C++; the
`fasm-validation-parse` x84_64 binary occupies about 24KiB.

## API

Call `fasm::parse()` and pass it a callback that receives feature names
with bit-patterns to be set.

```c++
// Parse callback for FASM lines. The "feature" found in line number "line"
// is set the values given in "bits", starting from lowest "start_bit" (lsb)
// with given "width"
using ParseCallback =
    std::function<void(uint32_t line, std::string_view feature, int start_bit,
                       int width, uint64_t bits)>;

// Result values in increasing amount of severity. Start to worry at kSkipped.
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

There is a `fasm-generate-testfile` utility that creates a dummy fasm file.
By default it generates 100M lines, about 3.6GiB of data:

```
./fasm-generate-testfile > /tmp/dummy.fasm
```

Each line contains a feature name, an address range and a hex number
assignment with random ranges and values, so the most involved from a parsing
perspective.

```
MVGBNGRMWJOBC[67:49] = 19'h43586
K_MDKBS_FDEDMB[229:215] = 15'h107
A_A_TMSDWHKOT[85:32] = 54'h343cc44685bba8
PSRFHJKAEYWSHB[17:11] = 7'h49
LJXZGXXXNPCHIB[269:241] = 29'hab73330
WAXEQOUIRVXXQ[173:148] = 26'h35a5cf8
NPKGUGSNGFQGN[185:149] = 37'h92c9d9f57
# ...
```

The `fasm-validation-parse` utility parses the given files and reports issues
as well as basic parse performance.

On an old i7-7500U laptop the file generated above parses with about 700MiB/s
on a single core:

```
$ ./fasm-validation-parse /tmp/dummy.fasm
Parsing /tmp/dummy.fasm with 3784055505 Bytes.
100000000 lines. XOR of all values: 230C2BE345D5860A
1 thread. 5.039s wall time. 716.1 MiB/s
```

Since files can be split at line-boundaries, it is also possible to parse
sub-parts of the file in parallel by setting the `PARALLEL_FASM` environment
variable to the number of desired threads the `fasm-validation-parse` should
use.

On this Ryzen 1950X, this reaches > 16 GiB/s parse speed:

```
$ PARALLEL_FASM=32 ./fasm-validation-parse /tmp/dummy.fasm
Parsing /tmp/dummy.fasm with 3784055505 Bytes.
100000000 lines. XOR of all values: 230C2BE345D5860A
32 threads. 0.210s wall time. 17190.6 MiB/s
```

This just parsed 100 Million FASM lines with address ranges and hex-number
assignment in a fifth of a second. Not too shabby.

[^1]: which I couldn't get to compile because of Conda/Python fragility and
bloat. That checked out repository with environment set-up and build takes
about 1.8G of disk, then the test fails with some dependency issue...

[fasm]: https://fasm.readthedocs.io/
[chipsalliance-fasm]: https://github.com/chipsalliance/fasm
