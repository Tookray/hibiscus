#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <sys/mman.h>
#include <valgrind/memcheck.h>

#include "chi/assert.h"
#include "chi/panic.h"

#include "page.h"

namespace page {

// NOTE: These functions are probably 'noexcept'. It's true that 'cout' can
//       throw an exception, but practically nobody ever calls
//       'cout.exceptions(iostate)' to enable that. However, the more important
//       question is whether we should even add 'noexcept' to these functions
//       in the first place. Read this article before making a decision:
//       https://akrzemi1.wordpress.com/2011/06/10/using-noexcept/

void *allocate(size_t size) {
#ifdef DEBUG
  chi::assert(size != 0, "size must be non-zero");
#endif

  // NOTE: Should size be rounded up to the nearest multiple of PAGE_SIZE? As
  //       far as I know, it's not necessary because the kernel will round it
  //       up for us.

  void *ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (ptr == MAP_FAILED) {
    chi::panic("MMAP failed");
  }

  VALGRIND_MALLOCLIKE_BLOCK(ptr, size, 0, 0);

#ifdef DEBUG
  std::cout << std::format("Allocated {} bytes at {}\n\n", size, ptr);
#endif

  return ptr;
}

void free(void *ptr) { free(ptr, PAGE_SIZE); }

void free(void *ptr, size_t size) {
#ifdef DEBUG
  chi::assert(ptr != nullptr, "ptr must not be null");
  chi::assert(reinterpret_cast<std::uintptr_t>(ptr) % PAGE_SIZE == 0,
              "ptr must be aligned to the page size");
  chi::assert(size != 0, "size must be non-zero");
#endif

  if (munmap(ptr, size) == -1) {
    chi::panic("MUNMAP failed");
  }

  VALGRIND_FREELIKE_BLOCK(ptr, 0);

#ifdef DEBUG
  std::cout << std::format("Freed {} bytes at {}\n\n", size, ptr);
#endif
}

} // namespace page
