# VidChopper C++20 Clarity and Safety Guide

| Metadata | Value |
|---|---|
| Linear issue | VID-45 |
| Audience | Engineering managers, maintainers, reviewers, and AI agents |
| Authority | This Markdown file is the source for the matching generated PDF. `CODING_STYLE.md` remains normative for code form. |
| Code baseline | `395c52b` (VID-40 shared process, probing, and queued Qt adapter integration) |
| Last verified | 2026-08-01 |
| Artifact pair | `docs/cpp20-clarity-and-safety.md` and `output/pdf/vidchopper-cpp20-clarity-and-safety-guide.pdf` |

## Purpose and authority

Use this guide to make implementation and review decisions without guessing which modern C++
features, ownership models, or framework boundaries VidChopper accepts. It summarizes the current
manager decision frame and points to code that already compiles in the repository.

The governing sources remain:

- [`CODING_STYLE.md`](../CODING_STYLE.md) for repository coding rules;
- [`CONTEXT.md`](../CONTEXT.md) for domain vocabulary;
- [ADR 0001](../knowledge/architecture/decisions/0001-shared-qt-free-engine.md) for the shared Qt-free
  engine boundary;
- [ADR 0002](../knowledge/architecture/decisions/0002-pinned-vcpkg-dependencies.md) for pinned JSON and
  YAML dependencies; and
- [ADR 0003](../knowledge/architecture/decisions/0003-windows-support-portability.md) for supported
  Windows behavior and portable shared interfaces.

If a proposed change conflicts with one of those sources, resolve the source-level decision first.
Do not use this guide to create a silent exception.

## Manager decision frame

| Decision | Default | Escalation condition |
|---|---|---|
| Language level | C++20 only | A separate toolchain and policy change is approved first. |
| Shared code | Portable values in the Qt-free engine | A new ADR changes the boundary. |
| Ownership | Values, references, RAII, or Qt parent ownership | Shared ownership is proven necessary. |
| Fallible operation | `std::optional` or a small result type | A richer error contract is required and tested. |
| Third-party code | Pinned in `vcpkg.json` and isolated behind an adapter | Dependency, licensing, packaging, and security review passes. |
| Stable behavior | Preserve enum values, settings, names, manifests, and command order | Linear explicitly accepts a compatibility change. |

Managers should reject a change that relies on a newer language mode, leaks Qt or a third-party type
into shared contracts, hides failure, or replaces an established contract without focused tests.

## Toolchain contract

`CMakeLists.txt` sets `CMAKE_CXX_STANDARD` to `20`, requires it, disables compiler extensions, and
requests `cxx_std_20` on the core and CLI targets. Supported verification uses MSVC 17 (Visual Studio
2022) and GCC 13. CMake 3.28 or newer is required. The GUI accepts Qt 6.7 or newer at configure time;
the repository verification baseline is Qt 6.9. Formatting and core static analysis use pinned
clang-format and clang-tidy 18.1.8.

Do not use C++23 library features such as:

- `std::expected`;
- `std::string::contains`;
- `std::to_underlying`; or
- `std::views::enumerate`.

Modules and coroutines are also outside the project dialect. Use the existing C++20 replacements:
small result structs, `find(...) != npos`, explicit checked casts, indexed loops, ordinary headers,
and explicit state machines.

**Schematic prohibited example - not part of a compiled target:**

```cpp
// Schematic only: both facilities exceed the project language contract.
auto load_job() -> std::expected<Job, Error>;
const bool has_name = text.contains("name");
```

## Values, ownership, and QObject lifetime

Use small values and aggregates for domain data. Use the fixed-width aliases in
`src/core/types.hpp` for domain quantities and `size_t` for STL sizes and positions. Give aggregate
members safe defaults, use designated initializers, and default comparisons when their semantics are
obvious.

**Compiled source excerpt - `src/core/models.hpp`:**

```cpp
enum class SeekMode : bool {
    Accurate = false,
    Fast = true,
};

struct FrameRate {
    u32 numerator {0};
    u32 denominator {1};

    [[nodiscard]] constexpr auto valid() const noexcept -> bool;
    [[nodiscard]] constexpr auto as_f64() const noexcept -> f64;
    [[nodiscard]] constexpr auto display_frames_per_second() const noexcept -> u32;

    [[nodiscard]] auto operator<=>(const FrameRate&) const = default;
};
```

Prefer values for ownership and `const T&` for non-owning access. A raw pointer must not silently own
a standard C++ object. For Qt objects, parent ownership is the explicit exception: construct widgets
and services with a parent, keep non-owning raw pointer observers where needed, and do not manually
delete parented objects. Use `QPointer` when an observed Qt object can be deleted independently.

Every `QObject` subclass must use `Q_DISABLE_COPY_MOVE`. Qt identity, signals, parentage, and moc
state must never be duplicated or moved.

**Compiled source excerpt - `src/qt/ui/main_window.cpp`:**

```cpp
MainWindow::MainWindow(DemoLaunchOptions demo_options, QWidget* parent)
    : QMainWindow(parent)
    , demo_options_(std::move(demo_options))
    , chapter_model_(new ChapterTableModel {this})
    , probe_coordinator_(new ProbeCoordinator {this})
    , export_coordinator_(new ExportCoordinator {this}) {
```

`MainWindow` follows the lifetime policy by constructing `ChapterTableModel`, `ProbeCoordinator`,
and `ExportCoordinator` with `this`, parenting widgets to their containing widgets, and observing
its transient settings dialog through `QPointer`.

**Schematic ownership anti-pattern - not part of a compiled target:**

```cpp
// Schematic only: the owner is ambiguous and QObject copying is invalid.
auto* settings = new ExportSettings;
auto copy = some_qobject;
delete child_widget;
```

## Optional values, errors, and results

Use `std::optional<T>` when absence is the complete failure contract, such as parsing one value. Do
not use `-1`, an empty string, or a `bool` plus output parameter as a sentinel. Callers must test
`has_value()` before dereferencing.

Use a named result struct when the caller needs state, diagnostics, or partial outputs. Examples are
`ValidationResult`, `ProcessResult`, `SettingsLoadResult`, and `ManifestWriteResult`. Give result
types an `ok() const noexcept` query when success has a stable meaning. Preserve specific errors at
the boundary instead of catching broadly or returning success-shaped defaults.

**Compiled source excerpt - `src/services/process_runner.hpp`:**

```cpp
struct ProcessResult {
    ProcessExitState state {ProcessExitState::FailedStart};
    i32 exit_code {0};
    std::string standard_output;
    std::string standard_error;
    std::string error_message;

    [[nodiscard]] auto ok() const noexcept -> bool;
    [[nodiscard]] auto operator==(const ProcessResult&) const -> bool = default;
};
```

Exceptions are not core control flow. A third-party adapter may catch the library's documented,
specific exception and translate it to a project result; `src/cli/chapter_config.cpp` does this for
JSON and YAML parsing. UI adapters surface failures through the established message-box and logging
paths.

**Schematic error anti-pattern - not part of a compiled target:**

```cpp
// Schematic only: failure is ambiguous and diagnostics are discarded.
auto parse_timestamp(std::string_view text, u64& value) -> bool;
```

## Checked arithmetic and narrowing

Parse external numbers into a wide type, validate their domain and target range, then narrow with
`static_cast`. Widen before multiplication when the source type is smaller. Never cast first and
validate the already-truncated value.

Guard addition and multiplication before performing them. Validate ordering before unsigned
subtraction. `std::from_chars` is the integer parsing default because it reports invalid input and
overflow without exceptions.

**Compiled source excerpt - `src/core/timecode.cpp`:**

```cpp
constexpr auto checked_add(const u64 left, const u64 right) noexcept -> std::optional<u64> {
    if (right > std::numeric_limits<u64>::max() - left) {
        return std::nullopt;
    }
    return left + right;
}

constexpr auto checked_multiply(const u64 left, const u64 right) noexcept -> std::optional<u64> {
    if (left != 0 && right > std::numeric_limits<u64>::max() / left) {
        return std::nullopt;
    }
    return left * right;
}
```

**Schematic arithmetic anti-pattern - not part of a compiled target:**

```cpp
// Schematic only: both operations can silently lose information.
const auto milliseconds = hours * 60 * 60 * 1000;
const auto chapter_count = static_cast<u8>(untrusted_count);
```

## Enums, ranges, constexpr, and noexcept

- Use `enum class` with explicit stable values and a fixed underlying type.
- Use `bool` for exactly two persisted states and `u8` for three or more states.
- Never reorder or renumber a persisted enum without a compatibility decision and tests.
- Prefer C++20 range algorithms such as `sort`, `find`, `any_of`, and `transform` over hand-written
  loops when the algorithm expresses the intent directly.
- Prefer `constexpr` for values and functions that can be evaluated at compile time.
- Add `noexcept` only when every operation in the function is genuinely non-throwing. Do not mark an
  allocating, formatting, filesystem, or third-party operation `noexcept` to satisfy style.

**Compiled source excerpt - `src/core/string_utils.cpp`:**

```cpp
auto to_lower_copy(std::string value) -> std::string {
    std::ranges::transform(value, value.begin(), [](const unsigned char character) -> char {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}
```

`FrameRate::display_frames_per_second()` is the model for working inside C++20: it uses an explicit
positive round-half-up expression because `std::lround` is not `constexpr` until C++23.

## Paths, includes, and headers

Use `Path` or `std::filesystem::path` for paths. Build paths with filesystem operations, not mixed
separator string concatenation. Normalize storage paths deliberately and convert to `QString` only
at the Qt boundary. Use wide-string conversion on Windows and native separators for display.

**Compiled source excerpt - `src/qt/ui/main_window.cpp`:**

```cpp
auto display_path(const std::filesystem::path& path) -> QString {
    return QDir::toNativeSeparators(QString::fromStdWString(path.wstring()));
}
```

All project headers use `.hpp` and `#pragma once`. A `.cpp` includes its matching header first, then
project headers, Qt headers, and standard library headers in manually preserved groups. Include what
the file uses. Prefer forward declarations for Qt classes in headers when a complete type is not
required. Do not introduce `.h` siblings, umbrella includes, or dependence on transitive includes.

**Schematic path anti-pattern - not part of a compiled target:**

```cpp
// Schematic only: separators and encoding are accidental.
const std::string output = directory + "\\" + filename;
```

## Qt, STL, platform, and third-party boundaries

The dependency direction is `src/qt -> shared Qt-free code`; shared code never includes Qt. Pass
plain values, `std::string`, containers, and filesystem paths through shared interfaces. Convert
`QString`, `QStringList`, Qt model indexes, signals, and widgets inside `src/qt`.

The CLI must remain usable without a Qt runtime. Windows-specific process and path behavior stays
behind CLI or platform adapters and must not enter shared contracts. This implements ADR 0001 and
ADR 0003 while the shared engine migration proceeds.

Production use of nlohmann-json and yaml-cpp is quarantined behind implementation adapters.
nlohmann-json is used by the shared probe service and CLI JSON adapters; yaml-cpp remains in the CLI
chapter-config adapter. Their versions are resolved through the pinned `vcpkg.json` baseline, and
their types do not appear in public shared headers. New dependencies or alternate package sources
require explicit review of the manifest, baseline, licenses, package contents, failure translation,
and all affected CI lanes. Do not add a system fallback or an ad hoc configure-time download.

## AI-agent and reviewer protocol

An AI agent or human reviewer must:

1. Read the Linear acceptance criteria, `CONTEXT.md`, relevant ADRs, and `CODING_STYLE.md`.
2. Search the whole repository for the symbol, type, setting, and dependency being changed.
3. Classify every code sample as compiled production/test code or explicitly schematic.
4. Preserve stable contracts with focused golden tests before behavior-preserving refactors.
5. Check core, CLI, Qt, tests, CMake, documentation, fixtures, and packaging where the change can
   propagate.
6. Run the repository verification lane appropriate to each affected target and report only checks
   actually observed.
7. Reject broad `NOLINT`, formatter suppression, weakened gates, silent fallbacks, and unsupported
   language features.
8. Escalate an unresolved policy conflict instead of choosing a convenient interpretation.

Review evidence must name the source files inspected, commands run, observed results, and any lane
left to CI. An assertion that code "looks safe" is not evidence.

## Review checklist

- [ ] The change compiles as C++20 with extensions disabled and uses no prohibited feature.
- [ ] Shared interfaces expose no Qt, Windows-only, or third-party type.
- [ ] Ownership is value/RAII-based or explicitly follows Qt parent ownership.
- [ ] Every `QObject` subclass disables copying and moving.
- [ ] Optional absence and result diagnostics have distinct, documented meanings.
- [ ] External numbers are range-checked before narrowing; arithmetic cannot wrap silently.
- [ ] Persisted enum values and other stable contracts remain compatible or change explicitly.
- [ ] Paths use filesystem types and Qt conversion occurs only at the boundary.
- [ ] Headers use `.hpp`, `#pragma once`, matching-header-first order, and direct includes.
- [ ] Third-party dependencies remain pinned, quarantined, packaged, and license-audited.
- [ ] Relevant format, static analysis, build, and test evidence is recorded.
- [ ] Documentation examples compile in an existing target or are visibly marked schematic.

## Phased adoption

1. **New code:** apply every rule immediately; do not add new debt.
2. **Touched code:** improve the local ownership, error, arithmetic, and boundary contract within the
   issue scope, protected by existing or new tests.
3. **Shared-engine migration:** move workflow behavior behind portable interfaces in the sequence
   established by ADR 0001; preserve GUI and CLI behavior with golden coverage.
4. **Legacy cleanup:** schedule broad consistency work as focused Linear issues. Do not mix a
   whole-tree modernization into an unrelated feature or release fix.

## Definition of done

A C++ change is done only when:

- its language and toolchain requirements stay inside the documented C++20 baseline;
- ownership, error handling, arithmetic, paths, and dependency boundaries are explicit;
- all affected targets build and their relevant tests and static checks pass;
- stable behavior is either preserved by evidence or intentionally changed by acceptance criteria;
- no formatter, lint, security, or test policy was weakened;
- code examples and documentation references match the current tree; and
- the PR reports the exact verification evidence and any remaining external gate.

This guide's publication is complete when its Markdown/PDF pair is freshness-checked by the shared
documentation tooling and every rendered page passes visual inspection.
