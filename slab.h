#pragma once

#include <cstddef>
#include <functional>
#include <string>

// Inspired by Jeff Bonwick's paper:
//
// "The Slab Allocator: An Object-Caching Kernel Memory Allocator"
//
// Back end                        Front end
// --------                        ---------
// kmem_cache_grow -> +-------+ -> kmem_cache_alloc
//                    | cache |
// kmem_cache_reap <- +-------+ <- kmem_cache_free
//
// - kmem_cache_grow: Gets memory from the VM system, makes objects out of it,
//                    and feeds those objects into the cache
// - kmem_cache_reap: Invoked by the VM system when it wants some of that
//                    memory back
// - kmem_alloc: Simply performs a kmem_cache_alloc from the nearest-size cache
//   - Allocations larger than 9K are handled directly by the back-end page
//     supplier

// The primary unit of currency in the slab allocator. When the allocator needs
// to grow a cache, for example, it acquires an entire slab of objects at once.
// Similarly, the allocator reclaims unused memory (shrinks a cache) by
// relinquishing a complete slab.
//
// A slab consists of one or more pages of virually contiguous memory carved up
// into equal-size chunks, with a reference count indicating how many of those
// chunks have been allocated.
class Slab {
};

// If I'm understanding the structure of the slab allocator correctly...
//
// +-------+    +-------+    +-------+
// | Cache | -> | Cache | -> | Cache |
// +-------+    +-------+    +-------+
//     |
//     v
//  +------+
//  | Slab |
//  +------+
//     |
//     v
//  +------+
//  | Page |
//  +------+
//     |
//     v
//  +--------+
//  | Object |
//  +--------+

class Statistics {
public:
  // TODO:
  // - [ ] Total allocations
  // - [ ] Allocated objects
  // - [ ] Freed objects
};

// At startup, the system creates a set of about 30 caches ranging in size from
// 8 bytes to 9K in roughly 10-20% increments.

// I don't think we should use a template here, the constructor and destructor
// functions is given by the user so we don't need to know the type of the
// object. On the contrary, using a template would just make our binary bigger.
class Cache {
public:
  // NOTE: Debug related members.
  std::string name;

  // It says in the paper that:
  //
  // `name` identifies the cache for statistics and debugging.
  //
  // Does this mean that we should store the statistics in the cache or should
  // we have a separate object for that?
  Statistics stats;

  size_t size;

  // Alignment boundary.
  size_t alignment;

  // Object constructor and destructor.
  std::function<void(void *)> constructor;
  std::function<void(void *)> destructor;

  // Don't use the empty constructor.
  Cache() = delete;
};

// Creates a cache of objects, each with size `size` and aligned on an
// `alignment` boundary. The alignment will always be rounded up to the minimum
// allowable value, so `alignment` can be zero whenever no special alignment is
// required.
Cache* cache_create(const std::string &name, size_t size, size_t alignment,
                   std::function<void(void *)> constructor,
                   std::function<void(void *)> destructor);

enum class AllocationFlag {
  // Acceptable to wait for memory if none is currently available.
  sleep,

  // Not acceptable to wait for memory if none is currently available.
  nosleep,
};

// User interface to the slab allocator ---------------------------------------

// Gets an object from the cache. The object will be in its constructed state.
void *cache_alloc(Cache *cache, AllocationFlag flag);

// Returns an object to the cache. The object must still be in its constructed
// state.
void cache_free(Cache *cache, void *object);

// ----------------------------------------------------------------------------

// Destroys the cache and reclaims all associated resources. All allocated
// objects must have been returned to the cache.
void cache_destroy(Cache *cache);

// Gets memory from the VM system, makes objects out of it, and feeds those
// objects into the cache.
void cache_grow();

// Invoked by the VM system when it wants some of that memory back.
void cache_reap();
