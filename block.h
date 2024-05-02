#pragma once

#include <cstddef>
#include <cstring>
#include <format>
#include <iostream>
#include <string>
#include <utility>

#include "chi/assert.h"
#include "chi/panic.h"

// The memory layout is as follows:
//
// - Page
//   - Run
//     - Block (metadata)
//     - Data  (user data)
//
// A page consists of multiple runs, and a run consists of multiple block-data
// pairs.

class Block {
public:
        size_t size;

        // To keep it simple, let's store all the blocks (both free and allocated) in
        // a list. This way we can easily coalesce adjacent free blocks (if possible)
        // to reduce fragmentation.
        bool free;

        // The page that this block belongs to.
        void *page;

        // The start of the data.
        void *data;

        // Zero out the data.
        void zero() const {
                std::memset(data, 0, size);
        }

        friend std::ostream &operator<<(std::ostream &os, Block &block) {
                return os << std::format("Block(size={}, free={}, page={})", block.size, block.free,
                                         static_cast<void *>(block.page));
        }
};
