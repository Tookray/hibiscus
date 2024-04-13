#pragma once

#include <cstddef>
#include <cstring>
#include <format>
#include <string>

namespace hibiscus {
class Block {
public:
  size_t size;

  // To keep it simple, let's store all the blocks (both free and allocated) in
  // a list. This way we can easily coalesce adjacent free blocks (if possible)
  // to reduce fragmentation.
  bool free;

  // +-----------------------------+
  // |            Page             |
  // +--------------+--------------+
  // |     Run      |     Run      |
  // +-------+------+-------+------+
  // | Block | Data | Block | Data |
  // +-------+------+-------+------+

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

  std::string to_string() {
    return std::format("Block(size={}, free={}, page={}, next={}, prev={})",
                       size, free, static_cast<void *>(page),
                       static_cast<void *>(next), static_cast<void *>(prev));
  }
};

// Treats the pointer like a block and zero initializes its members.
Block *make_block(void *ptr);
} // namespace hibiscus
