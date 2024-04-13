#include <cstddef>
#include <vector>

#include "chi/panic.h"

#include "hibiscus.h"

constexpr const size_t COUNT = 3;
constexpr const size_t DATA_SIZE = 2000;

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
