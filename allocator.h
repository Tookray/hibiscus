#pragma once

#include <cstddef>

class Allocator {
protected:
        // Add members here.

public:
        Allocator() = default;
        virtual ~Allocator() = default;

        virtual void initialize() = 0;
        virtual void *allocate(size_t size) = 0;
        virtual void free(void *ptr) = 0;
};
