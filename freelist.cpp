#include <expected>
#include <functional>
#include <optional>

#include "chi/panic.h"

#include "block.h"
#include "freelist.h"

// A santiy check to confirm that the list is in a valid state. This should
// only be called in debug mode.
void check(hibiscus::Block *list) {
  if (list == nullptr) {
    return;
  }

  hibiscus::Block *current = list;

  if (current->prev != nullptr) {
    chi::panic(
        "The head of the linked list should have a null previous pointer!");
  }

  while (current != nullptr) {
    if (current->size == 0) {
      chi::panic("The size of the block should not be zero!");
    }

    if (current->next != nullptr && current->next->prev != current) {
      chi::panic(
          "If the current block is not the last block in the list, then the "
          "next block's previous pointer should be the current block!");
    }

    current = current->next;
  }
}

namespace hibiscus::dll {
Block *list = nullptr;

std::expected<void, Error> append(Block *block) {
#ifdef DEBUG
  if (block == nullptr) {
    chi::panic("Why are you trying to append a null block?");
  }

  check(block);
#endif

  if (block == nullptr) {
    return std::unexpected<Error>(Error::NullBlock);
  }

  if (block->size == 0) {
    return std::unexpected<Error>(Error::ZeroSize);
  }

  if (auto tail = back(); tail.has_value()) {
    tail.value()->next = block;
    block->prev = tail.value();
  } else {
    list = block;
  }

  return {};
}

std::optional<Block *> back() {
  if (list == nullptr) {
    return std::nullopt;
  }

  Block *current = list;

  while (current != nullptr && current->next != nullptr) {
    current = current->next;
  }

  return current;
}

void clear() { chi::panic("Why are you trying to clear the free list?"); }

bool contains(Block *block) {
  Block *current = list;

  while (current != nullptr) {
    if (current == block) {
      return true;
    }

    current = current->next;
  }

  return false;
}

std::optional<Block *> first(std::function<bool(Block *)> predicate) {
  Block *current = list;

  while (current != nullptr) {
    if (predicate(current)) {
      return current;
    }

    current = current->next;
  }

  return std::nullopt;
}

std::optional<Block *> front() {
  if (list == nullptr) {
    return std::nullopt;
  }

  return list;
}

size_t len() {
  size_t length = 0;

  Block *current = list;

  while (current != nullptr) {
    ++length;

    current = current->next;
  }

  return length;
}

std::optional<Block *> pop_back() {
  if (auto tail = back(); tail.has_value()) {
    if (tail.value()->prev != nullptr) {
      tail.value()->prev->next = nullptr;
    }

    tail.value()->prev = nullptr;

    return tail.value();
  }

  return std::nullopt;
}

std::optional<Block *> pop_front() {
  if (list == nullptr) {
    return std::nullopt;
  }

  Block *head = list;

  if (head->next != nullptr) {
    head->next->prev = nullptr;

    list = head->next;
  } else {
    list = nullptr;
  }

  head->next = nullptr;

  return head;
}

std::expected<void, Error> push_back(Block *block) {
  // This function is the same as calling 'append'.
  return append(block);
}

std::expected<void, Error> push_front(Block *block) {
#ifdef DEBUG
  if (block == nullptr) {
    chi::panic("Why are you trying to prepend a null block?");
  }

  check(block);
#endif

  if (block == nullptr) {
    return std::unexpected<Error>(Error::NullBlock);
  }

  if (block->size == 0) {
    return std::unexpected<Error>(Error::ZeroSize);
  }

  if (list == nullptr) {
    list = block;
  } else {
    // Make sure that the block's previous pointer is a null pointer since it
    // is the new head of the list.
    list->prev = block;

    // Link the block to the current head of the list.
    block->next = list;

    // Update the head of the list.
    list = block;
  }

  return {};
}

std::expected<void, Error> remove(Block *block) {
  if (block == nullptr) {
    return std::unexpected<Error>(Error::NullBlock);
  }

#ifdef DEBUG
  // Make sure that the block is in the list.
  if (!contains(block)) {
    chi::panic("Why are you trying to remove a block that is not in the list?");
  }
#endif

  if (block->prev != nullptr) {
    block->prev->next = block->next;
  }

  if (block->next != nullptr) {
    block->next->prev = block->prev;
  }

  if (block == list) {
    list = block->next;
  }

  block->next = nullptr;
  block->prev = nullptr;

  return {};
}
} // namespace hibiscus::dll
