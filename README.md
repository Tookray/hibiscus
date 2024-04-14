# Hibiscus

>A little memory allocator that could.

## Roadmap

- [x] Free list
- [ ] Slab allocator

## Technical Details

Initially I tried to use `std::optional` and `std::expected` as much as
possible because I liked Rust's `Option` and `Result` types, but I found that
it was too cumbersome to use them in C++. I think it was a combination of:

1. No pattern matching
2. No variable shadowing in the same scope
3. Too much boilerplate

I ended up removing all of them because they got in the way instead of helping.

```cpp
if (auto result = fn(); result /* .has_value() */) {
  // Use result.value()
} else {
  // Handle error or lack of value
}
```

### Free List

I shamelessly copied parts of Rust's `std::collections::LinkedList` API.
