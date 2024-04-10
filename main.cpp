#include <cstddef>
#include <cstdio>
#include <expected>
#include <iostream>
#include <optional>
#include <sys/mman.h>
#include <valgrind/memcheck.h>

#include "chi/panic.h"

// The page size of the system.
constexpr const size_t PAGE_SIZE = 4096;

// Allocate a page of memory from the kernel.
std::optional<void *> allocate_page() {
  void *p = mmap(nullptr, PAGE_SIZE, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (p == MAP_FAILED) {
#ifdef DEBUG
    chi::panic("Failed to get more memory from the kernel");
#endif

    return std::nullopt;
  }

  // Valgrind and friends do not know about the memory we just allocated, so
  // we need to tell them about it.
  VALGRIND_MALLOCLIKE_BLOCK(p, PAGE_SIZE, 0, 0);

  return p;
}

// Free a page of memory back to the kernel.
std::expected<void, std::string> free_page(void *p) {
  if (p == nullptr) {
    return std::unexpected("Cannot free a null pointer.");
  }

  if (munmap(p, PAGE_SIZE) == -1) {
#ifdef DEBUG
    chi::panic("Failed to free memory back to the kernel");
#endif

    return std::unexpected("Failed to free memory back to the kernel.");
  }

  // Like the allocation, we need to tell Valgrind about the memory we just
  // freed.
  VALGRIND_FREELIKE_BLOCK(p, 0);

  return {};
}

int main() {
  std::optional<void *> p = allocate_page();

  if (!p.has_value()) {
    return 1;
  }

  int *q = static_cast<int *>(p.value());

  *q = 42;

  std::cout << *q << std::endl;

  if (!free_page(static_cast<void *>(q)).has_value()) {
    return 1;
  }
}
