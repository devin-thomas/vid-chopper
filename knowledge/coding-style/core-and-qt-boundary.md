# Core And Qt Boundary

## Core Rules

- `src/core` owns domain logic and must compile without Qt.
- Do not add Qt headers or Qt types to `src/core`.
- Core code should use standard C++ strings and paths.

## Type And Construction Conventions

- Prefer project aliases such as `u8`, `u16`, `u64`, `usize`, and `f64`.
- Prefer `auto` with typed braced initialization.
- Use designated initializers for aggregates.
- Default comparisons on small value types where appropriate instead of hand-writing equality logic.

## Enum Rules

- Persisted enum values are explicit and stable.
- Two-state enums use `bool` as the underlying type.
- Three-or-more-state enums use `u8`.
- Do not reorder or renumber persisted values casually.

## String And Path Boundary

- Core code should operate on standard strings and `std::filesystem::path`.
- Convert to and from Qt types only at the UI boundary.
- Build paths structurally, not through slash-mixed string concatenation.
- Display user-facing paths with native separators.

## Qt Ownership And UI Rules

- Parent Qt widgets and other owned `QObject`s so the object tree manages lifetime.
- Use function-pointer `connect` syntax.
- Block signals around programmatic widget updates when they would re-enter handlers.
- Surface UI errors through the existing message-box/logging patterns instead of silent drops.
