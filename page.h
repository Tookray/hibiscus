#pragma once

#include <cstddef>
#include <unistd.h>

namespace page {

// The page size of the system.
const size_t PAGE_SIZE = sysconf(_SC_PAGESIZE);

// Calls `mmap` to allocate one or more pages.
void *allocate(size_t size);

// Calls `munmap` to deallocate a page.
void free(void *ptr);

// Calls `munmap` to deallocate one or more pages.
void free(void *ptr, size_t size);

} // namespace page
