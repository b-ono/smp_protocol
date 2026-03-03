# SMP MCUboot mcumgr Compatibility Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace custom upload payload encoding with standard mcumgr-compatible CBOR payloads for SMP image upload while preserving transport and platform callback interfaces.

**Architecture:** Keep the existing `smp_core` and `smp_dispatch` layers intact, add a CBOR adapter module around an external CBOR library, and update `smp_cmd_img_upload` to decode/encode standard upload maps. Implement in strict TDD cycles so each wire behavior is validated before production code changes.

**Tech Stack:** C99, CMake, CTest, zcbor (external CBOR library)

---

### Task 1: Add External CBOR Dependency and Build Wiring

**Files:**
- Modify: `CMakeLists.txt`
- Create: `third_party/README.md`
- Modify: `README.md`

**Step 1: Write the failing test**

Add a compile-time check test source in `tests/test_main.c` that references a CBOR adapter symbol:

```c
extern int smp_cbor_adapter_link_probe(void);
```

Expected compile failure until adapter/dependency is wired.

**Step 2: Run test to verify it fails**

Run: `cmake -S . -B build && cmake --build build`
Expected: FAIL with unresolved/missing CBOR adapter symbol.

**Step 3: Write minimal implementation**

- Add zcbor as vendored dependency (or cmake-fetch strategy) in `CMakeLists.txt`.
- Expose include paths and link target for SMP library only.
- Document dependency source/version in `third_party/README.md`.

**Step 4: Run test to verify it passes**

Run: `cmake -S . -B build && cmake --build build`
Expected: PASS build.

**Step 5: Commit**

```bash
git add CMakeLists.txt third_party/README.md README.md
git commit -m "build: add external cbor dependency for mcumgr compatibility"
```

### Task 2: Create CBOR Adapter API (Decode/Encode Contracts)

**Files:**
- Create: `src/smp_cbor_adapter.h`
- Create: `src/smp_cbor_adapter.c`
- Modify: `src/smp_internal.h`
- Test: `tests/test_cbor_adapter_contract.c`

**Step 1: Write the failing test**

Create `tests/test_cbor_adapter_contract.c` with expected adapter API calls:

```c
int test_decode_upload_map_contract(void);
int test_encode_upload_response_contract(void);
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build && ctest --test-dir build -R cbor_adapter_contract --output-on-failure`
Expected: FAIL due to missing adapter implementation.

**Step 3: Write minimal implementation**

- Define internal structs for decoded upload request and upload response.
- Implement placeholder decode/encode with strict bounds checks.
- Add `smp_cbor_adapter_link_probe()` symbol used by test.

**Step 4: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build -R cbor_adapter_contract --output-on-failure`
Expected: PASS.

**Step 5: Commit**

```bash
git add src/smp_cbor_adapter.h src/smp_cbor_adapter.c src/smp_internal.h tests/test_cbor_adapter_contract.c
git commit -m "feat: add cbor adapter interface for upload payloads"
```

### Task 3: Decode mcumgr Upload Request CBOR (RED -> GREEN)

**Files:**
- Modify: `src/smp_cbor_adapter.c`
- Test: `tests/test_cbor_decode_upload.c`

**Step 1: Write the failing test**

Create `tests/test_cbor_decode_upload.c` with vectors for:

```c
int test_decodes_off_and_data(void);
int test_decodes_optional_len_sha_image_upgrade(void);
int test_rejects_wrong_types_for_off_and_data(void);
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build && ctest --test-dir build -R cbor_decode_upload --output-on-failure`
Expected: FAIL before decode logic exists.

**Step 3: Write minimal implementation**

Implement zcbor-based map decoding for keys:

- `off` (uint)
- `data` (bstr)
- `len` (uint, optional)
- `sha` (bstr, optional)
- `image` (uint, optional)
- `upgrade` (bool, optional)

Reject malformed CBOR and wrong key types.

**Step 4: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build -R cbor_decode_upload --output-on-failure`
Expected: PASS.

**Step 5: Commit**

```bash
git add src/smp_cbor_adapter.c tests/test_cbor_decode_upload.c
git commit -m "feat: decode mcumgr image upload request cbor"
```

### Task 4: Encode mcumgr Upload Response CBOR

**Files:**
- Modify: `src/smp_cbor_adapter.c`
- Test: `tests/test_cbor_encode_response.c`

**Step 1: Write the failing test**

Create `tests/test_cbor_encode_response.c`:

```c
int test_encodes_rc_and_off(void);
int test_encodes_optional_match_when_set(void);
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build && ctest --test-dir build -R cbor_encode_response --output-on-failure`
Expected: FAIL until response encoder is implemented.

**Step 3: Write minimal implementation**

Encode response CBOR map with:

- required `rc`
- required `off`
- optional `match`

Ensure deterministic map contents and bounds safety.

**Step 4: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build -R cbor_encode_response --output-on-failure`
Expected: PASS.

**Step 5: Commit**

```bash
git add src/smp_cbor_adapter.c tests/test_cbor_encode_response.c
git commit -m "feat: encode mcumgr upload response cbor"
```

### Task 5: Integrate Adapter into Image Upload Command

**Files:**
- Modify: `src/smp_cmd_img_upload.c`
- Modify: `src/smp_internal.h`
- Test: `tests/test_main.c`

**Step 1: Write the failing test**

In `tests/test_main.c`, add end-to-end tests using CBOR payload frames:

```c
int test_upload_happy_path_mcumgr_cbor(void);
int test_invalid_offset_returns_rc_and_server_off(void);
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build && ctest --test-dir build -R smp_tests --output-on-failure`
Expected: FAIL because handler still expects custom binary payload.

**Step 3: Write minimal implementation**

- Replace binary payload parsing in upload handler with adapter decode.
- Reuse existing upload state logic for offset/write/finalize/pending.
- Encode response using adapter encode.

**Step 4: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build -R smp_tests --output-on-failure`
Expected: PASS.

**Step 5: Commit**

```bash
git add src/smp_cmd_img_upload.c src/smp_internal.h tests/test_main.c
git commit -m "feat: switch image upload command to mcumgr cbor payloads"
```

### Task 6: Malformed CBOR and Error Mapping Coverage

**Files:**
- Modify: `src/smp_cmd_img_upload.c`
- Modify: `src/smp_core.c`
- Test: `tests/test_error_mapping.c`

**Step 1: Write the failing test**

Create/extend `tests/test_error_mapping.c`:

```c
int test_malformed_cbor_maps_protocol_error(void);
int test_wrong_key_type_maps_invalid_arg_error(void);
int test_platform_write_failure_maps_internal_error(void);
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build && ctest --test-dir build -R error_mapping --output-on-failure`
Expected: FAIL until mapping is deterministic.

**Step 3: Write minimal implementation**

- Normalize adapter and upload-handler failures to stable `rc` mapping.
- Ensure responses always include server `off` where expected by client resume behavior.

**Step 4: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build -R error_mapping --output-on-failure`
Expected: PASS.

**Step 5: Commit**

```bash
git add src/smp_cmd_img_upload.c src/smp_core.c tests/test_error_mapping.c
git commit -m "fix: stabilize mcumgr-compatible cbor error responses"
```

### Task 7: Update Test Fixtures and Documentation

**Files:**
- Modify: `README.md`
- Modify: `examples/minimal_target.c`
- Create: `tests/fixtures/mcumgr_upload_vectors.md`

**Step 1: Write the failing test**

Add a documentation conformance check note requiring response fields and request keys to match mcumgr upload protocol.

**Step 2: Run test to verify it fails**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL if tests/docs/examples still reference custom payload format.

**Step 3: Write minimal implementation**

- Update README protocol section to CBOR map shape.
- Update example comments to mention mcumgr-compatible payload handling.
- Add fixture notes with representative request/response vectors.

**Step 4: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: PASS.

**Step 5: Commit**

```bash
git add README.md examples/minimal_target.c tests/fixtures/mcumgr_upload_vectors.md
git commit -m "docs: describe mcumgr-compatible upload payload format"
```

### Task 8: Final Strict Verification

**Files:**
- Modify: `docs/plans/2026-03-03-smp-mcumgr-compat-implementation.md` (checklist status)

**Step 1: Write the failing test**

Track completion checklist:

```markdown
- [ ] strict warnings build clean
- [ ] all unit tests green
- [ ] upload cbor vectors validated
```

**Step 2: Run test to verify it fails**

Run: `cmake -S . -B build -DCMAKE_C_FLAGS='-Wall -Wextra -Werror' && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL if any warnings/tests remain.

**Step 3: Write minimal implementation**

- Fix any remaining warnings or flaky tests.
- Keep feature scope unchanged.

**Step 4: Run test to verify it passes**

Run: `cmake -S . -B build -DCMAKE_C_FLAGS='-Wall -Wextra -Werror' && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: PASS with clean output.

**Step 5: Commit**

```bash
git add -A
git commit -m "chore: finalize mcumgr-compatible image upload protocol"
```
