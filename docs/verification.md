# Local verification

The repository-owned PowerShell scripts are the canonical local workflow. Run them from the repository root.

```powershell
.\tools\bootstrap.ps1
.\tools\verify.ps1 -Tier Quick
.\tools\verify.ps1 -Tier Full
.\tools\verify.ps1 -Tier Release
```

`bootstrap.ps1` installs the pinned clang tools into `.venv-tools` and pins the repository-local `.vcpkg` checkout to the manifest baseline. It reports exact remediation for missing CMake, MSVC, Qt 6.9, Node, npm, ffmpeg, and ffprobe. It does not silently install Visual Studio, Qt, Node, or media SDKs.

On Windows, the scripts automatically discover the newest Qt 6.9 MSVC 2022 kit under `C:\Qt`. Custom installations can be selected with `Qt6_ROOT`, `Qt6_DIR`, or `QTDIR`; the selection applies only to the verification process and does not modify global environment settings.

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
