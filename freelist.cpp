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

  if (current->prev_ != nullptr) {
    chi::panic(
        "The head of the linked list should have a null previous pointer!");
  }

  while (current != nullptr) {
    if (current->size_ == 0) {
      chi::panic("The size of the block should not be zero!");
    }

    if (current->next_ != nullptr && current->next_->prev_ != current) {
      chi::panic(
          "If the current block is not the last block in the list, then the "
          "next block's previous pointer should be the current block!");
    }

    current = current->next_;
  }
}

namespace hibiscus::dll {
Block *list = nullptr;

void append(Block *block) {
#ifdef DEBUG
  check(list);
  check(block);
#endif

  assert(block != nullptr);
  assert(block->size_ != 0);

  Block *tail = back();

  if (tail != nullptr) {
    tail->next_ = block;
    block->prev_ = tail;
  } else {
    list = block;
  }

#ifdef DEBUG
  check(list);
#endif
}

Block *back() {
  if (list == nullptr) {
    return nullptr;
  }

  Block *current = list;

  while (current != nullptr && current->next_ != nullptr) {
    current = current->next_;
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

    current = current->next_;
  }

  return nullptr;
}

void for_each(std::function<void(Block *)> callback) {
  Block *current = list;

  while (current != nullptr) {
    callback(current);

    current = current->next_;
  }
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

    current = current->next_;
  }

  return length;
}

Block *pop_back() {
  assert(list != nullptr);

#ifdef DEBUG
  check(list);
#endif

  Block *tail = back();

  if (tail->prev_ != nullptr) {
    tail->prev_->next_ = nullptr;
  }

  // Detach the node from the list.
  tail->prev_ = nullptr;

#ifdef DEBUG
  check(list);
  check(tail);
#endif

  return tail;
}

Block *pop_front() {
  assert(list != nullptr);

#ifdef DEBUG
  check(list);
#endif

  Block *head = list;

  if (head->next_ != nullptr) {
    head->next_->prev_ = nullptr;

    list = head->next_;
  } else {
    list = nullptr;
  }

  head->next_ = nullptr;

#ifdef DEBUG
  check(list);
  check(head);
#endif

  return head;
}

void push_back(Block *block) {
  // This function is the same as calling 'append'.
  append(block);
}

void push_front(Block *block) {
#ifdef DEBUG
  check(block);
  check(list);
#endif

  assert(block != nullptr);
  assert(block->size_ != 0);

  if (list == nullptr) {
    list = block;
  } else {
    // Make sure that the block's previous pointer is a null pointer since it
    // is the new head of the list.
    list->prev_ = block;

    // Link the block to the current head of the list.
    block->next_ = list;

    // Update the head of the list.
    list = block;
  }

#ifdef DEBUG
  check(list);
#endif
}

void remove(Block *block) {
  assert(block != nullptr);

#ifdef DEBUG
  check(list);

  // Make sure that the block is in the list.
  if (!contains(block)) {
    chi::panic("Why are you trying to remove a block that is not in the list?");
  }
#endif

  if (block->prev_ != nullptr) {
    block->prev_->next_ = block->next_;
  }

  if (block->next_ != nullptr) {
    block->next_->prev_ = block->prev_;
  }

  if (block == list) {
    list = block->next_;
  }

  block->next_ = nullptr;
  block->prev_ = nullptr;

#ifdef DEBUG
  check(list);
  check(block);
#endif
}
} // namespace hibiscus::dll
