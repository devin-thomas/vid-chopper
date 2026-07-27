# Agent Workflow

This file captures the repo-local operating rules that matter for future sessions. It is the practical
blend of the local `CODEX.md`, the current `AGENTS.md` instructions, and the way this repo has already
been worked.

## Default Mode

- Deliver working code, not just plans.
- Treat the Linear `vid-chopper` project as the authoritative source for current roadmap state,
  issue status, ordering, and acceptance criteria.
- Treat repository planning documents as dated snapshots; reconcile them to Linear rather than
  inferring task status from stale branch or handoff text.
- Gather enough context up front to avoid thrashing.
- Prefer repo-native patterns and existing helpers over new abstractions.
- If a dedicated tool exists for the action, prefer it over shell improvisation.

## Exploration Rules

- Prefer `rg` and `rg --files` for search.
- Batch reads when possible instead of reading files one by one.
- Use parallel tool calls for independent reads and status checks.
- Re-read actual files before irreversible writes if a handoff or summary may be stale.

## Editing Rules

- Use `apply_patch` for manual edits.
- Default to ASCII unless a file clearly needs otherwise.
- Do not revert unrelated user changes.
- Avoid destructive git commands unless the user explicitly requests them.

## Publish Rules

- A publish request is not done until a commit is pushed or a PR/release URL exists.
- Validate before pushing when the repo provides meaningful checks.
- Treat decisive network actions such as `git push` and `gh release create` as the real status signal.

## Future-Agent Checklist

1. Read `knowledge/README.md`.
2. Fetch the relevant Linear issue and its acceptance criteria.
3. Read the relevant coding-style split docs.
4. Read the dated snapshot in `knowledge/history/features-and-progress.md`; use `features_plan.md`
   only as the archived `v0.2.0-alpha` record.
5. Check `git status --short --branch`.
6. Preserve unrelated untracked or dirty user files unless the user explicitly says otherwise.
