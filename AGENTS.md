# Agent Workflow for VidChopper

This file is for ChatGPT/Codex/Claude-style agents working through GitHub and Linear connectors. It complements `CODING_STYLE.md`; it does not replace it.

## Non-negotiables

- Use neutral branch names only. Do not include personal handles, email stems, customer names, or secrets.
- Keep one Linear task to one focused branch and PR unless the user explicitly asks for a broader batch.
- Do not claim checks passed unless GitHub Actions or the user-provided CI logs confirm it.
- Do not weaken lint, tests, or security policy to make a PR pass.
- Do not use `// clang-format off` or broad `NOLINT` as a shortcut.

## Branch naming

Use issue-number-first neutral names:

```text
vid-13-cli-command-contract
vid-14-cli-settings-precedence
vid-16-config-loader
```

Avoid names that contain personal identifiers or generated Linear branch stems with user-specific prefixes.

## Start-of-task checklist

1. Fetch the Linear issue and read the acceptance criteria.
2. Review the current `main` branch before assuming the task is not already partly complete.
3. Create or reuse a neutral branch from current `main`.
4. Update Linear to `In Progress` and record the neutral branch name in the issue or a comment.
5. Keep the implementation scoped to the issue. If related cleanup is necessary, call it out clearly.

## GitHub connector write discipline

- Fetch the current file before updating it and use the returned blob SHA.
- Do not run concurrent writes to the same path.
- Prefer small, sequential commits with clear messages.
- After changing a shared file, use the returned `content_sha` or refetch before another update.
- Compare the branch against `main` before opening the PR to verify the changed-file list is expected.

## Formatting workflow

`clang-format` is the authority. When editing without a local checkout, avoid patterns that are likely to churn:

- Avoid long one-line `expect_eq` / `expect_true` assertions.
- Prefer named expected values and named predicates.
- Do not manually wrap by guessing alignment.
- If CI reports a formatter failure, fix the pattern across the changed file instead of only one reported line.

Good test style:

```cpp
const auto expected_count = std::size_t {1};
test_support::expect_eq(chapters.size(), expected_count, "1 second should produce 1 chapter");

const bool shows_usage = contains(output, "Usage:");
test_support::expect_true(shows_usage, "invalid invocation should print usage");
```

## Whole-tree refactor workflow

Before removing or renaming any symbol, alias, setting, flag, enum value, file, or target:

1. Search the whole repository for the exact spelling.
2. Update every affected layer: `src/core`, `src/cli`, `src/qt`, `tests`, CMake, docs, and fixtures.
3. Think through all CI lanes: format, clang-tidy, core build, GUI build, fast tests, slow tests.
4. If one CI lane fails, inspect whether the same pattern exists in other lanes before pushing a fix.

Examples:

- Removing a type alias from `core/types.h` requires checking Qt files and tests, not only core.
- Replacing shell command execution on Windows requires checking paths with spaces and slow ffmpeg tests.
- Changing CLI parsing requires checking parser tests, app-level behavior tests, usage text, and CMake registration.

## PR workflow

1. Open the PR as draft unless the user explicitly asks for ready-for-review.
2. Include the Linear issue ID in the PR body.
3. Summarize what changed and what was intentionally left out.
4. State whether tests were actually run. If not run, list the expected CI commands instead.
5. Wait for CI or user-provided logs before marking Linear done.

## CI failure triage

Read failures as signal, not noise:

- Formatter failures usually mean the code should be shortened structurally.
- MSVC failures often catch narrowing, Windows-only headers, path quoting, or Qt boundary issues.
- Slow test failures often involve real tool behavior, path handling, or process invocation.
- A compile error after removing a symbol means the sweep was incomplete.

After a failed run, fix one coherent class of failure, then summarize the root cause in Linear or the PR thread.

## Linear updates

Keep Linear comments factual and non-sensitive:

- Mention the neutral branch and PR number.
- Mention the exact acceptance criteria covered.
- Mention checks only when they are confirmed.
- Do not paste long CI logs unless they are necessary for diagnosis.
