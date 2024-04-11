#pragma once

#include <cstddef>
#include <cstring>

namespace hibiscus {
class Block {
public:
  size_t size;

  // Since only free blocks are stored in the list, we can avoid adding a
  // Boolean member to the class. This reduces the size of the header by 8
  // bytes (1 + 7 bytes of padding).
  // bool free;

  Block *next;
  Block *prev;

  // No default constructor please!
  Block() = delete;

  // With this, we can do static cast a void pointer to a 'Block' pointer
  // instead of using a reinterpret cast (which is not good practice).
  // form
  Block(void *ptr __attribute__((unused)))
      : size(0), next(nullptr), prev(nullptr) {}

  // Return the address of the data.
  void *data() {
    // The way that pointer arithmetic works is that it depends on the type of
    // the pointer. Here 'this' is a pointer to a 'Block' object, so when we
    // add 1 to it, we are actually adding the size of a 'Block' object.
    return this + 1;
  }

  // Zero out the data.
  void zero() { std::memset(data(), 0, size); }
};
} // namespace hibiscus
