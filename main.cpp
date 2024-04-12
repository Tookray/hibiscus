#include <iostream>

#include "chi/panic.h"

#include "hibiscus.h"

int main() {
  int *ptr = static_cast<int *>(hibiscus::allocate(sizeof(int)));

  if (ptr == nullptr) {
    chi::panic("Failed to allocate memory");
  }

  *ptr = 42;

  std::cout << "The value of ptr is " << *ptr << std::endl;

  hibiscus::free(ptr);
}
