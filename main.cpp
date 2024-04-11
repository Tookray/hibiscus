#include <cstddef>
#include <cstring>
#include <expected>
#include <iostream>
#include <optional>
#include <sys/mman.h>
#include <valgrind/memcheck.h>

#include "chi/panic.h"

#include "block.h"
#include "freelist.h"

// The page size of the system.
constexpr const size_t PAGE_SIZE = 4096;

// Allocate a page of memory from the kernel.
std::optional<void *> allocate_page() {
  void *ptr = mmap(nullptr, PAGE_SIZE, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (ptr == MAP_FAILED) {
#ifdef DEBUG
    chi::panic("Failed to get more memory from the kernel");
#endif

    return std::nullopt;
  }

  // Valgrind and friends do not know about the memory we just allocated, so
  // we need to tell them about it.
  VALGRIND_MALLOCLIKE_BLOCK(ptr, PAGE_SIZE, 0, 0);

  return ptr;
}

// Free a page of memory back to the kernel.
std::expected<void, std::string> free_page(void *ptr) {
  if (ptr == nullptr) {
    return std::unexpected("Cannot free a null pointer.");
  }

  if (munmap(ptr, PAGE_SIZE) == -1) {
#ifdef DEBUG
    chi::panic("Failed to free memory back to the kernel");
#endif

    return std::unexpected("Failed to free memory back to the kernel.");
  }

  // Like the allocation, we need to tell Valgrind about the memory we just
  // freed.
  VALGRIND_FREELIKE_BLOCK(ptr, 0);

  return {};
}

// Split a page into two pages at the given size.
std::pair<std::optional<hibiscus::Block *>, std::optional<hibiscus::Block *>>
split_page(void *ptr, size_t size) {
  if (ptr == nullptr || size == 0 ||
      size > PAGE_SIZE - sizeof(hibiscus::Block)) {
    return std::make_pair(std::nullopt, std::nullopt);
  }

  // We always have a usable block.
  void *usable = ptr;
  static_cast<hibiscus::Block *>(usable)->size = size;

  // Let's see if there is enough space for a second block.
  if (sizeof(hibiscus::Block) + size + sizeof(hibiscus::Block) + 1 <=
      PAGE_SIZE) {
    void *remainder =
        static_cast<std::byte *>(ptr) + sizeof(hibiscus::Block) + size;

    static_cast<hibiscus::Block *>(remainder)->size =
        PAGE_SIZE - sizeof(hibiscus::Block) - size - sizeof(hibiscus::Block);

    return std::make_pair(static_cast<hibiscus::Block *>(usable),
                          static_cast<hibiscus::Block *>(remainder));
  }

  // Otherwise, we can only return the usable part.
  return std::make_pair(static_cast<hibiscus::Block *>(usable), std::nullopt);
}

// Allocate memory at the memory management level.
void *allocate(size_t size) {
  // It is impossible to allocate a page of memory that is zero bytes or larger
  // than the page size (for now).
  if (size == 0 || size > PAGE_SIZE - sizeof(hibiscus::Block)) {
    return nullptr;
  }

  // Look for a free block of memory.
  auto block = hibiscus::dll::first(
      [size](hibiscus::Block *block) { return block->size >= size; });

  if (block.has_value()) {
    return block.value()->data();
  }

  // If we cannot find a free block, then we need to allocate a new page and
  // carve out a block from it to return to the user.
  std::optional<void *> page = allocate_page();

  // If we cannot allocate a new page, then there is nothing we can do.
  if (!page.has_value()) {
    return nullptr;
  }

  // Split the page into 2 parts.
  auto [usable, remainder] =
      split_page(page.value(), size + sizeof(hibiscus::Block));

  // Add the remainder to the list of free blocks if we can make a new block
  // from it.
  if (remainder.has_value()) {
    if (auto result = hibiscus::dll::push_back(remainder.value());
        !result.has_value()) {
      chi::panic("Failed to add the remainder to the free list");
    }
  }

  return usable.value()->data();
}

// Free memory at the memory management level.
void deallocate(void *ptr) {
  // Make sure that the pointer can actually be deallocated.
  if (ptr == nullptr) {
    return;
  }

  // From reasearch, it looks like reinterpret casting is considered as a bad
  // practice, but since we are not updating the pointer in any way, I think it
  // should be fine.
  hibiscus::Block *header = reinterpret_cast<hibiscus::Block *>(
      static_cast<std::byte *>(ptr) - sizeof(hibiscus::Block));

  // For safety reasons, let's zero out the memory.
  std::memset(header->data(), 0, header->size);
}

int main() {
  int *ptr = static_cast<int *>(allocate(sizeof(int)));

  if (ptr == nullptr) {
    chi::panic("Failed to allocate memory");
  }

  *ptr = 42;

  std::cout << "The value of ptr is " << *ptr << std::endl;

  deallocate(ptr);
}
