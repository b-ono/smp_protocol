# sem_protocol

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

## Minimal integration

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
