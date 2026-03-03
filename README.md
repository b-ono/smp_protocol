# SMP-Protocol

Small, transport-agnostic SMP target library for MCUboot image upload in C.

## What it provides

- SMP frame parsing and response encoding
- Dispatcher-based command handling
- v1 image upload command support
- Transport abstraction via callback struct (`read`/`write`)
- Platform update abstraction via callback struct (slot write/finalize/pending)
- Unit tests (CMake + CTest)

## Build and test

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Dependencies

- [TinyCBOR](https://github.com/intel/tinycbor) is fetched at configure time with CMake `FetchContent`.

## Examples

### Complete Upload Demo
Demonstrates full image upload workflow with 256-byte image split into 4 chunks:
```bash
./build/complete_upload_demo
```

### Error Scenarios Demo
Shows error handling for invalid offsets, write/finalize/pending failures, and unknown commands:
```bash
./build/error_scenarios_demo
```

### STM32 Integration
Realistic embedded integration pattern with STM32 flash operations:
- See `examples/integration_target.c`
- Copy `examples/integration_target.h` into your project
- Hook up to your flash driver

### Minimal Integration
Bare minimum to get SMP running:
```c
struct smp_ctx ctx;
struct smp_transport_io io = { .read = my_read, .write = my_write, .ctx = &my_ctx };
struct smp_platform_ops ops = { .slot_begin = my_begin, .slot_write = my_write,
                                 .slot_finalize = my_finalize, .set_pending = my_pending, .ctx = &my_plat };

smp_init(&ctx, &io, &ops, NULL);

while (1) {
    smp_poll(&ctx);
}
```

See `examples/minimal_target.c` for a complete setup.

The API entry points are:

- `smp_init(...)`
- `smp_poll(...)`
- `smp_reset_upload_state(...)`

## Protocol shape in this v1

This implementation uses standard SMP 8-byte headers with CBOR payloads for `mcumgr image upload` compatibility.

- Request payload map keys:
  - required `off` (uint)
  - required `data` (bstr, chunk bytes)
  - optional `len` (uint, total image length)
  - optional `sha`, `image`, `upgrade`
- Response payload map keys:
  - `rc` (int)
  - `off` (uint)
  - optional `match` (bool)
