#include <cassert>

#include "block.h"

namespace hibiscus {
Block *make_block(void *ptr) {
  assert(ptr != nullptr);

  Block *block = reinterpret_cast<Block *>(ptr);

  block->size = 0;
  block->free = false;
  block->page = nullptr;
  block->next = nullptr;
  block->prev = nullptr;

  return block;
}
} // namespace hibiscus
