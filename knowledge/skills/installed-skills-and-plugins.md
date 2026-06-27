# Installed Skills And Plugins

## Relevant Skills In This Environment

- `codebase-map`
  - path: `C:\Users\lilgo\.agents\skills\codebase-map\SKILL.md`
  - purpose: structural repo indexing and context slicing
  - caveat: best suited to TS/JS/Python/Go/Rust repos, so in VidChopper it is more a mapping method than a turnkey C++ indexer
- `shared-canon-memory`
  - path: `C:\Users\lilgo\.agents\skills\shared-canon-memory\SKILL.md`
  - purpose: durable multi-agent memory and handoff discipline
- `headroom-agent-compression`
  - path: `C:\Users\lilgo\.codex\skills\headroom-agent-compression\SKILL.md`
  - purpose: reduce context cost through local proxy or wrapper modes
- `qt-project`
  - path: `C:\Users\lilgo\.codex\skills\qt-project\SKILL.md`
  - purpose: Qt 6 CMake/project structure and wiring
- `qt-cpp-review`
  - path: `C:\Users\lilgo\.codex\skills\qt-cpp-review\SKILL.md`
  - purpose: structured Qt/C++ correctness review
- `skill-installer`
  - path: `C:\Users\lilgo\.codex\skills\.system\skill-installer\SKILL.md`
  - purpose: install curated or GitHub-hosted skills into the local Codex skill roots

## Relevant Plugins

- Linear
  - used for issue tracking and team documents
- GitHub
  - useful for remote repo inspection and repository operations

## How Future Agents Should Use The Skills

Minimum process:

1. Confirm the skill appears in the current environment’s skill list.
2. Read the `SKILL.md` fully before acting.
3. Read any referenced support file the skill requires.
4. Use the smallest relevant skill set for the task instead of loading everything by habit.

Recommended mapping for VidChopper:

- repo orientation -> `knowledge/README.md` + `knowledge/architecture/repo-map.md`
- long-running continuity -> `shared-canon-memory`
- heavy-context sessions -> `headroom-agent-compression`
- Qt build wiring -> `qt-project`
- Qt/C++ review -> `qt-cpp-review`

## How To Install Or Recover Skills

There are two paths:

### Curated or GitHub-installable skills

Use the `skill-installer` helper scripts. The skill doc states that these scripts require network access.

Typical flow:

1. Read `C:\Users\lilgo\.codex\skills\.system\skill-installer\SKILL.md`.
2. List available curated skills.
3. Install the requested skill into the local Codex skill directory.
4. Restart Codex so the new skill appears in the session inventory.

### Machine-local custom skills

Some useful skills here live under local skill roots rather than a curated remote catalog, such as:

- `C:\Users\lilgo\.agents\skills`
- `C:\Users\lilgo\.codex\skills`

If a future session is missing one of those skills, the operator needs to restore it into a recognized
skill root or re-expose it through Codex. A plain repo clone is not enough unless the skill ends up in
one of the configured skill directories.
