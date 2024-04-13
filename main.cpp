#include <iostream>

#include "chi/panic.h"

#include "hibiscus.h"

int main() {
  for (int i = 0; i < 10; ++i) {
    int *ptr = static_cast<int *>(hibiscus::allocate(500));

    if (ptr == nullptr) {
      chi::panic("Failed to allocate memory!");
    }

    *ptr = i;

    std::cout << *ptr << std::endl;

    hibiscus::free(ptr);
  }
}
