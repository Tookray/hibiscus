#include <cstddef>

#include "freelist.h"
#include "hibiscus.h"

namespace hibiscus {

// Where should I initialize this?
FreeListAllocator freelist;

void *allocate(size_t size) {
        return freelist.allocate(size);
}

void free(void *ptr) {
        freelist.free(ptr);
}

} // namespace hibiscus
