# enigma

Small C library.

## Build

```bash
cmake -S . -B build -Denigma_BUILD_APP=ON -Denigma_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```
