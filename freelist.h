#pragma once

#include <cstddef>
#include <functional>

#include "allocator.h"
#include "block.h"
#include "list.h"

class FreeListAllocator : public Allocator {
private:
        List<Block *> blocks;

public:
        FreeListAllocator() = default;
        ~FreeListAllocator() override = default;

        void initialize() override;
        void *allocate(size_t size) override;
        void free(void *ptr) override;
};
