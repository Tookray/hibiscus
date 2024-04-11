#pragma once

#include <cstddef>
#include <expected>
#include <functional>
#include <optional>

#include "block.h"

namespace hibiscus::dll {
enum class Error {
  // Passed a null block to the function.
  NullBlock,

  // Attempted to append a zero-sized block to the list.
  ZeroSize,

  // Attempted to remove a block that is not in the list.
  DoesNotContain,
};

// Append another list to the end of the list.
std::expected<void, Error> append(Block *block);

// Get the back block of the list.
std::optional<Block *> back();

// Remove all the blocks from the list.
void clear();

// Check if the list contains a block.
bool contains(Block *block);

// Get the first block that matches the predicate.
std::optional<Block *> first(std::function<bool(Block *)> predicate);

// Get the front block of the list.
std::optional<Block *> front();

// Get the length of the list.
size_t len();

// Pop the last block from the list.
std::optional<Block *> pop_back();

// Pop the first block from the list.
std::optional<Block *> pop_front();

// Add a block to the back of the list.
std::expected<void, Error> push_back(Block *block);

// Add a block to the front of the list.
std::expected<void, Error> push_front(Block *block);

// Remove a block from the list.
std::expected<void, Error> remove(Block *block);
} // namespace hibiscus::dll
