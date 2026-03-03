# SMP Protocol for MCUboot (Target-Side) Design

**Date:** 2026-03-03  
**Status:** Approved

## Goal

Build a bare-metal/RTOS-agnostic C SMP implementation for target firmware that supports image upload for MCUboot, with transport abstraction through user-provided read/write function pointers, and strong unit test coverage.

## Scope (v1)

- Target-side SMP handling module (firmware side)
- Transport-agnostic I/O via callbacks in a struct
- Image upload command support (only), intentionally designed for future command extension
- No embedded security crypto verification in library; trust/verification remains with MCUboot/platform
- Unit tests for protocol, upload state machine, transport behavior, and failure paths

## Non-Goals (v1)

- Full SMP command set
- Zephyr/Mynewt-specific integration layers
- Built-in crypto/signature verification
- Dynamic memory-heavy runtime behavior

## Architecture

### 1. Layered Modules

- `smp_core`: frame decode/encode, request validation, response creation
- `smp_dispatch`: route SMP requests by group/id to registered handlers
- `smp_cmd_img_upload`: implements image upload command logic

This keeps core parsing stable while allowing additional commands later without major rewrites.

### 2. Transport Abstraction

Public transport contract:

```c
typedef int (*smp_read_fn)(void *ctx, uint8_t *buf, size_t max_len);
typedef int (*smp_write_fn)(void *ctx, const uint8_t *buf, size_t len);

struct smp_transport_io {
    smp_read_fn read;
    smp_write_fn write;
    void *ctx;
};
```

The library does not know UART/BLE/USB specifics; it only consumes/provides bytes.

### 3. Platform Update Hooks

Image write/finalization is delegated to platform callbacks:

```c
struct smp_platform_ops {
    int (*slot_begin)(void *ctx);
    int (*slot_write)(void *ctx, uint32_t off, const uint8_t *data, size_t len);
    int (*slot_finalize)(void *ctx, size_t total_size);
    int (*set_pending)(void *ctx);
    void *ctx;
};
```

This keeps the module MCU/flash-driver agnostic.

## Data Flow

1. Application initializes `smp_ctx` with transport + platform callbacks.
2. Application calls `smp_poll(&ctx)` periodically.
3. `smp_poll` reads bytes, assembles complete SMP frame, validates header.
4. Dispatcher routes request to image upload handler.
5. Upload handler validates `off`, writes data to slot, updates state.
6. Response returns current/next expected offset.
7. Final chunk triggers finalize/pending hooks.

## Upload State Model

- State held inside `smp_ctx` (no globals)
- Core fields:
  - `expected_off`
  - `bytes_written`
  - `is_upload_active`
  - `last_error`
- Transitions:
  - `IDLE -> UPLOADING -> COMPLETE`
  - Any callback failure enters `ERROR` until reset

## Error Handling

- Deterministic error responses for:
  - malformed request
  - unsupported command
  - invalid offset
  - platform write/finalize/pending failure
  - busy/invalid state
- Offset mismatch returns expected offset for resume
- Oversize frame/chunk rejected at parse stage

## Extensibility Strategy

- Keep command handlers in isolated modules
- Dispatcher table allows adding new commands (image list/state in v2)
- Keep upload logic independent from transport implementation

## Test Strategy

### Unit Tests (host side)

- `smp_core`:
  - header parsing/encoding
  - invalid frame rejection
  - unknown group/id handling
- `smp_cmd_img_upload`:
  - happy path chunk sequence
  - invalid/duplicate offset behavior
  - finalization path
  - injected platform failures
- transport behavior:
  - fragmented reads
  - partial writes
  - back-to-back frames

### Test Infrastructure

- Build tests with CMake + CTest
- Fake transport and fake platform implementations
- Golden request/response vectors for regression coverage

## Acceptance Criteria

- Library compiles as plain C (C99)
- Image upload works with callback-backed transport and platform hooks
- No RTOS dependency
- All unit tests pass in CI/local
- API allows future command handlers without changing core upload behavior
