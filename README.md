# flat-unordered-hash-map

WIP Implementation of a flat unordered hash map based on the google's absl swiss table paper. Uses sse2 instructions to speed up lookups. 

Unofficial benchmarking concludes faster inserts, iteration, and lookups compared to std::unordered_map.

# TODO
- Complete c++ container API not implemented.
- Deletions
- Faster hashing implementation
- Further Optimizations

Supported compiler: msvc
