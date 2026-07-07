# VidChopper Coding Style & Engineering Guide

This document is the single source of truth for **how code is written in this repository**.
It is descriptive of the conventions the codebase already follows and prescriptive for all
new work. Read it before contributing, and follow it rigorously — consistency here is
deliberate and self-documenting.

If a change would violate a rule below, either follow the rule or change this document first
(with justification) — do not silently diverge.

For connector-based work, automated code edits, or future AI-agent handoffs, read
`AGENTS.md` alongside this guide before changing code.

---

## 1. Philosophy & priorities

VidChopper turns one long source video (think multi-hour streams) into chapter clips with
`ffmpeg`. When goals conflict, resolve them in this order:

1. **Speed.** Cutting a 6-hour stream is inherently heavy; prefer the faster correct option.
   Avoid needless copies, allocate once and reserve, compute at compile time where possible.
2. **Clarity & readability.** Code is read far more than written. Favor obvious code; add a
   short comment when the *why* is non-obvious. Clarity rarely conflicts with speed — when it
   does, write the fast path and explain it.
3. **Brevity.** Shorter is better only when it is also clear.

Supporting principles:

- **Behavior-preserving by default.** The core works; do not change observable behavior
  (output bytes, file names, command strings, INI round-trips) unless the task says so. Lock
  intended-identical behavior with **golden tests added _before_ the refactor**.
- **Alpha, but disciplined.** Signature/struct changes are welcome when they improve the
  product, but they come with updated tests and a clear PR rationale.
- **The core stays Qt-free.** `src/core` must compile and be fully testable without the Qt
  SDK. Qt lives only in `src/qt`.

---

## 2. Repository layout

```
src/core/      Qt-free C++20 library: models, timecode, parsing, command building,
               chapter planning, string utils, enum utils. Fully unit-tested in CI.
src/qt/        Qt 6 Widgets desktop shell. Depends on core; never the reverse.
  src/qt/ui/         windows, dialogs, table models
  src/qt/services/   ffprobe, gpu detection, export orchestration
tests/         Hand-rolled, dependency-free tests (see §15). fast + slow labels.
resources/     App icon assets (.ico/.png/.qrc/.rc).
docs/          Static site published to GitHub Pages.
.devin/skills/ Qt agent-skills (qt-cpp-review, qt-cpp-docs, qt-project).
AGENTS.md      Connector/AI-agent workflow for branch, PR, formatting, and CI discipline.
```

Dependency rule: **`src/qt` → `src/core` only.** Nothing in `core` may include Qt or anything
from `qt`.

---

## 3. Language standard & toolchain

- **C++20**, no C++23 features (no `std::expected`, `std::string::contains`,
  `std::to_underlying`, `views::enumerate`). No modules or coroutines.
- Compilers: **MSVC 17 (2022)** on Windows, **g++-13** in CI (libstdc++ that has
  `std::format`). Code must build warning-clean on both.
- Build system: **CMake ≥ 3.28** with presets (`core-release`, `windows-gui-release`).
- Linters: **clang-format 18.1.8** and **clang-tidy 18.1.8** (pinned; pip wheels).

---

## 4. Formatting — `.clang-format` is law

Formatting is **not** a matter of taste here; `clang-format` decides. CI fails on any
deviation (`clang-format --dry-run --Werror` over `src` + `tests`).

Run before every commit:

```bash
# format in place
clang-format -i $(git ls-files 'src/*.cpp' 'src/*.h' 'tests/*.cpp' 'tests/*.h')
# or just verify (what CI does)
clang-format --dry-run --Werror $(git ls-files 'src/*.cpp' 'src/*.h' 'tests/*.cpp' 'tests/*.h')
```

The config (LLVM-based) is tuned to the codebase rather than the reverse. Key choices and the
reason they exist:

| Setting | Value | Why |
|---|---|---|
| `ColumnLimit` | `120` | Wider than 80; matches the existing hand-wrapping. |
| `IndentWidth` / `UseTab` | `4` / `Never` | Spaces only. |
| `AlignAfterOpenBracket` | `DontAlign` | Keeps the `push_back(Type {.a = …, .b = …})` designated-init idiom attached to the call instead of churning every struct. |
| `Cpp11BracedListStyle` + `SpaceBeforeCpp11BracedList` | `true` / `true` | Produces `Type {…}` (space before `{`) — the project's signature brace style. |
| `BreakBeforeBinaryOperators` | `NonAssignment` | Operators lead continuation lines. |
| `PointerAlignment`/`ReferenceAlignment` | `Left` | `T* p`, `const T& r`. |
| `BreakStringLiterals` | `false` | Never split user-facing message strings. |
| `SortIncludes` / `IncludeBlocks` | `Never` / `Preserve` | Include **ordering is manual** (see §13); clang-format will not reorder it. |
| `InsertBraces` | `true` | Braces are mandatory on every control statement. |
| `AllowShort*OnASingleLine` | `false`/`None` | No one-line `if`, loop, function, or case. |

Do not add `// clang-format off` blocks except as a last resort, and explain why if you do.

### Formatter-sensitive patterns

Do not hand-predict how `clang-format` will wrap long statements. Shorten the code
structurally, especially in tests.

Prefer naming expected values or predicates:

```cpp
const auto expected_chapter_count = std::size_t {1};
test_support::expect_eq(chapters.size(), expected_chapter_count, "1 second should produce 1 chapter");

const bool shows_direct_syntax = contains(help.output, "VidChopperCLI.exe <input-video>");
test_support::expect_true(shows_direct_syntax, "help should show direct syntax");
```

Avoid long one-line casts or manually wrapped assertions that only exist to satisfy the column
limit:

```cpp
// avoid
test_support::expect_eq(chapters.size(), static_cast<std::size_t>(1), "1 second duration with 6 requested should produce 1 chapter");
```

When editing through a connector or any environment where `clang-format -i` cannot be run, do
not call the change ready until the formatter check is green. If CI reports a formatting error,
fix the **class of pattern** that caused it, not only the first line mentioned in the log.

---

## 5. Static analysis — `.clang-tidy`

- Scope: **`src/core` only** (`HeaderFilterRegex: 'src/core/.*\.h$'`). The Qt layer is compiled
  in CI but **not** tidied — clang-tidy can't parse Qt without the SDK and core-guideline checks
  fight moc/signal-slot code.
- `WarningsAsErrors: '*'` — a tidy warning in core is a build failure.
- Enabled families: `bugprone-*`, `performance-*`, `modernize-*`, `readability-*`,
  `cppcoreguidelines-*`, plus `clang-diagnostic-*`.
- The suppression list exists to protect the deliberate style (e.g. `readability-magic-numbers`,
  `readability-identifier-length`, `readability-function-cognitive-complexity`,
  `cppcoreguidelines-avoid-magic-numbers`, unchecked-container-access). Do **not** broaden
  suppressions to silence a real finding — fix the code instead. If a suppression is genuinely
  warranted in one spot, use a scoped `// NOLINTNEXTLINE(check-name)` **with a comment** saying
  why (see `FrameRate::display_frames_per_second` for the canonical example).

Run locally (mirrors CI):

```bash
for f in src/core/*.cpp; do
  clang-tidy "$f" -- -std=c++20 -Isrc
done
```

---

## 6. Naming conventions

| Entity | Convention | Example |
|---|---|---|
| Types, structs, enums, enum classes | `PascalCase` | `ChapterSegment`, `EncoderKind` |
| Enumerators | `PascalCase` | `EncoderKind::HevcNvenc` |
| Functions / methods | `snake_case` | `build_default_chapters`, `as_f64` |
| Variables / parameters | `snake_case` | `duration_ms`, `frame_rate` |
| Private data members | `snake_case_` (trailing underscore) | `chapter_model_`, `settings_store_` |
| Constants / `constexpr` tables | `snake_case` | `string_fields`, `enum_key::display_mode` |
| Namespaces | lower `snake_case` | `vidchopper`, `test_support`, `ffmpeg_arg` |
| Files | `snake_case.{h,cpp}` | `chapter_table_model.cpp` |
| Macros | avoid; only `Q_OBJECT` etc. from Qt | — |

Everything lives in `namespace vidchopper` (tests use `test_support` + `using namespace vidchopper;`).
Close every namespace with a `// namespace name` comment (clang-format enforces this).

Units belong in names: milliseconds are `*_ms`, kbps is `*_kbps`, seconds `*_seconds`.

---

## 7. Fixed-width type aliases and standard sizes

Use the project aliases from `core/types.h`, not raw `int`/`unsigned`:

```
u8 u16 u32 u64   i8 i16 i32 i64   f64   Path
```

- Pick the smallest type that fits the domain (`u8 default_chapter_count`, `u16 aac_bitrate_kbps`).
- Use `std::size_t` for STL container sizes, string positions, `reserve`, and indexes that are
  directly compared with `.size()` or `.find()` results. Do not hide this behind a project alias.
- Use `std::ptrdiff_t` for portable signed sizes when signed size math is genuinely required;
  `ssize_t` is POSIX-only and should not appear in portable Windows-targeted C++.
- Use `u64` for domain quantities that are conceptually 64-bit values, such as timestamps,
  durations, byte counts, or media sizes. Do not use `u64` merely because a container size happens
  to be 64-bit on current targets.
- Convert **explicitly** at boundaries with `static_cast`; never rely on implicit narrowing.
  Qt APIs use `int`, so casting at the Qt boundary is normal and expected
  (`static_cast<u8>(chapter_count_spin_->value())`).

---

## 8. "Almost always auto" with typed braced init

Declare with `auto` and a typed braced initializer rather than a leading type when the right-hand
side names the type directly:

```cpp
auto total = u64 {0};
auto names = std::vector<std::string> {};
auto dialog = AdvancedSettingsDialog {this};
auto index = std::size_t {0};
```

Not `u64 total = 0;` or `std::vector<std::string> names;`. The type is on the right, the
initializer is braced, and there is a space before `{`.

Do not use `auto` when the type is only implied by another variable or function return. Prefer the
explicit leading type in cases like `const char* const first = text.data();` or
`const std::optional<u64> parsed = parse_unsigned(value);`.

Do not force this rule onto plain `bool` locals. `clang-tidy` treats `auto flag = bool {false};`
as redundant casting. Use direct braced bool declarations instead:

```cpp
bool in_quotes {false};
bool previous_was_space {false};
```

---

## 9. Trailing return types — everywhere

Every function uses a trailing return type, **including `-> void`** and lambdas with a
non-trivial body:

```cpp
auto build_ui() -> void;
[[nodiscard]] auto current_output_directory() const -> std::filesystem::path;
auto to_lower_copy(std::string_view text) -> std::string;
// lambda
std::ranges::transform(in, out, [](unsigned char c) -> char { return …; });
```

Member function definitions repeat the trailing form (`auto MainWindow::build_ui() -> void {`).

---

## 10. const-correctness — leave no const unturned

`const` is self-documenting and forces deliberate change later. Apply it aggressively after
carefully reasoning about the logic:

- Mark every local that is not reassigned `const`.
- When the RHS names the type directly, `const auto` is correct
  (`const auto expected_count = std::size_t {3};`).
- When the type is only implied by another variable or function return, combine `const` with an
  explicit leading type (`const std::optional<u64> parsed = parse_unsigned(value);`).
- Mark parameters `const` where it documents intent (`const bool success`, `const u64 ms`).
- Mark member functions `const` whenever they don't mutate.
- Prefer `constexpr` for anything computable at compile time (see `FrameRate` accessors,
  the `std::to_array` setting tables, `ffmpeg_arg::` constants). Use `consteval` for functions
  that *must* run at compile time, and `constinit` for non-trivial statics that must avoid the
  static-init-order fiasco.
- Add `noexcept` to functions that cannot throw (the `constexpr` `FrameRate` accessors are
  `noexcept`).

Order of preference for a value: `constexpr` > `const` > mutable. Only drop `const` when
mutation is genuinely required.

---

## 11. Aggregates & designated initializers

Construct structs with C++20 designated initializers — it is the dominant idiom and the
formatter is tuned for it:

```cpp
chapters_.push_back(ChapterSegment {
    .name = "Chapter 1",
    .start_ms = 0,
    .end_ms = duration_ms,
});

const auto frame_rate = FrameRate {.numerator = 24, .denominator = 1};
```

- Give struct members **default member initializers** so partial designated-init is safe
  (`u64 start_ms {0};`).
- For value/aggregate structs, default the comparisons instead of hand-writing them:
  `[[nodiscard]] auto operator<=>(const T&) const = default;` (or `operator==` when ordering
  isn't meaningful). See `models.h`.
- Prefer a small **named options struct** (with designated init at the call site) over multiple
  positional `bool`/`int` parameters.

---

## 12. Modern C++ feature usage (the house dialect)

Use these in preference to hand-rolled equivalents:

- **`std::format`** for all string formatting (timecodes, zero-padded indices). Never
  `ostringstream` + `setw`/`setfill`.
- **`std::from_chars`** for integer parsing — no manual digit/overflow loops.
- **`std::ranges`** algorithms (`sort`, `find`, `any_of`, `transform`) and `std::erase_if`
  for erase-remove.
- **`starts_with`/`ends_with`** instead of substring comparisons.
- **`std::cmp_less` / `std::ssize`** to avoid signed/unsigned cast noise in index code.
- **`std::optional`** for fallible returns — **never** sentinel values (`-1`, empty string) or
  bool + out-parameter. Parsers return `std::optional<u64>`.
- **`std::to_array`** for fixed tables of constants (setting tables, command fragments).
- **`std::string_view`** for non-owning string parameters in core helpers; `QStringView` in
  Qt-heavy inspection helpers.
- **Concepts** to constrain templates (see `ScopedEnum` in `enum_utils.h`). Prefer a named
  concept over `enable_if`; it reads as documentation.
- **`[[nodiscard]]`** on every pure function and every fallible/validation/builder function.
- **`enum class`** with **explicit values** and a **fixed underlying type**, sized to the option
  count: a **two-state** mode uses `: bool` (`enum class SeekMode : bool { Accurate = false, Fast = true };`),
  a mode with three or more options uses `: u8` (`enum class EncoderKind : u8 { Auto = 0, X264 = 1, … };`).
  Enumerator values are persisted to the config (`0`/`1`/…), so they are a **stable contract** —
  never reorder or renumber them.

Centralize string constants that would otherwise drift (e.g. `ffmpeg_arg::` flag names); a typo
in one of many scattered literals is a real bug class.

---

## 13. Include ordering (manual, preserved)

`SortIncludes: Never` + `IncludeBlocks: Preserve` mean **you** order includes; group them with
blank lines in this order:

1. The matching header for this `.cpp` (`#include "qt/ui/main_window.h"` first in `main_window.cpp`).
2. Project headers (`"core/…"`, `"qt/…"`), grouped.
3. Qt headers (`<QtWidget>` style), grouped.
4. Standard library headers, grouped.

Use `#pragma once` in every header (not include guards). Include what you use; forward-declare
Qt classes in headers (`class QLabel;`) to keep header includes light, as `main_window.h` does.

---

## 14. File-local helpers

Put translation-unit-private helpers, constants, and enums in an **anonymous namespace** at the
top of the `.cpp` (see the `Column` enum in `chapter_table_model.cpp` and the setting tables in
`app_settings.cpp`). Don't expose them in headers.

---

## 15. Testing

The test harness is **hand-rolled and dependency-free** — keep it that way (no GTest/Catch).

- Helpers live in `tests/test_support.h`: `expect_true`, `expect_eq`, `fail`. Each test is a
  standalone `auto main() -> int` that returns `0` on success and throws on failure.
- **Golden tests** pin byte-exact output for anything meant to be behavior-preserving
  (timecode strings, ffmpeg argument streams, zero-padded names, INI round-trips, enum-clamp
  behavior). Add/extend them **before** a refactor that claims to be identical.
- **Compile-time tests** via `static_assert` are encouraged for `constexpr`/concept code
  (see `test_models.cpp`, `test_enum_clamp.cpp`).
- Labels: `fast` (pure unit, no external deps) and `slow` (the `ffmpeg` integration test).
  Default to `fast`; only the genuine ffmpeg end-to-end is `slow`.

Register a new test in `CMakeLists.txt`:

```cmake
add_vidchopper_test(vidchopper_test_<area> tests/test_<area>.cpp fast)
```

Run them:

```bash
ctest --test-dir build/core-release -C Release -L fast --output-on-failure
ctest --test-dir build/core-release -C Release -L slow --output-on-failure   # needs ffmpeg
```

---

## 16. The Qt layer — handle with care

- Keep all Qt out of `src/core`. UI talks to the core through plain C++ types.
- **Ownership:** parent every `QObject`/widget so Qt's tree owns it (`new QLabel {group}`); do
  not `delete` parented objects. Members that are Qt objects are raw pointers initialized to
  `{nullptr}` and created in `build_ui()`.
- **Signals/slots:** use the function-pointer `connect(...)` syntax; declare UI handlers as
  `private slots`. Block signals around programmatic widget updates that would re-enter
  (`spin->blockSignals(true) … false`).
- **String boundary:** convert at the edge — `QString::fromStdString` / `toStdString`, and
  `QString::fromStdWString` / `toStdWString` for `std::filesystem::path` on Windows. Keep core
  types (`std::string`, `std::filesystem::path`) inside the core; keep `QString` in the UI.
- Lean on the `.devin/skills/qt-cpp-review` linter and Qt docs for non-trivial Qt changes.

---

## 17. Paths & filesystem

- Represent paths as `std::filesystem::path` in the core; convert to/from `QString` only at the
  Qt boundary. The project-level `Path` alias is allowed when it keeps signatures readable.
- **Normalize slash direction.** Do not concatenate path strings with mixed `/` and `\`. Build
  paths with `std::filesystem::path` operators (`/`) or Qt's `QDir`, and present a single,
  consistent separator to the user. When displaying a path, normalize it
  (`path.make_preferred()` / `QDir::toNativeSeparators`) rather than emitting whatever the inputs
  happened to contain.

---

## 18. Comments

- Default to **no comment**; rely on good names. Most code needs none.
- When you do comment, explain the **why**, not the what, and keep it terse.
- Never write comments that only make sense as a diff ("now also handles X", "previously Y").
- Always document a `NOLINT`/`NOLINTNEXTLINE` with the reason it is safe.

---

## 19. Error handling

- Core: return `std::optional<T>` (or a small result/validation struct like `ValidationIssue`/
  the `validate_chapters` result) — no exceptions for control flow, no sentinels.
- UI: surface failures to the user with `QMessageBox` at the boundary and log to the activity
  log; don't let core failures pass silently.

---

## 20. Commits, PRs & CI

- **One theme per PR.** Keep diffs focused; avoid drive-by reformatting of unrelated files
  (the formatter already keeps the tree clean).
- For behavior-preserving refactors, **add golden tests first**, then refactor.
- CI must be **green** before merge: `lint` (clang-format + clang-tidy), `core-tests`
  (fast + slow), and `gui-build`. Run format + the fast tests locally before pushing.
- Never weaken tests, lint config, or security settings to make CI pass. Fix the code, or
  escalate if the test/spec is wrong.
- Enum values, INI keys, output file-name patterns, and ffmpeg argument order are **stable
  contracts** — change them only intentionally and call it out in the PR.
- Use neutral branch names only. Do not include personal handles, email stems, customer names,
  or secrets in branch names, commit messages, PR titles, or Linear updates.

### Whole-tree refactor workflow

When removing or renaming a symbol, type alias, file, flag, setting, or public behavior:

1. Search the whole repository for the exact old spelling before the first commit.
2. Update every affected target class: core, CLI, Qt GUI, tests, CMake, docs, and dummy fixtures.
3. Re-run or wait for all relevant checks before calling the task ready.
4. If CI fails, classify the failure pattern and sweep for similar instances before pushing again.

For example, removing a type alias from `core/types.h` is not complete until `src/qt`, all tests,
and slow integration tests have been checked for the old alias.

---

## 21. Quick command reference

```bash
# Configure + build the Qt-free core (what CI tests)
cmake --preset core-release
cmake --build --preset core-release

# Configure + build the Qt GUI (needs Qt 6.9 + a C++20 compiler)
cmake --preset windows-gui-release
cmake --build --preset windows-gui-release

# Tests
ctest --test-dir build/core-release -C Release -L fast --output-on-failure
ctest --test-dir build/core-release -C Release -L slow --output-on-failure

# Lint (pinned 18.1.8)
clang-format --dry-run --Werror $(git ls-files 'src/*.cpp' 'src/*.h' 'tests/*.cpp' 'tests/*.h')
for f in src/core/*.cpp; do clang-tidy "$f" -- -std=c++20 -Isrc; done

# Exact-spelling sweep before removing/renaming symbols
rg -n "old_symbol_or_alias" src tests CMakeLists.txt CODING_STYLE.md AGENTS.md
```

---

## 22. Pre-commit checklist

- [ ] Builds clean on the core preset (and GUI preset if Qt code changed).
- [ ] `clang-format --dry-run --Werror` is silent.
- [ ] `clang-tidy` is clean over `src/core`.
- [ ] Fast tests pass; slow tests pass if ffmpeg is available.
- [ ] New/changed behavior is covered by tests; intended-identical behavior is golden-locked.
- [ ] Long test assertions were shortened structurally instead of hand-wrapped.
- [ ] Removed/renamed symbols were searched across `src`, `tests`, CMake, docs, and fixtures.
- [ ] Trailing return types, project type aliases, designated initializers, and aggressive
      `const`/`constexpr` are applied.
- [ ] No new Qt include in `src/core`; UI ownership and string boundaries respected.
- [ ] No stable contract (enum values, INI keys, naming patterns, ffmpeg arg order) changed
      unintentionally.
- [ ] Branch name, commit message, PR text, and Linear update contain no personal identifiers,
      email stems, customer names, or secrets.
- [ ] Comments explain *why*; any `NOLINT` is justified.
