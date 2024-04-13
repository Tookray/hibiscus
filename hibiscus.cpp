#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>
#include <valgrind/memcheck.h>

#include "chi/panic.h"

#include "block.h"
#include "freelist.h"
#include "hibiscus.h"

// The page size of the system.
const size_t PAGE_SIZE = sysconf(_SC_PAGESIZE);

// TODO: Consider extracting the common code between `split_page` and
//       `split_block` into a helper function.

// Split a page into two free blocks, one with data size `size` and the other
// with the remaining size.
std::pair<hibiscus::Block *, hibiscus::Block *> split_page(void *page,
                                                           size_t size) {
  // +-----------------------------+
  // |            total            |
  // +--------------+--------------+
  // |     left     |    right     |
  // +--------------+--------------+
  // | Block | size | Block | Data |
  // +--------------+--------------+

  const size_t left_size = sizeof(hibiscus::Block) + size;

  // Validate the arguments.
  assert(page != nullptr);
  assert(size != 0 && left_size <= PAGE_SIZE);

  // Since we are splitting a page, we can override the headers as we wish.
  hibiscus::Block *const left = hibiscus::make_block(page);

  left->size = size;
  left->free = true;
  left->page = left;

  // If we can't make a right block, we'll need to extend the left block.
  if (left_size + sizeof(hibiscus::Block) + 1 > PAGE_SIZE) {
    left->size = PAGE_SIZE - sizeof(hibiscus::Block);

    return std::make_pair(left, nullptr);
  }

  const size_t right_size = PAGE_SIZE - left_size - sizeof(hibiscus::Block);
  hibiscus::Block *const right =
      hibiscus::make_block(static_cast<std::byte *>(page) + left_size);

  right->size = right_size;
  right->free = true;
  right->page = left;

  // Link the two blocks together.
  left->next = right;
  right->prev = left;

  return std::make_pair(left, right);
}

// Split a block into two free blocks, one with data size `size` and the other
// with the remaining size.
std::pair<hibiscus::Block *, hibiscus::Block *>
split_block(hibiscus::Block *block, size_t size) {
  // +-----------------------------+
  // |            total            |
  // +--------------+--------------+
  // |     left     |    right     |
  // +--------------+--------------+
  // | Block | size | Block | Data |
  // +--------------+--------------+

  const size_t total_size = sizeof(hibiscus::Block) + block->size;
  const size_t left_size = sizeof(hibiscus::Block) + size;

  // Validate the arguments.
  assert(block != nullptr);
  assert(size != 0 && left_size <= total_size);

  // If we can't make space for a right block, we can skip all the hard work.
  if (left_size + sizeof(hibiscus::Block) + 1 > total_size) {
    block->free = true;

    return std::make_pair(block, nullptr);
  }

  // We are modifying a pre-existing block, so we should be careful how we
  // modify the header's members.
  hibiscus::Block *const left = block;

  left->size = size;
  left->free = true;

  const size_t right_size = total_size - left_size - sizeof(hibiscus::Block);
  hibiscus::Block *const right =
      hibiscus::make_block(reinterpret_cast<std::byte *>(block) + left_size);

  right->size = right_size;
  right->free = true;
  right->page = left->page;

  // We'll need to go from:
  //
  // +----------+--------------+------+
  // | Previous |     Left     | Next |
  // +----------+--------------+------+
  //
  // to:
  //
  // +----------+------+-------+------+
  // | Previous | Left | Right | Next |
  // +----------+------+-------+------+

  hibiscus::Block *const next = left->next;

  left->next = right;
  right->prev = left;

  if (next != nullptr) {
    right->next = next; // This doesn't need to be in here, but I think it's
                        // clearer this way.
    next->prev = right;
  }

  return std::make_pair(left, right);
}

// Get memory from the system.
std::pair<void *, size_t> alloc(size_t size) {
  if (size == 0) {
    return std::make_pair(nullptr, 0);
  }

  // Request some multiple of the page size from the system.
  // i.e. we want size < n * PAGE_SIZE => n = ceil(size / PAGE_SIZE)
  size_t total = static_cast<size_t>(ceil(size / PAGE_SIZE)) * PAGE_SIZE;
  assert(total % PAGE_SIZE == 0);

  // NOTE: Consider using MAP_HUGETLB (and MAP_HUGE_2MB, MAP_HUGE_1GB) for
  //       large allocations.
  void *ptr = mmap(nullptr, total, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (ptr == MAP_FAILED) {
    // The system didn't give us a page of memory, let's abort.
    chi::panic("Failed to allocate memory from the system");
  }

  // Valgrind and friends do not know about the memory we just allocated, so
  // we need to tell them about it.
  VALGRIND_MALLOCLIKE_BLOCK(ptr, total, 0, 0);

#ifdef DEBUG
  std::cout << "Allocated " << total << " bytes at " << ptr << "\n\n";
#endif

  return std::make_pair(ptr, total);
}

// Grab a page from the system.
std::pair<void *, size_t> page_alloc() { return alloc(PAGE_SIZE); }

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

#ifdef DEBUG
  std::cout << "Freed " << PAGE_SIZE << " bytes at " << ptr << "\n\n";
#endif
}

namespace hibiscus {
// Allocate a large block of memory.
void *allocate_large(size_t size) {
  assert(sizeof(Block) + size > PAGE_SIZE);

  auto [ptr, total] = alloc(sizeof(Block) + size);

  if (ptr == nullptr) {
    return nullptr;
  }

  Block *block = make_block(ptr);

  block->size = total;
  block->page = block;

  // Add the block to the free list to keep track of it.
  dll::push_back(block);

  return block->data();
}

void *allocate(size_t size) {
  // Invalid size requested, let's return a null pointer.
  if (size == 0) {
    return nullptr;
  }

  // Requested allocation is larger than a page.
  if (sizeof(Block) + size > PAGE_SIZE) {
    void *ptr = allocate_large(size);

#ifdef DEBUG
    dll::for_each(
        [](Block *block) { std::cout << block->to_string() << std::endl; });

    std::cout << std::endl;
#endif

    return ptr;
  }

  // Grab the first block that is free and large enough.
  Block *block = dll::first([size](hibiscus::Block *block) {
    return block->free && block->size >= size;
  });

  // We found a pre-existing block that is large enough.
  if (block != nullptr) {
    // Try splitting the block into two smaller blocks.
    auto [left, right] = split_block(block, size);

    // Mark the left block as used.
    left->free = false;

#ifdef DEBUG
    dll::for_each(
        [](Block *block) { std::cout << block->to_string() << std::endl; });

    std::cout << std::endl;
#endif

    return left->data();
  }

  auto [ptr, total] = page_alloc();
  auto [left, right] = split_page(ptr, size);

  // Mark the left block as used.
  left->free = false;

  // Since this is a new run, we'll need to add it to the free list.
  dll::push_back(left);

#ifdef DEBUG
  dll::for_each(
      [](Block *block) { std::cout << block->to_string() << std::endl; });

  std::cout << std::endl;
#endif

  return left->data();
}

void free(void *ptr) {
  if (ptr == nullptr) {
    return;
  }

  Block *header = static_cast<Block *>(ptr) - 1;

  // Mark the block as free.
  header->free = true;

  // +----------+--------+------+
  // | Previous | Header | Next |
  // +----------+--------+------+

  Block *previous = header->prev;
  Block *next = header->next;

  // Coalesce the block with the previous and next blocks if they are free and
  // they are part of the same run (i.e. they point to the same page).
  if (previous != nullptr && previous->free && previous->page == header->page) {
    previous->size += sizeof(Block) + header->size;
    previous->next = next;

    if (next != nullptr) {
      next->prev = previous;
    }

    header = previous;
  }

  // +-------------------+------+
  // | Previous + Header | Next |
  // +-------------------+------+

  if (next != nullptr && next->free && next->page == header->page) {
    header->size += sizeof(Block) + next->size;
    header->next = next->next;

    if (next->next != nullptr) {
      next->next->prev = header;
    }

    // Stop using 'next'.
    next = nullptr;
  }

  // In case we coalesced the block with the previous and/or next blocks, we
  // need to update the previous and next pointers.
  previous = header->prev;
  next = header->next;

  // If the previous block is a null pointer or it's from a different run and
  // the next block is a null pointer or it's from a different run, then we can
  // release the page back to the system.
  if ((previous == nullptr || previous->page != header->page) &&
      (next == nullptr || next->page != header->page)) {
    assert(header == header->page);

    // Before we release the page back to the system, we need to remove the run
    // from the free list.
    dll::remove(header);

    page_free(header);
  } else {
    // Zero out the memory for safety reasons.
    header->zero();
  }

#ifdef DEBUG
  dll::for_each(
      [](Block *block) { std::cout << block->to_string() << std::endl; });

  std::cout << std::endl;
#endif
}
} // namespace hibiscus
