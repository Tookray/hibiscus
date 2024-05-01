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
  size_t size;

  // To keep it simple, let's store all the blocks (both free and allocated) in
  // a list. This way we can easily coalesce adjacent free blocks (if possible)
  // to reduce fragmentation.
  bool free;

  // The page that this block belongs to.
  Block *page;

  Block *next;
  Block *prev;

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
  void zero() { std::memset(data(), 0, size); }

  friend std::ostream &operator<<(std::ostream &os, Block &block) {
    return os << std::format(
               "Block(size={}, free={}, page={}, next={}, prev={})", block.size,
               block.free, static_cast<void *>(block.page),
               static_cast<void *>(block.next),
               static_cast<void *>(block.prev));
  }
};

// Zero initialize a block.
Block *make_block(void *ptr);
} // namespace hibiscus
