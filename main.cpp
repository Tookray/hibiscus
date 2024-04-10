#include <cstddef>
#include <cstdio>
#include <cstring>
#include <expected>
#include <iostream>
#include <optional>
#include <sys/mman.h>
#include <valgrind/memcheck.h>

#include "chi/panic.h"

// The page size of the system.
constexpr const size_t PAGE_SIZE = 4096;

class Header {
public:
  size_t size = 0;

  // Should the free member be stored here or at the end of the header? The LSP
  // says that both take the same amount of space, so let's keep it here for
  // now.
  bool free = true;

  Header *next = nullptr;
  Header *prev = nullptr;

  Header() = default;

  // Return the address of the data.
  void *data() {
    // The way that pointer arithmetic works is that it depends on the type of
    // the pointer. Here 'this' is a pointer to a Header object, so when we add
    // 1 to it, we are actually adding the size of a 'Header' object.
    return this + 1;
  }
};

// Create a new header from a page.
Header *new_header(void *page) {
  Header *header = static_cast<Header *>(page);

  header->size = PAGE_SIZE - sizeof(Header);
  header->free = true;
  header->next = nullptr;
  header->prev = nullptr;

  return header;
}

Header *list = nullptr;

// Add a page to the end of the list.
void add_block(Header *block) {
  Header *current = list;

  // If the list is empty, then the block is the new head of the list.
  if (current == nullptr) {
    list = block;

    return;
  }

  while (current != nullptr && current->next != nullptr) {
    current = current->next;
  }

  current->next = block;
  block->prev = current;
}

// Returns a free block of memory that is at least the size requested.
Header *find_free(size_t size) {
  Header *current = list;

  while (current != nullptr) {
    if (current->free && current->size >= size) {
      return current;
    }

    current = current->next;
  }

  return nullptr;
}

// Returns the header of the block that is associated with the given data
// pointer.
Header *find_header(void *ptr) {
  Header *current = list;

  while (current != nullptr) {
    if (current->data() == ptr) {
      return current;
    }

    current = current->next;
  }

  return nullptr;
}

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

// Allocate memory at the memory management level.
void *allocate(size_t size) {
  // It is impossible to allocate a page of memory that is zero bytes or larger
  // than the page size (for now).
  if (size == 0 || size > PAGE_SIZE - sizeof(Header)) {
    return nullptr;
  }

  // Look for a free block of memory.
  Header *block = find_free(size);

  if (block != nullptr) {
    // Mark the block as used.
    block->free = false;

    // Return the "usable" part of the block.
    return block->data();
  }

  // If we cannot find a free block, then we need to allocate a new page and
  // carve out a block from it to return to the user.
  std::optional<void *> page = allocate_page();

  // If we cannot allocate a new page, then there is nothing we can do.
  if (!page.has_value()) {
    return nullptr;
  }

  // For now, let's just use the entire page.
  block = new_header(page.value());

  // Add the page to the global list.
  add_block(block);

  // Mark the block as used and return it to the user.
  block->free = false;

  return block->data();
}

// Free memory at the memory management level.
void deallocate(void *ptr) {
  // Make sure that the pointer can actually be deallocated.
  if (ptr == nullptr) {
    return;
  }

  // Find the corresponding header of the given pointer.
  Header *header = find_header(ptr);

  // We couldn't find the header, so we cannot deallocate the memory.
  if (header == nullptr) {
    return;
  }

  // Mark the block as free.
  header->free = true;

  std::cout << header->size << std::endl;

  // As a safety measure, let's zero out the memory.
  std::memset(ptr, 0, header->size);
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
