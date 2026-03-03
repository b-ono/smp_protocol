# Build Directory Convention

**All CMake builds must use out-of-source builds in the `build/` directory.**

## Why

- Keeps repository clean (no build artifacts polluting source)
- Makes it easy to rebuild from scratch
- Prevents accidental commits of generated files

## Usage

```bash
# Build
mkdir -p build && cd build && cmake .. && make && ctest

# Clean rebuild
rm -rf build && mkdir build && cd build && cmake .. && make

# Just configure
cmake .. && make

# Run tests
ctest --output-on-failure
```

## Files Generated in build/

- `CMakeCache.txt`
- `CMakeFiles/`
- `Makefile`
- `compile_commands.json`
- `libsmp.a`
- `minimal_target_example`
- `tests/smp_tests`

## Git

The `build/` directory should never be committed. It's safe to ignore since it's just build artifacts.
