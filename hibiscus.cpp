#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>
#include <valgrind/memcheck.h>

#include "chi/assert.h"

#include "block.h"
#include "freelist.h"
#include "hibiscus.h"
#include "page.h"

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
  chi::assert(page != nullptr, "page must not be null");
  chi::assert(size != 0 && left_size <= page::PAGE_SIZE,
              "size must be non-zero and less than the page size");

  // Since we are splitting a page, we can override the headers as we wish.
  hibiscus::Block *const left = hibiscus::make_block(page);

  left->size = size;
  left->free = true;
  left->page = left;

  // If we can't make a right block, we'll need to extend the left block.
  if (left_size + sizeof(hibiscus::Block) + 1 > page::PAGE_SIZE) {
    left->size = page::PAGE_SIZE - sizeof(hibiscus::Block);

    return std::make_pair(left, nullptr);
  }

  const size_t right_size =
      page::PAGE_SIZE - left_size - sizeof(hibiscus::Block);
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
  chi::assert(block != nullptr, "block must not be null");
  chi::assert(size != 0 && left_size <= total_size,
              "size must be non-zero and less than the total size");

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

namespace hibiscus {
// Allocate a large block of memory.
void *allocate_large(size_t size) {
  const size_t total = sizeof(Block) + size;

#ifdef DEBUG
  chi::assert(total > page::PAGE_SIZE, "size must be larger than a page");
#endif

  void *ptr = page::allocate(total);

#ifdef DEBUG
  chi::assert(ptr != nullptr, "ptr must not be null");
#endif

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
  if (sizeof(Block) + size > page::PAGE_SIZE) {
    void *ptr = allocate_large(size);

#ifdef DEBUG
    dll::for_each(
        [](Block *block) { std::cout << block->to_string() << "\n"; });

    std::cout << "\n";
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

  void *ptr = page::allocate(page::PAGE_SIZE);
  auto [left, right] = split_page(ptr, size);

  // Mark the left block as used.
  left->free = false;

  // Since this is a new run, we'll need to add it to the free list.
  dll::push_back(left);

#ifdef DEBUG
  dll::for_each([](Block *block) { std::cout << block->to_string() << "\n"; });

  std::cout << "\n";
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
    chi::assert(header == header->page, "header must be the page");

    // Before we release the page back to the system, we need to remove the run
    // from the free list.
    dll::remove(header);

    // FIXME: If the pointer that we are freeing is a large allocation, we need
    //        to release all the pages that it occupies instead of just one.
    page::free(header);
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
