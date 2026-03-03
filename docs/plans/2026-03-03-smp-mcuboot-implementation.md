# SMP MCUboot Target Library Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a transport-agnostic, target-side SMP C library for MCUboot image upload with thorough unit tests.

**Architecture:** Implement a layered design: frame/encoding core, command dispatcher, and an image upload command module. Keep transport and platform update logic behind callback tables so the library remains RTOS-agnostic and easy to port. Start from failing tests and grow implementation in minimal increments.

**Tech Stack:** C99, CMake, CTest, custom C unit test executable

---

### Task 1: Repository Skeleton and Build Wiring

**Files:**
- Create: `CMakeLists.txt`
- Create: `include/smp/smp.h`
- Create: `include/smp/smp_types.h`
- Create: `src/smp_core.c`
- Create: `src/smp_dispatch.c`
- Create: `src/smp_cmd_img_upload.c`
- Create: `tests/CMakeLists.txt`
- Create: `tests/test_main.c`
- Create: `tests/test_support.h`
- Create: `tests/test_support.c`

**Step 1: Write the failing test**

Add to `tests/test_main.c`:

```c
#include "test_support.h"

int main(void) {
    return test_smoke_build_links();
}
```

And in `tests/test_support.c`:

```c
int test_smoke_build_links(void) {
    struct smp_ctx ctx;
    (void)ctx;
    return 0;
}
```

**Step 2: Run test to verify it fails**

Run: `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`

Expected: FAIL due to missing `struct smp_ctx`/headers/sources.

**Step 3: Write minimal implementation**

- Add minimal `struct smp_ctx` declaration to `include/smp/smp_types.h`
- Wire CMake targets for library and tests.

**Step 4: Run test to verify it passes**

Run: `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`

Expected: PASS for smoke test.

**Step 5: Commit**

```bash
git add CMakeLists.txt include/smp/smp_types.h include/smp/smp.h src tests
git commit -m "build: bootstrap smp library and test harness"
```

### Task 2: Public API for Transport and Platform Callbacks

**Files:**
- Modify: `include/smp/smp_types.h`
- Modify: `include/smp/smp.h`
- Test: `tests/test_api_contract.c`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the failing test**

Create `tests/test_api_contract.c`:

```c
#include "smp/smp.h"

static int rd(void *ctx, uint8_t *buf, size_t len) { (void)ctx; (void)buf; (void)len; return 0; }
static int wr(void *ctx, const uint8_t *buf, size_t len) { (void)ctx; (void)buf; (void)len; return (int)len; }

int test_api_contract(void) {
    struct smp_transport_io io = { .read = rd, .write = wr, .ctx = 0 };
    struct smp_platform_ops ops = {0};
    struct smp_ctx ctx;
    return smp_init(&ctx, &io, &ops, 0);
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`

Expected: FAIL because `smp_init` and API structs are missing/incomplete.

**Step 3: Write minimal implementation**

Define in headers:

```c
struct smp_transport_io { smp_read_fn read; smp_write_fn write; void *ctx; };
struct smp_platform_ops { smp_slot_begin_fn slot_begin; smp_slot_write_fn slot_write; smp_slot_finalize_fn slot_finalize; smp_set_pending_fn set_pending; void *ctx; };
int smp_init(struct smp_ctx *ctx, const struct smp_transport_io *io, const struct smp_platform_ops *ops, const struct smp_cfg *cfg);
```

Add minimal `smp_init` implementation returning `0` when required callbacks exist.

**Step 4: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`

Expected: PASS for API contract test.

**Step 5: Commit**

```bash
git add include/smp/smp_types.h include/smp/smp.h src/smp_core.c tests/test_api_contract.c tests/CMakeLists.txt
git commit -m "feat: add callback-based transport and platform API"
```

### Task 3: SMP Frame Parsing and Basic Dispatch

**Files:**
- Modify: `src/smp_core.c`
- Modify: `src/smp_dispatch.c`
- Modify: `include/smp/smp_types.h`
- Test: `tests/test_core_parse_dispatch.c`

**Step 1: Write the failing test**

Create `tests/test_core_parse_dispatch.c` with cases:

```c
int test_rejects_short_header(void);
int test_rejects_unknown_command(void);
int test_dispatch_calls_registered_handler(void);
```

Use fake RX bytes and assert error code/handler invocation.

**Step 2: Run test to verify it fails**

Run: `cmake --build build && ctest --test-dir build -R core_parse_dispatch --output-on-failure`

Expected: FAIL with missing parse/dispatch behavior.

**Step 3: Write minimal implementation**

- Add fixed-size RX accumulator in `smp_ctx`.
- Implement minimal SMP header parser.
- Implement dispatcher lookup by `(group,id)` and fallback unsupported response.

**Step 4: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build -R core_parse_dispatch --output-on-failure`

Expected: PASS.

**Step 5: Commit**

```bash
git add src/smp_core.c src/smp_dispatch.c include/smp/smp_types.h tests/test_core_parse_dispatch.c
git commit -m "feat: implement minimal smp parse and command dispatch"
```

### Task 4: Implement Upload State Machine (Offset Validation)

**Files:**
- Modify: `src/smp_cmd_img_upload.c`
- Modify: `include/smp/smp_types.h`
- Test: `tests/test_img_upload_offsets.c`

**Step 1: Write the failing test**

Create `tests/test_img_upload_offsets.c`:

```c
int test_accepts_first_chunk_at_offset_zero(void);
int test_rejects_non_matching_offset_with_expected_offset_returned(void);
int test_advances_expected_offset_after_write(void);
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build && ctest --test-dir build -R img_upload_offsets --output-on-failure`

Expected: FAIL due to missing state logic.

**Step 3: Write minimal implementation**

- Track `expected_off` and `bytes_written` in `smp_ctx`.
- For valid chunk: call `slot_write`, then increment `expected_off`.
- For invalid offset: return protocol error and expected offset.

**Step 4: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build -R img_upload_offsets --output-on-failure`

Expected: PASS.

**Step 5: Commit**

```bash
git add src/smp_cmd_img_upload.c include/smp/smp_types.h tests/test_img_upload_offsets.c
git commit -m "feat: add upload offset validation and state progression"
```

### Task 5: Finalization and Pending-Image Marking

**Files:**
- Modify: `src/smp_cmd_img_upload.c`
- Test: `tests/test_img_upload_finalize.c`

**Step 1: Write the failing test**

Create `tests/test_img_upload_finalize.c`:

```c
int test_final_chunk_calls_slot_finalize_and_set_pending(void);
int test_finalize_failure_surfaces_error(void);
int test_pending_failure_surfaces_error(void);
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build && ctest --test-dir build -R img_upload_finalize --output-on-failure`

Expected: FAIL due to missing finalize and pending behavior.

**Step 3: Write minimal implementation**

- Detect final chunk flag/condition from request.
- Call `slot_finalize` then `set_pending`.
- Return completion response or mapped failure code.

**Step 4: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build -R img_upload_finalize --output-on-failure`

Expected: PASS.

**Step 5: Commit**

```bash
git add src/smp_cmd_img_upload.c tests/test_img_upload_finalize.c
git commit -m "feat: finalize uploads and mark image pending"
```

### Task 6: Transport Fragmentation and Partial Write Handling

**Files:**
- Modify: `src/smp_core.c`
- Test: `tests/test_transport_fragmentation.c`
- Modify: `tests/test_support.c`

**Step 1: Write the failing test**

Create `tests/test_transport_fragmentation.c`:

```c
int test_fragmented_reads_accumulate_until_full_frame(void);
int test_partial_write_retries_until_response_sent(void);
int test_back_to_back_frames_are_processed_in_order(void);
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build && ctest --test-dir build -R transport_fragmentation --output-on-failure`

Expected: FAIL due to no robust buffering/tx retry behavior.

**Step 3: Write minimal implementation**

- Add RX/TX cursors in context.
- Process complete frame only when full bytes available.
- Keep unsent response bytes for subsequent `smp_poll` calls.

**Step 4: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build -R transport_fragmentation --output-on-failure`

Expected: PASS.

**Step 5: Commit**

```bash
git add src/smp_core.c tests/test_transport_fragmentation.c tests/test_support.c
git commit -m "feat: support fragmented rx and partial tx in smp poll"
```

### Task 7: Error Code Mapping and Robust Failure Paths

**Files:**
- Modify: `include/smp/smp_types.h`
- Modify: `src/smp_core.c`
- Modify: `src/smp_cmd_img_upload.c`
- Test: `tests/test_error_mapping.c`

**Step 1: Write the failing test**

Create `tests/test_error_mapping.c`:

```c
int test_invalid_offset_maps_to_protocol_error_code(void);
int test_slot_write_failure_maps_to_io_error_code(void);
int test_unsupported_command_maps_to_not_supported_code(void);
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build && ctest --test-dir build -R error_mapping --output-on-failure`

Expected: FAIL due to incomplete deterministic error mapping.

**Step 3: Write minimal implementation**

- Introduce enum for stable SMP internal status codes.
- Add mapping layer to response error fields.

**Step 4: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build -R error_mapping --output-on-failure`

Expected: PASS.

**Step 5: Commit**

```bash
git add include/smp/smp_types.h src/smp_core.c src/smp_cmd_img_upload.c tests/test_error_mapping.c
git commit -m "feat: add deterministic protocol error mapping"
```

### Task 8: Public Documentation and Usage Example

**Files:**
- Create: `README.md`
- Create: `examples/minimal_target.c`

**Step 1: Write the failing test**

Add doc test checklist item in `README.md` draft that requires a compilable example snippet.

```c
/* example must compile with project headers */
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build`

Expected: FAIL until `examples/minimal_target.c` compiles and links against headers correctly.

**Step 3: Write minimal implementation**

- Add README with API overview and integration steps.
- Add minimal example using callback structs and `smp_poll` loop.

**Step 4: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`

Expected: PASS for all tests; example builds cleanly.

**Step 5: Commit**

```bash
git add README.md examples/minimal_target.c
git commit -m "docs: add usage guide and minimal target example"
```

### Task 9: Final Verification Gate

**Files:**
- Modify: `docs/plans/2026-03-03-smp-mcuboot-implementation.md` (checklist state)

**Step 1: Write the failing test**

Define completion checklist:

```markdown
- [ ] clean configure/build
- [ ] full test suite green
- [ ] no compiler warnings on default flags
```

**Step 2: Run test to verify it fails**

Run: `cmake -S . -B build -DCMAKE_C_FLAGS='-Wall -Wextra -Werror' && cmake --build build && ctest --test-dir build --output-on-failure`

Expected: FAIL if warnings/tests remain.

**Step 3: Write minimal implementation**

- Fix any remaining warnings or brittle tests.
- Keep behavior unchanged.

**Step 4: Run test to verify it passes**

Run: `cmake -S . -B build -DCMAKE_C_FLAGS='-Wall -Wextra -Werror' && cmake --build build && ctest --test-dir build --output-on-failure`

Expected: PASS with clean output.

**Step 5: Commit**

```bash
git add -A
git commit -m "chore: finalize smp mcuboot upload library with full tests"
```
