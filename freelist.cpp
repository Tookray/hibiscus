#include <cstddef>
#include <iostream>

#include "chi/assert.h"
#include "chi/page.h"
#include "chi/panic.h"

#include "block.h"
#include "freelist.h"
#include "list.h"

// Since the memory that we have is a continuous, let's go with the following
// layout for now (we can always change it later):
//
// +-------+-----------+------+
// | Node  | *previous | 0x00 |
// |       | *next     | 0x01 |
// |       | *value    | 0x02 ----+
// +-------+-----------+------+   |
// | Node  | size      | 0x03 <---+
// |       | free      | 0x04 |
// |       | *page     | 0x05 |
// |       | *data     | 0x06 ----+
// +-------+-----------+------+   |
// | Data  |           | 0x07 <---+
// |       |           | ...  |
// + ------+-----------+----- +
//
// For the above layout, the value and data pointers are unnecessary (since we
// can simply compute where the block and data are), but I think this will make
// it easier to change the layout down the line.

bool same_address(void *lhs, void *rhs) {
        return lhs == rhs;
}

Node<Block *> *make_node_from_page(void *page) {
        // NOTE: Perhaps we should avoid using C-style casts.
        Node<Block *> *node = (Node<Block *> *)page;
        Block *block = (Block *)((std::byte *)page + sizeof(Node<Block *>));
        void *data = (void *)((std::byte *)page + sizeof(Node<Block *>) + sizeof(Block));

        node->previous = nullptr;
        node->next = nullptr;
        node->value = block;

        block->size = chi::page::PAGE_SIZE - sizeof(Node<Block *>) - sizeof(Block);
        block->free = true;
        block->page = page;
        block->data = data;

        return node;
}

// NOTE: The split function modifies the current node's block but not the node
//       itself.
Node<Block *> *split_node(Node<Block *> *node, size_t requested_size) {
        Block *block = node->value;

        chi::assert(block != nullptr, "Block is null");
        chi::assert(block->free, "Block is not free");
        chi::assert(block->size >= requested_size, "Block is too small");

        // Hold at least 1 byte of data.
        size_t const minimum_size = sizeof(Node<Block *>) + sizeof(Block) + 1;

        if (block->size < minimum_size) {
                return nullptr;
        }

        // NOTE: Again, probably should avoid using C-style casts.
        Node<Block *> *remaining =
            (Node<Block *> *)((std::byte *)node + sizeof(Node<Block *>) + sizeof(Block) + requested_size);
        Block *remaining_block = (Block *)((std::byte *)remaining + sizeof(Node<Block *>));
        void *remaining_data = (void *)((std::byte *)remaining + sizeof(Node<Block *>) + sizeof(Block));

        remaining->previous = nullptr;
        remaining->next = nullptr;
        remaining->value = remaining_block;

        remaining_block->size = block->size - sizeof(Node<Block *>) - sizeof(Block) - requested_size;
        remaining_block->free = true;
        remaining_block->page = block->page;
        remaining_block->data = remaining_data;

        block->size = requested_size;

        return remaining;
}

void FreeListAllocator::initialize() {
        // Do nothing.
}

void *FreeListAllocator::allocate(size_t size) {
        if (size == 0) {
                return nullptr;
        }

        size_t const total = sizeof(Node<Block *>) + sizeof(Block) + size;

        if (total > chi::page::PAGE_SIZE) {
                // TODO: Implement support for large allocations.
                chi::panic("Large allocations are not supported");
        }

        std::cout << "Searching for a block\n";

        // Search for a satisfying block.
        Node<Block *> *current = blocks.front();

        while (current != nullptr) {
                Block *block = current->value;
                chi::assert(block != nullptr, "Block is null");

                if (block->free && block->size >= size) {
                        std::cout << "Found a block\n";

                        Node<Block *> *remaining = split_node(current, size);

                        if (remaining != nullptr) {
                                blocks.insert_after(current, remaining);
                        }

                        current->value->free = false;

                        std::cout << "Returning pointer to data\n";
                        return current->value->data;
                }

                current = current->next;
        }

        std::cout << "Allocating a new page\n";
        void *page = chi::page::allocate(chi::page::PAGE_SIZE);

        if (page == nullptr) {
                chi::panic("MMAP failed");
        }

        std::cout << "Creating a new block from page\n";
        Node<Block *> *node = make_node_from_page(page);
        chi::assert(node != nullptr, "Node is null");

        blocks.push_back(node);

        Node<Block *> *remaining = split_node(node, size);

        if (remaining != nullptr) {
                blocks.push_back(remaining);
        }

        node->value->free = false;

        std::cout << "Returning pointer to data\n";
        return node->value->data;
}

void FreeListAllocator::free(void *ptr) {
        if (ptr == nullptr) {
                return;
        }

        std::cout << "Searching for matching block\n";
        Node<Block *> *current = blocks.front();

        while (current != nullptr) {
                Block *block = current->value;
                chi::assert(block != nullptr, "Block is null");

                if (block->data == ptr) {
                        break;
                }

                current = current->next;
        }

        if (current == nullptr) {
                // NOTE: Unless the caller is trying to free a block that was not allocated
                //       from this allocator, we should never reach this point.
                chi::panic("Block not found");
        }

        std::cout << "Matching block found\n";

        Block *current_block = current->value;
        chi::assert(!current_block->free, "Block should be in use");

        current_block->free = true;

        // We can coalesce the block with the previous and next blocks if they
        // are both free and belong to the same page.

        std::cout << "Trying to coalesce with adjacent blocks\n";

        Node<Block *> *previous = current->previous;

        if (previous != nullptr) {
                Block *previous_block = previous->value;

                if (previous_block->free && previous_block->page == current_block->page) {
                        std::cout << "Coalescing with previous block\n";
                        chi::assert(same_address(static_cast<std::byte *>(previous_block->data) +
                                                     sizeof(Node<Block *>) + sizeof(Block) + previous_block->size,
                                                 current_block->data),
                                    "Previous and current blocks are not adjacent");

                        previous_block->size += sizeof(Node<Block *>) + sizeof(Block) + current_block->size;
                        blocks.remove(current);

                        current = previous;
                        current_block = previous_block;
                }
        }

        Node<Block *> *next = current->next;

        if (next != nullptr) {
                Block *next_block = next->value;

                if (next_block->free && next_block->page == current_block->page) {
                        std::cout << "Coalescing with next block\n";
                        chi::assert(same_address(static_cast<std::byte *>(current_block->data) + sizeof(Node<Block *>) +
                                                     sizeof(Block) + current_block->size,
                                                 next_block->data),
                                    "Current and next blocks are not adjacent");

                        current_block->size += sizeof(Node<Block *>) + sizeof(Block) + next_block->size;
                        blocks.remove(next);
                }
        }

        // If the entire page is free, we can deallocate it.
        if (sizeof(Node<Block *>) + sizeof(Block) + current_block->size == chi::page::PAGE_SIZE) {
                void *page = current_block->page;

                blocks.remove(current);

                std::cout << "Deallocating page\n";
                chi::assert(chi::page::free(page), "MUNMAP failed");
        }
}
