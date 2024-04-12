#pragma once

#include <cstddef>

namespace hibiscus {
// Allocate memory.
void *allocate(size_t size);

// Free memory.
void free(void *ptr);
} // namespace hibiscus
