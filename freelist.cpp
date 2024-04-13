#include <cassert>
#include <functional>

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

void append(Block *block) {
#ifdef DEBUG
  check(block);
#endif

  assert(block != nullptr);
  assert(block->size != 0);

  Block *tail = back();

  if (tail != nullptr) {
    tail->next = block;
    block->prev = tail;
  } else {
    list = block;
  }
}

Block *back() {
  if (list == nullptr) {
    return nullptr;
  }

  Block *current = list;

  while (current != nullptr && current->next != nullptr) {
    current = current->next;
  }

  return current;
}

void clear() { chi::panic("Why are you trying to clear the free list?"); }

bool contains(Block *block) {
  return first([block](Block *current) { return current == block; }) != nullptr;
}

Block *first(std::function<bool(Block *)> predicate) {
  Block *current = list;

  while (current != nullptr) {
    if (predicate(current)) {
      return current;
    }

    current = current->next;
  }

  return nullptr;
}

Block *front() {
  if (list == nullptr) {
    return nullptr;
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

Block *pop_back() {
  if (list == nullptr) {
    return nullptr;
  }

  Block *tail = back();

  if (tail->prev != nullptr) {
    tail->prev->next = nullptr;
  }

  // Detach the node from the list.
  tail->prev = nullptr;

  return tail;
}

Block *pop_front() {
  if (list == nullptr) {
    return nullptr;
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

void push_back(Block *block) {
  // This function is the same as calling 'append'.
  return append(block);
}

void push_front(Block *block) {
#ifdef DEBUG
  check(block);
#endif

  assert(block != nullptr);
  assert(block->size != 0);

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
}

void remove(Block *block) {
  assert(block != nullptr);

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
}
} // namespace hibiscus::dll
