# Coding Style Overview

`CODING_STYLE.md` is the repo source of truth. This directory splits the guidance into shorter files that
future agents can scan quickly.

## Priorities

VidChopper resolves tradeoffs in this order:

1. Speed
2. Clarity
3. Brevity

## Non-Negotiables

- Keep `src/core` Qt-free.
- Stay on C++20, not C++23.
- Use project fixed-width aliases from `src/core/types.h`.
- Use trailing return types everywhere, including `-> void`.
- Prefer `auto` with typed braced initialization.
- Apply `const`, `constexpr`, `[[nodiscard]]`, and explicit narrowing conversions aggressively.
- Use designated initializers for aggregates.
- Persist enum values explicitly.
- Use `bool` as the underlying type for two-state enums.
- Keep include ordering manual and stable.

## Read Next

- [`knowledge/coding-style/core-and-qt-boundary.md`](core-and-qt-boundary.md)
- [`knowledge/coding-style/testing-and-quality-gates.md`](testing-and-quality-gates.md)
