// See if a file can be parsed successfully with fasm-parse and simple benchmark

#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <string_view>
#include <thread>

#include "fasm-parse.h"

int64_t getTimeInMicros() {
  struct timeval t;
  gettimeofday(&t, NULL);
  return (int64_t)t.tv_sec * 1000000 + t.tv_usec;
}

struct ParseStatistics {
  uint64_t accumulate = 0;
  uint32_t last_line = 0;
};

// Parse file and print number of lines and performance report.
fasm::ParseResult ParseFile(const char *fasm_file) {
  const int fd = open(fasm_file, O_RDONLY);
  if (fd < 0) {
    perror("Can't open file");
    return fasm::ParseResult::kError;
  }

  struct stat s;
  fstat(fd, &s);
  const size_t file_size = s.st_size;

  fprintf(stdout, "Parsing %s with %zu Bytes.\n", fasm_file, file_size);
  if (file_size == 0) {
    fprintf(stdout, "Empty file.\n");
    close(fd);
    return fasm::ParseResult::kSuccess;
  }

  // Memory map everything into a convenient contiguous buffer
  void *const buffer = mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  if (buffer == MAP_FAILED) {
    perror("Couldn't map file");
    return fasm::ParseResult::kError;
  }

  std::string_view content((const char *)buffer, file_size);
  const int64_t start_us = getTimeInMicros();
  ParseStatistics stats;
  fasm::ParseResult result = fasm::parse(
    content, stderr,
    [&stats](uint32_t line, std::string_view, int, int, uint64_t bits) {
      stats.accumulate ^= bits;
      stats.last_line = line;
    });
  const int64_t duration_us = getTimeInMicros() - start_us;

  fprintf(stdout, "%d lines. XOR of all values: %" PRIX64 "\n",
          stats.last_line, stats.accumulate);
  constexpr float MiBFactor = 1e6 / (1 << 20);
  const float bytes_per_microsecond = 1.0f * file_size / duration_us;
  fprintf(stdout,
          "%.3f seconds wall time. %.1f MiB/s\n",
          duration_us / 1e6, bytes_per_microsecond * MiBFactor);
  munmap(buffer, file_size);

  return result;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("usage: %s <fasm-file> [<fasm-file>...]\n", argv[0]);
    return 1;
  }

  fasm::ParseResult combined_result = fasm::ParseResult::kSuccess;
  for (int i = 1; i < argc; ++i) {
    if (i != 1) fprintf(stdout, "\n");
    auto result = ParseFile(argv[i]);
    combined_result = std::max(combined_result, result);
  }

  return combined_result <= fasm::ParseResult::kNonCritical ? 0 : 1;
}
