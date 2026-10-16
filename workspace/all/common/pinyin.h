#ifndef PINYIN_H
#define PINYIN_H

#include <stdbool.h>
#include <stddef.h>

// Fold a UTF-8 string into a pinyin-initials key: each CJK ideograph becomes
// its most-common reading's first letter (吞食天地 -> tstd), ASCII letters
// are lowercased, digits are kept, everything else is dropped. Returns true
// when at least one character landed in `out` (an all-ASCII name yields its
// lowercased form, which is still useful for case-insensitive matching).
bool pinyin_initials(const char* utf8, char* out, size_t out_size);

#endif
