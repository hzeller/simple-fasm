// Generate some fasm file for testing.

#include <stdio.h>
#include <stdlib.h>

#include <cinttypes>
#include <cstdint>

constexpr int64_t kDefaultCount = 100'000'000;  // Results in ~3.5GiB file.

int usage(const char *progname) {
  fprintf(stderr, "usage: %s <optional-count>\nDefault: %" PRId64 "\n",
          progname, kDefaultCount);
  return 1;
}

int main(int argc, char *argv[]) {
  if (argc > 2) {
    return usage(argv[0]);
  }
  const int64_t count = (argc == 2) ? atol(argv[1]) : kDefaultCount;
  if (count == 0) {
    return usage(argv[0]);
  }

  srand(42); // Make 'random' numbers repeatable.

  constexpr int kEncodePool = 27;
  constexpr char encode_chars[kEncodePool + 1] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ_";
  char feature_name[128];
  for (int64_t i = 0; i < count; ++i) {
    // Create some pseudo-random string as feature name from a random id.
    uint64_t feature_id = (uint64_t)rand() << 32 | (uint64_t)rand();
    int feature_len = 0;
    for (/**/; feature_id; feature_id /= kEncodePool) {
      feature_name[feature_len++] = encode_chars[feature_id % kEncodePool];
    }
    fwrite(feature_name, feature_len, 1, stdout);

    const int bit = rand() % 256;
    const int width = rand() % 63 + 1;  // min 1 up to max width we can parse.
    const uint64_t long_rand = (uint64_t)rand() << 32 | (uint64_t)rand();
    printf("[%d:%d] = %d'h%" PRIx64 "\n", bit + width - 1, bit, // bit range
           width, long_rand & ((1L << width) - 1));
  }
}
