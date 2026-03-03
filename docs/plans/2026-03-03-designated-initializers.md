# Designated Initializers Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace positional struct initializers with designated initializers in test_main.c for improved code clarity

**Architecture:** Designated initializers were introduced in C99, so this change is compatible with C17 and C23. No standard library changes needed, just syntax updates to struct initialization.

**Tech Stack:** C99/C17/C23, CMake, CTest

---

### Task 1: Analyze current struct initializers in test_main.c

**Files:**
- Read: `tests/test_main.c`

**Step 1: Review all struct initializations**

Identify all positional initializers that can benefit from designated initializers:
- `struct smp_transport_io t = { fake_read, fake_write, &io };`
- `struct smp_platform_ops p = { fake_slot_begin, fake_slot_write, fake_slot_finalize, fake_set_pending, &plat };`
- Array of test functions without field names

**Step 2: Document locations**

| Line | Current | Can use designated |
|------|---------|-------------------|
| 22-23 | `struct smp_transport_io t = { fake_read, fake_write, &io };` | Yes |
| 24-26 | `struct smp_platform_ops p = { ... }` | Yes |
| 138-148 | `struct { const char *name; test_fn fn; } tests[] = { {"api_contract", test_api_contract}, ... };` | Yes |

---

### Task 2: Update smp_transport_io initialization (line 22-23)

**Files:**
- Modify: `tests/test_main.c:22-23`

**Step 1: Read current code**

```c
struct smp_transport_io t = { fake_read, fake_write, &io };
```

**Step 2: Replace with designated initializer**

```c
struct smp_transport_io t = { .read = fake_read, .write = fake_write, .ctx = &io };
```

**Step 3: Verify syntax**

Run: `gcc -std=c17 -fsyntax-only tests/test_main.c`
Expected: PASS (no errors)

**Step 4: Commit**

```bash
git add tests/test_main.c
git commit -m "test: use designated initializers for smp_transport_io"
```

---

### Task 3: Update smp_platform_ops initialization (line 24-26)

**Files:**
- Modify: `tests/test_main.c:24-26`

**Step 1: Read current code**

```c
struct smp_platform_ops p = { fake_slot_begin, fake_slot_write, 
                               fake_slot_finalize, fake_set_pending, &plat };
```

**Step 2: Replace with designated initializer**

```c
struct smp_platform_ops p = { 
    .slot_begin = fake_slot_begin, 
    .slot_write = fake_slot_write, 
    .slot_finalize = fake_slot_finalize, 
    .set_pending = fake_set_pending, 
    .ctx = &plat 
};
```

**Step 3: Verify syntax**

Run: `gcc -std=c17 -fsyntax-only tests/test_main.c`
Expected: PASS (no errors)

**Step 4: Commit**

```bash
git add tests/test_main.c
git commit -m "test: use designated initializers for smp_platform_ops"
```

---

### Task 4: Update test array initialization (line 138-148)

**Files:**
- Modify: `tests/test_main.c:138-148`

**Step 1: Read current code**

```c
struct {
    const char *name;
    test_fn fn;
} tests[] = {
    {"api_contract", test_api_contract},
    {"upload_happy_path", test_upload_happy_path},
    {"upload_fragmented", test_upload_fragmented},
    {"upload_aborted", test_upload_aborted},
    {"upload_invalid", test_upload_invalid},
    {"upload_oversized", test_upload_oversized},
    {"upload_rejected", test_upload_rejected},
};
```

**Step 2: Replace with designated initializers**

```c
struct {
    const char *name;
    test_fn fn;
} tests[] = {
    { .name = "api_contract", .fn = test_api_contract },
    { .name = "upload_happy_path", .fn = test_upload_happy_path },
    { .name = "upload_fragmented", .fn = test_upload_fragmented },
    { .name = "upload_aborted", .fn = test_upload_aborted },
    { .name = "upload_invalid", .fn = test_upload_invalid },
    { .name = "upload_oversized", .fn = test_upload_oversized },
    { .name = "upload_rejected", .fn = test_upload_rejected },
};
```

**Step 3: Verify syntax**

Run: `gcc -std=c17 -fsyntax-only tests/test_main.c`
Expected: PASS (no errors)

**Step 4: Commit**

```bash
git add tests/test_main.c
git commit -m "test: use designated initializers for test array"
```

---

### Task 5: Run full test suite to verify no regressions

**Files:**
- Test: `tests/test_main.c`

**Step 1: Build tests**

Run: `cd tests && cmake .. && make`
Expected: BUILD SUCCESS

**Step 2: Run tests**

Run: `ctest --output-on-failure`
Expected: ALL TESTS PASS

**Step 3: Verify with different standards**

Run: `gcc -std=c11 -fsyntax-only tests/test_main.c`
Expected: PASS (designated initializers are C99 feature)

Run: `gcc -std=c17 -fsyntax-only tests/test_main.c`
Expected: PASS

Run: `gcc -std=c23 -fsyntax-only tests/test_main.c`
Expected: PASS

**Step 4: Commit final changes**

```bash
git add tests/test_main.c
git commit -m "test: finalize designated initializers migration"
```

---

### Task 6: Update CMakeLists.txt to C17 (optional)

**Files:**
- Modify: `CMakeLists.txt:4-6`

**Step 1: Check current standard**

Read: `CMakeLists.txt:4-6`
Current: `set(CMAKE_C_STANDARD 99)`

**Step 2: Update to C17**

```cmake
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
```

**Step 3: Verify build**

Run: `cmake . && make`
Expected: BUILD SUCCESS

**Step 4: Commit**

```bash
git add CMakeLists.txt
git commit -m "build: upgrade C standard from C99 to C17"
```

---

**Plan complete. Ready for execution.**
