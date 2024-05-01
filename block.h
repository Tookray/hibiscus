#pragma once

#include <cstddef>
#include <cstring>
#include <format>
#include <iostream>
#include <string>

// The memory layout is as follows:
//
// - Page
//   - Run
//     - Block (metadata)
//     - Data  (user data)
//
// A page consists of multiple runs, and a run consists of multiple block-data
// pairs.

namespace hibiscus {

class Block {
public:
  size_t size_;

  // To keep it simple, let's store all the blocks (both free and allocated) in
  // a list. This way we can easily coalesce adjacent free blocks (if possible)
  // to reduce fragmentation.
  bool free_;

  // The page that this block belongs to.
  Block *page_;

  Block *next_;
  Block *prev_;

  // No default constructor please!
  Block() = delete;

  // Return the address of the data.
  void *data() {
    // The way that pointer arithmetic works is that it depends on the type of
    // the pointer. Here 'this' is a pointer to a 'Block' object, so when we
    // add 1 to it, we are actually adding the size of a 'Block' object.
    return this + 1;
  }

  // Zero out the data.
  void zero() { std::memset(data(), 0, size_); }

  friend std::ostream &operator<<(std::ostream &os, Block &block) {
    return os << std::format(
               "Block(size={}, free={}, page={}, next={}, prev={})",
               block.size_, block.free_, static_cast<void *>(block.page_),
               static_cast<void *>(block.next_),
               static_cast<void *>(block.prev_));
  }

  // Let's try using the builder pattern and see how it feels.

  Block *size(const size_t size) {
    this->size_ = size;

    return this;
  }

  Block *free() {
    this->free_ = true;

    return this;
  }

  Block *used() {
    this->free_ = false;

    return this;
  }

  Block *page(Block *page) {
    this->page_ = page;

    return this;
  }

  Block *next(Block *next) {
    this->next_ = next;

    return this;
  }

  Block *prev(Block *prev) {
    this->prev_ = prev;

    return this;
  }
};

} // namespace hibiscus
