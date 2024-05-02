#include <cstddef>
#include <vector>

#include "chi/panic.h"

#include "hibiscus.h"

constexpr size_t const COUNT = 6;
constexpr size_t const DATA_SIZE = 1000;

int main() {
        std::vector<int *> ptrs = std::vector<int *>(COUNT);

        for (size_t i = 0; i < COUNT; ++i) {
                int *ptr = static_cast<int *>(hibiscus::allocate(DATA_SIZE));

                if (ptr == nullptr) {
                        chi::panic("Failed to allocate memory!");
                }

                ptrs.emplace_back(ptr);
        }

        for (int *ptr : ptrs) {
                hibiscus::free(ptr);
        }
}
