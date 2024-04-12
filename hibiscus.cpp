#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <sys/mman.h>
#include <utility>
#include <valgrind/memcheck.h>

#include "chi/panic.h"

#include "block.h"
#include "freelist.h"
#include "hibiscus.h"

// The page size of the system.
constexpr const size_t PAGE_SIZE = 4096;

// Split a page into two blocks. The first block will be of header size plus
// the size requested, and the second block will be the remainder of the page.
// However, if the second block is unable to hold one or more bytes, then it
// will not be set.
std::pair<hibiscus::Block *, hibiscus::Block *> page_split(void *ptr,
                                                           size_t size) {
  if (ptr == nullptr || size == 0 ||
      size > PAGE_SIZE - sizeof(hibiscus::Block)) {
    // Something went wrong, the arguments you passed are invalid. You should
    // probably fix that.
    chi::panic("Invalid arguments to page_split: ptr = {}, size = {}", ptr,
               size);
  }

  hibiscus::Block *left = reinterpret_cast<hibiscus::Block *>(ptr);

  left->size = size;
  left->next = nullptr;
  left->prev = nullptr;

  if (sizeof(hibiscus::Block) + size + sizeof(hibiscus::Block) >= PAGE_SIZE) {
    // There isn't enough space for a second block.
    return std::make_pair(left, nullptr);
  }

  hibiscus::Block *right = reinterpret_cast<hibiscus::Block *>(
      static_cast<std::byte *>(ptr) + sizeof(hibiscus::Block) + size);

  right->size =
      PAGE_SIZE - sizeof(hibiscus::Block) - size - sizeof(hibiscus::Block);
  right->next = nullptr;
  right->prev = nullptr;

  return std::make_pair(left, right);
}

// Grab a page from the system.
void *page_alloc() {
  void *ptr = mmap(nullptr, PAGE_SIZE, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (ptr == MAP_FAILED) {
    // The system didn't give us a page of memory, let's abort.
    chi::panic("Failed to allocate a page of memory");
  }

  // Valgrind and friends do not know about the memory we just allocated, so
  // we need to tell them about it.
  VALGRIND_MALLOCLIKE_BLOCK(ptr, PAGE_SIZE, 0, 0);

  return ptr;
}

// Release a page back to the system.
void page_free(void *ptr) {
  // We can't free a null pointer.
  if (ptr == nullptr) {
    return;
  }

  if (munmap(ptr, PAGE_SIZE) == -1) {
    // Trying to free the memory back to the system failed, this is beyond our
    // pay grade, so we'll just panic.
    chi::panic("Failed to free memory back to the kernel");
  }

  // Like during the allocation, we need to tell Valgrind about the memory we
  // just freed.
  VALGRIND_FREELIKE_BLOCK(ptr, 0);
}

namespace hibiscus {
void *allocate(size_t size) {
  if (size == 0) {
    // Invalid size requested, let's return a null pointer.
    return nullptr;
  }

  if (size > PAGE_SIZE - sizeof(Block)) {
    // This is just a limitation of the current implementation, we can fix this
    // later.
    return nullptr;
  }

  // Try to grab a block from the free list.
  std::optional<Block *> block = dll::first(
      [size](hibiscus::Block *block) { return block->size >= size; });

  if (block) {
    return block.value()->data();
  }

  // If we can't, then we'll need to allocate a new page and carve out a block
  // from it.
  void *page = page_alloc();

  if (page == nullptr) {
    // We couldn't allocate a page.
    return nullptr;
  }

  auto [left, right] = page_split(page, size);

  assert(left != nullptr && "The left block should never be a null pointer");

  if (right != nullptr) {
    if (auto result = dll::push_front(right); !result) {
      chi::panic("Failed to add the right block to the free list");
    }
  }

  return left->data();
}

void free(void *ptr) {
  if (ptr == nullptr) {
    return;
  }

  Block *header = static_cast<Block *>(ptr) - 1;

  // Zero out the memory for safety reasons.
  header->zero();

  // --------------------------------------------------------------------------
  // TODO: Coalesce blocks and release pages back to the system.
  // --------------------------------------------------------------------------

  // Add the block back to the free list.
  if (auto result = dll::push_front(header); !result) {
    chi::panic("Failed to add the block back to the free list");
  }
}
} // namespace hibiscus
