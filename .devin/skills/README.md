# Agent skills (Qt AI Skills, Devin-adapted)

This directory holds [Devin agent skills](https://docs.devin.ai/cli/extensibility/skills/overview)
that Devin auto-discovers (`.devin/skills/<name>/SKILL.md`) and can
invoke during a session. The skills here are adapted from
[TheQtCompanyRnD/agent-skills](https://github.com/TheQtCompanyRnD/agent-skills).

## Installed skills

Only the skills relevant to this repository (a Qt 6 **Widgets** C++ /
CMake app — no QML) are installed:

| Skill | Type | Use it for |
|-------|------|------------|
| `qt-cpp-review` | Review | Read-only Qt6 C++ review: a deterministic Python linter (60+ rules) plus six deep-analysis missions (model contracts, ownership, threading, API correctness, error handling, performance). |
| `qt-cpp-docs` | Process | Generate Markdown reference docs for Qt/C++ source files (classes, headers, free functions, `main.cpp`). |
| `qt-project` | Conceptual | Create/edit Qt 6 CMake projects and `CMakeLists.txt` using modern `qt_*` commands (avoids `qt5_*`/qmake-isms). |

The upstream repo also ships QML- and Figma-focused skills
(`qt-qml*`, `qt-figma*`, `qt-ui-design`, `qt-qml-profiler`). They were
**not** installed because this project has no QML/Quick UI. Ask if you
want them added.

## What "Devin-adapted" means

The upstream `SKILL.md` files were written for Claude Code. The
content is unchanged in substance; only the tool/runtime references
were mapped to Devin:

- "Launch six parallel subagents" → Devin **child sessions** (via the
  `managing-child-sessions` skill) **or** sequential in-session
  analysis passes.
- `AskUserQuestion` → `message_user` with `content_type: "user_question"`.
- `Glob` / `Bash` tools → Devin's `grep` / `exec` tools.
- Linter path is given repo-relative
  (`.devin/skills/qt-cpp-review/references/lint-scripts/qt_review_lint.py`);
  the interpreter is `python` on Windows, `python3` on Linux/macOS.
- The Qt docs MCP lookup maps to a Devin MCP server named `qt-docs`
  (if configured) with a browser-based web-fetch fallback.

Each adapted `SKILL.md` has a short "Running under Devin" note near
the top; each skill's `README.md` has a **Devin** row in its install
table.

## Optional: Qt documentation MCP

`qt-project` (and others) can consult Qt's documentation MCP server.
Upstream `.mcp.json` points at the HTTP server
`https://qt-docs-mcp.qt.io/mcp` (name `qt-docs`). It is optional —
the skills fall back to the browser / bundled references when it is
absent. Configure it in your Devin org/repo MCP settings if you want it.

## Provenance & license

- Source: <https://github.com/TheQtCompanyRnD/agent-skills>
- Upstream commit: `6e3411d7e58965aa31fa3803c398511f27b29216`
- License: `LicenseRef-Qt-Commercial OR BSD-3-Clause` (see each skill's
  `LICENSE.txt`). Using these skills under Qt commercial licensing is
  subject to the [Qt AI Services Terms & Conditions](https://www.qt.io/terms-conditions/ai-services-2025-06).
- These skills use AI and can make mistakes — always review their output.
