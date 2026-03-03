# SMP MCUboot mcumgr Compatibility Design

**Date:** 2026-03-03  
**Status:** Approved

## Goal

Make the target-side SMP upload implementation wire-compatible with `mcumgr image upload` by replacing the custom payload format with standard SMP CBOR payloads, while keeping transport and platform callbacks unchanged.

## Scope

- Strict compatibility for image upload command only
- Standard SMP framing + CBOR payload for upload requests/responses
- Keep current module split (`smp_core`, `smp_dispatch`, `smp_cmd_img_upload`)
- Integrate an external CBOR library (no custom CBOR implementation)
- Add focused tests for CBOR decode/encode and upload behavior expected by `mcumgr`

## Non-Goals

- `image list` compatibility in this iteration
- OS mgmt/echo/reset commands
- Changes to transport callback API
- Built-in cryptographic verification in this library

## Architecture

### 1. Preserve Existing Core/Dispatch Layers

- `smp_core` remains responsible for SMP header framing, buffering, and writeback.
- `smp_dispatch` still routes by `(group, id)`.
- `smp_cmd_img_upload` remains upload state machine owner.

Only the request/response payload encoding in upload command changes from compact binary to CBOR map.

### 2. External CBOR Library via Adapter Boundary

Add a dedicated adapter module to isolate third-party dependency:

- `src/smp_cbor_adapter.c`
- `src/smp_cbor_adapter.h`

Responsibilities:

- Decode upload request CBOR map into internal struct
- Encode upload response CBOR map from internal struct
- Keep all CBOR-library-specific symbols out of `include/smp/*.h`

### 3. Internal Upload Message Shape

Decode request keys used by `mcumgr image upload`:

- Required: `off`
- Required for non-empty chunk: `data`
- Optional: `len`, `sha`, `image`, `upgrade`

Encode response keys:

- `rc`
- `off`
- Optional: `match` when relevant for resume behavior

## Data Flow

1. `smp_poll` receives complete frame and dispatches image upload command.
2. Upload command calls CBOR adapter to decode payload map.
3. Upload command validates `off` against `expected_off`.
4. On first accepted chunk (`off == 0`), call `slot_begin`.
5. Call `slot_write` with decoded `data` bytes.
6. On final condition, call `slot_finalize` then `set_pending`.
7. Command encodes CBOR response containing `rc` and server `off`.

## Error Handling

- Malformed CBOR / wrong key types -> protocol/invalid-argument style `rc`
- Unsupported command/group -> not-supported `rc`
- Offset mismatch -> invalid-argument style `rc` + current server `off`
- Platform write/finalize/pending failures -> internal/IO style `rc`

Unknown CBOR keys are ignored unless they conflict with required semantics.

## Testing Strategy

### Unit Tests

- Decode valid upload CBOR vectors (`off`, `data`, optional keys)
- Reject malformed CBOR and wrong key types
- Encode response CBOR and verify keys/values (`rc`, `off`, optional `match`)
- End-to-end upload sequence with fake transport and platform callbacks
- Offset mismatch/resume scenarios used by `mcumgr`

### Build/Verification

- Build with strict warnings (`-Wall -Wextra -Werror`)
- Run CTest suite
- Keep protocol fixtures deterministic for regression checks

## Acceptance Criteria

- `mcumgr image upload` payload format is accepted by target library
- Response fields (`rc`, `off`) match expected wire semantics
- Existing transport abstraction remains unchanged
- Unit tests pass with strict compiler flags
