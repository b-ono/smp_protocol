# AGENTS.md - Development Guidelines for sem_protocol

## Build System

**Always use out-of-source builds in `build/` directory.**

```bash
mkdir -p build && cd build && cmake .. && make && ctest
```

**Why:**
- Keeps repository clean (no build artifacts polluting source)
- Makes it easy to rebuild from scratch
- Prevents accidental commits of generated files

**Never build in the root directory.**

## C Standard

**Current standard:** C17

**Features in use:**
- Designated initializers (C99) - improves code clarity
- Fixed-width integers (`stdint.h`)
- Boolean type (`stdbool.h`)
- Inline functions

**Designated initializers example:**
```c
// Instead of positional:
struct smp_transport_io t = { fake_read, fake_write, &io };

// Use designated:
struct smp_transport_io t = { .read = fake_read, .write = fake_write, .ctx = &io };
```

## Code Style

- No comments unless explaining non-obvious logic
- Use exact file paths in plans
- Complete code in plans (not "add validation")
- DRY, YAGNI, TDD principles
- Frequent commits

## Testing

**Run tests:**
```bash
cd build && ctest --output-on-failure
```

**Verify changes:**
- Always run tests before claiming completion
- Check syntax: `gcc -std=c17 -fsyntax-only file.c`
- Build must succeed with no errors

## Git Workflow

**Commit messages:**
- `feat: add new feature`
- `test: add test for X`
- `fix: resolve issue Y`
- `build: upgrade C standard`
- `refactor: improve code structure`

**Never commit:**
- Build artifacts (`build/`, `CMakeCache.txt`, etc.)
- Secrets (`.env`, credentials)
- Generated files

## Verification Before Completion

**Always verify before claiming success:**
1. Run tests - see actual output
2. Check build - verify exit code 0
3. Review changes - diff shows what was modified
4. No "should pass" - evidence required

## C23 Features

**Not currently adopted.** C99/C17 features are sufficient for this codebase.

**Potential C23 features (future consideration):**
- Binary integer literals (`0b00000001`)
- `_Static_assert` for compile-time checks
- Unnamed parameters (`__unused`)

**Current priority:** Low - code is clean and functional with C17.

## Tools

- **CMake** for build system
- **CTest** for testing
- **GCC 15.2.1** (or compatible compiler)
- **tinycbor** for CBOR encoding/decoding

## Skills

When relevant, use these skills:
- `writing-plans` - for implementation plans
- `verification-before-completion` - before claiming success
- `subagent-driven-development` - for complex multi-task work
- `brainstorming` - before creative work
