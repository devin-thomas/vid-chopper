# Local verification

The repository-owned PowerShell scripts are the canonical local workflow. Run them from the repository root.

```powershell
.\tools\bootstrap.ps1
.\tools\verify.ps1 -Tier Quick
.\tools\verify.ps1 -Tier Full
.\tools\verify.ps1 -Tier Release
```

`bootstrap.ps1` installs the pinned clang tools into `.venv-tools` and pins the repository-local `.vcpkg` checkout to the manifest baseline. It reports exact remediation for missing CMake, MSVC, Qt 6.9, Node, npm, ffmpeg, and ffprobe. It does not silently install Visual Studio, Qt, Node, or media SDKs.

On Windows, the scripts automatically discover the newest Qt 6.9 MSVC 2022 kit under `C:\Qt`. Custom installations can be selected with `Qt6_ROOT`, `Qt6_DIR`, `QTDIR`, or `QT_ROOT_DIR`; the selection applies only to the verification process and does not modify global environment settings.

`Quick` checks formatting, static policy and the Qt-free boundary, builds the core and CLI, runs fast tests, and type-checks/builds the Pages site. `Full` adds slow media tests, CLI end-to-end fixtures, the GUI build, and a seeded noninteractive GUI startup. `Release` adds deterministic demo capture, version and manifest checks, Markdown/PDF freshness, and package assembly/audit.

Use `-Fix` only with `Quick` to apply the pinned formatter before running every remaining Quick stage:

```powershell
.\tools\verify.ps1 -Tier Quick -Fix
```

Each stage fails fast with a named `FAILED STAGE` message and a nonzero exit code.

## Optional pre-push hook

Install the repository-local hook without changing global Git configuration:

```powershell
.\tools\install-hooks.ps1
```

The hook runs the Quick tier. Remove only the managed VidChopper hook with:

```powershell
.\tools\install-hooks.ps1 -Remove
```

The installer refuses to overwrite or remove an unmanaged `pre-push` hook.

## GitHub Actions lanes

GitHub Actions calls the same verification script with focused lanes:

```powershell
.\tools\verify.ps1 -CiLane Lint
.\tools\verify.ps1 -CiLane Core
.\tools\verify.ps1 -CiLane Gui
.\tools\verify.ps1 -CiLane Docs
```

The Lint lane uses the pinned formatter and static-analysis versions and discovers tracked `.cpp` and `.hpp` files. The Core lane builds the CLI and core, runs fast tests, slow ffmpeg-backed tests, and CLI fixtures. The GUI lane performs a fresh Qt build and seeded startup. The Docs lane runs `npm ci`, type checks, and the production build.

Pull requests use path filtering so docs-only changes skip the C++ lanes. Workflow, tool, source, test, build, and packaging changes run the full code lanes. Superseded runs for the same pull request are canceled, and a failing lane uploads a short diagnostic tail for seven days.
