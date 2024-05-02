#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>
#include <valgrind/memcheck.h>

#include "chi/assert.h"
#include "chi/page.h"

#include "block.h"
#include "freelist.h"
#include "hibiscus.h"

// TODO: Consider extracting the common code between `split_page` and
//       `split_block` into a helper function.

// Split a page into two free blocks, one with data size `size` and the other
// with the remaining size.
std::pair<hibiscus::Block *, hibiscus::Block *> split_page(void *page,
                                                           const size_t size) {
#ifdef DEBUG
  chi::assert(page != nullptr, "page must not be null");
  chi::assert(0 < size && size <= chi::page::PAGE_SIZE,
              "size must be between (0, PAGE_SIZE]");
#endif

  const size_t left_size = sizeof(hibiscus::Block) + size;

  // Since we are splitting a page, we can override the headers as we wish.
  hibiscus::Block *const left = static_cast<hibiscus::Block *>(page)
                                    ->size(size)
                                    ->free()
                                    ->page(static_cast<hibiscus::Block *>(page))
                                    ->next(nullptr)
                                    ->prev(nullptr);

  // If we can't make a right block, we'll need to extend the left block.
  if (left_size + sizeof(hibiscus::Block) + 1 > chi::page::PAGE_SIZE) {
    // FIXME: I don't like how I'm directly modifying the header's member here.
    left->size_ = chi::page::PAGE_SIZE - sizeof(hibiscus::Block);

    return std::make_pair(left, nullptr);
  }

  const size_t right_size =
      chi::page::PAGE_SIZE - left_size - sizeof(hibiscus::Block);

  // FIXME: Avoid using 'reinterpret_cast' here.
  hibiscus::Block *const right = reinterpret_cast<hibiscus::Block *>(
                                     static_cast<std::byte *>(page) + left_size)
                                     ->size(right_size)
                                     ->free()
                                     ->page(left)
                                     ->next(nullptr)
                                     ->prev(left);

  // FIXME: Again, I don't like how I'm setting 'size_' here.
  // Stitch the left block with the right block.
  left->next_ = right;

  return std::make_pair(left, right);
}

// Split a block into two free blocks, one with data size `size` and the other
// with the remaining size.
std::pair<hibiscus::Block *, hibiscus::Block *>
split_block(hibiscus::Block *block, size_t size) {
  // FIXME: Another instance of directly accessing 'size_'.
  const size_t total_size = sizeof(hibiscus::Block) + block->size_;
  const size_t left_size = sizeof(hibiscus::Block) + size;

#ifdef DEBUG
  chi::assert(block != nullptr, "block must not be null");
  chi::assert(size != 0 && left_size <= total_size,
              "size must be non-zero and the left size must be less than the "
              "total size");
#endif

  // If we can't make space for a right block, we can skip all the hard work.
  if (left_size + sizeof(hibiscus::Block) + 1 > total_size) {
    // FIXME: Member access.
    block->free_ = true;

    return std::make_pair(block, nullptr);
  }

  // We are modifying a pre-existing block, so we should be careful how we
  // modify the header's members.
  hibiscus::Block *const left = block->size(size)->free();

  const size_t right_size = total_size - left_size - sizeof(hibiscus::Block);

  hibiscus::Block *const right =
      reinterpret_cast<hibiscus::Block *>(reinterpret_cast<std::byte *>(block) +
                                          left_size)
          ->size(right_size)
          ->free()
          ->page(left->page_)
          ->next(nullptr)
          ->prev(nullptr);

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

  hibiscus::Block *const next = left->next_;

  left->next_ = right;
  right->prev_ = left;

  if (next != nullptr) {
    right->next_ = next; // This doesn't need to be in here, but I think it's
                         // clearer this way.
    next->prev_ = right;
  }

  return std::make_pair(left, right);
}

namespace hibiscus {
// Allocate a large block of memory.
void *allocate_large(size_t size) {
  const size_t total = sizeof(Block) + size;

#ifdef DEBUG
  chi::assert(total > chi::page::PAGE_SIZE, "size must be larger than a page");
#endif

  void *ptr = chi::page::allocate(total);

#ifdef DEBUG
  chi::assert(ptr != nullptr, "ptr must not be null");
#endif

  Block *block = static_cast<Block *>(ptr)
                     ->size(total)
                     ->used()
                     ->page(static_cast<Block *>(ptr))
                     ->next(nullptr)
                     ->prev(nullptr);

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
  if (sizeof(Block) + size > chi::page::PAGE_SIZE) {
    void *ptr = allocate_large(size);

#ifdef DEBUG
    dll::for_each([](Block *block) { std::cout << *block << "\n"; });

    std::cout << "\n";
#endif

    return ptr;
  }

  // Grab the first block that is free and large enough.
  Block *block = dll::first([size](hibiscus::Block *block) {
    return block->free_ && block->size_ >= size;
  });

  // We found a pre-existing block that is large enough.
  if (block != nullptr) {
    // Try splitting the block into two smaller blocks.
    auto [left, right] = split_block(block, size);

    // Mark the left block as used.
    left->free_ = false;

#ifdef DEBUG
    dll::for_each([](Block *block) { std::cout << *block << "\n"; });

    std::cout << "\n";
#endif

    return left->data();
  }

  void *ptr = chi::page::allocate(chi::page::PAGE_SIZE);
  auto [left, right] = split_page(ptr, size);

  // Mark the left block as used.
  left->free_ = false;

  // Since this is a new run, we'll need to add it to the free list.
  dll::push_back(left);

#ifdef DEBUG
  dll::for_each([](Block *block) { std::cout << *block << "\n"; });

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
  header->free_ = true;

  // +----------+--------+------+
  // | Previous | Header | Next |
  // +----------+--------+------+

  Block *previous = header->prev_;
  Block *next = header->next_;

  // Coalesce the block with the previous and next blocks if they are free and
  // they are part of the same run (i.e. they point to the same page).
  if (previous != nullptr && previous->free_ &&
      previous->page_ == header->page_) {
    previous->size_ += sizeof(Block) + header->size_;
    previous->next_ = next;

    if (next != nullptr) {
      next->prev_ = previous;
    }

    header = previous;
  }

  // +-------------------+------+
  // | Previous + Header | Next |
  // +-------------------+------+

  if (next != nullptr && next->free_ && next->page_ == header->page_) {
    header->size_ += sizeof(Block) + next->size_;
    header->next_ = next->next_;

    if (next->next_ != nullptr) {
      next->next_->prev_ = header;
    }

    // Stop using 'next'.
    next = nullptr;
  }

  // In case we coalesced the block with the previous and/or next blocks, we
  // need to update the previous and next pointers.
  previous = header->prev_;
  next = header->next_;

  // If the previous block is a null pointer or it's from a different run and
  // the next block is a null pointer or it's from a different run, then we can
  // release the page back to the system.
  if ((previous == nullptr || previous->page_ != header->page_) &&
      (next == nullptr || next->page_ != header->page_)) {
    chi::assert(header == header->page_, "header must be the page");

    // Before we release the page back to the system, we need to remove the run
    // from the free list.
    dll::remove(header);

    // FIXME: If the pointer that we are freeing is a large allocation, we need
    //        to release all the pages that it occupies instead of just one.
    chi::assert(chi::page::free(header), "failed to free page");
  } else {
    // Zero out the memory for safety reasons.
    header->zero();
  }

#ifdef DEBUG
  dll::for_each([](Block *block) { std::cout << *block << "\n"; });

  std::cout << "\n";
#endif
}
} // namespace hibiscus
