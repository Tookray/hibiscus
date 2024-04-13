#pragma once

#include <cstddef>
#include <functional>

#include "block.h"

namespace hibiscus::dll {
// Append another list to the end of the list.
void append(Block *block);

// Get the back block of the list.
Block *back();

// Remove all the blocks from the list.
void clear();

// Check if the list contains a block.
bool contains(Block *block);

// Get the first block that matches the predicate.
Block *first(std::function<bool(Block *)> predicate);

// Iterate over each block in the list and call the callback function for each
// element.
void for_each(std::function<void(Block *)> callback);

// Get the front block of the list.
Block *front();

// Get the length of the list.
size_t len();

// Pop the last block from the list.
Block *pop_back();

// Pop the first block from the list.
Block *pop_front();

// Add a block to the back of the list.
void push_back(Block *block);

// Add a block to the front of the list.
void push_front(Block *block);

// Remove a block from the list.
void remove(Block *block);
} // namespace hibiscus::dll
