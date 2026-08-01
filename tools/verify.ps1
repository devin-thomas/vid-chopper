param(
    [ValidateSet("Quick", "Full", "Release")][string]$Tier = "Quick",
    [ValidateSet("None", "Lint", "Core", "Gui", "Docs")][string]$CiLane = "None",
    [switch]$Fix
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "verification-common.ps1")

$repoRoot = Get-RepoRoot

function Get-CppFiles {
    $files = @(& git -C $repoRoot ls-files "src/*.cpp" "src/*.hpp" "tests/*.cpp" "tests/*.hpp")
    if ($LASTEXITCODE -ne 0 -or $files.Count -eq 0) {
        throw "No tracked C++ source files were found."
    }
    return $files
}

function Invoke-TextPolicyChecks {
    $sizeMatches = @(& git -C $repoRoot grep -n "std::size_t" -- "src" "tests")
    if ($LASTEXITCODE -gt 1) {
        throw "git grep failed while checking size_t style."
    }
    $violations = @($sizeMatches | Where-Object { $_ -notmatch "src[\\/]core[\\/]types\.hpp:.*using std::size_t;" })
    if ($violations.Count -gt 0) {
        throw "Unqualified size_t policy violations:`n$($violations -join "`n")"
    }

    $qtMatches = @(& git -C $repoRoot grep -n -E '#include[[:space:]]*[<"]Q[^>"]*[>"]' -- "src/core" "src/cli")
    if ($LASTEXITCODE -gt 1) {
        throw "git grep failed while checking the Qt-free boundary."
    }
    if ($qtMatches.Count -gt 0) {
        throw "Qt-free boundary violations:`n$($qtMatches -join "`n")"
    }
}

function Invoke-ReviewFindingChecks {
    $patterns = @(
        @{ Name = "duplicate enum clamping"; Pattern = "safe_enum_cast|clamp_enum" },
        @{ Name = "switch defaults that hide new enumerators"; Pattern = "^[[:space:]]*default:" },
        @{ Name = "Windows-unsafe min/max calls"; Pattern = "std::(min|max)(<[^>]+>)?\(" },
        @{ Name = "chapter-plan copies"; Pattern = "const auto chapters = chapter_model_->chapters\(\)" }
    )
    foreach ($check in $patterns) {
        $matches = @(& git -C $repoRoot grep -n -E $check.Pattern -- "src")
        if ($LASTEXITCODE -gt 1) {
            throw "git grep failed while checking $($check.Name)."
        }
        if ($matches.Count -gt 0) {
            throw "Deterministic review finding ($($check.Name)):`n$($matches -join "`n")"
        }
    }

    $qobjectHeaders = @(& git -C $repoRoot grep -l "Q_OBJECT" -- "src/qt/*.hpp")
    if ($LASTEXITCODE -gt 1) {
        throw "git grep failed while checking QObject ownership declarations."
    }
    foreach ($header in $qobjectHeaders) {
        $copyMoveDeclaration = @(& git -C $repoRoot grep -n "Q_DISABLE_COPY_MOVE" -- $header)
        if ($LASTEXITCODE -gt 1) {
            throw "git grep failed while checking $header."
        }
        if ($copyMoveDeclaration.Count -eq 0) {
            throw "Deterministic review finding (QObject copy/move not disabled): $header"
        }
    }
}

function Invoke-FormattingChecks {
    Invoke-VerificationStage -Name "Pinned clang-format" -Action {
        $clangFormat = Get-RepoCommand -Name "clang-format" -Remediation "Run tools/bootstrap.ps1."
        $versionOutput = (& $clangFormat --version) -join " "
        if ($versionOutput -notmatch [regex]::Escape($script:ToolVersions.Clang)) {
            throw "Expected clang-format $($script:ToolVersions.Clang), got: $versionOutput"
        }
        $files = Get-CppFiles
        if ($Fix) {
            Invoke-RepoCommand -FilePath $clangFormat -ArgumentList (@("-i") + $files)
        }
        Invoke-RepoCommand -FilePath $clangFormat -ArgumentList (@("--dry-run", "--Werror") + $files)
    }
}

function Invoke-StaticChecks {
    Invoke-VerificationStage -Name "Static policy checks" -Action {
        Invoke-TextPolicyChecks
        Invoke-ReviewFindingChecks
        $clangTidy = Get-RepoCommand -Name "clang-tidy" -Remediation "Run tools/bootstrap.ps1."
        $versionOutput = (& $clangTidy --version) -join " "
        if ($versionOutput -notmatch [regex]::Escape($script:ToolVersions.Clang)) {
            throw "Expected clang-tidy $($script:ToolVersions.Clang), got: $versionOutput"
        }
        if ($IsWindows) {
            Invoke-RepoCommand -FilePath $clangTidy -ArgumentList @("--verify-config")
            Write-Host "Windows validates the pinned clang-tidy configuration; the Linux Quick lane analyzes every core translation unit against GCC 13."
        } else {
            foreach ($source in @(& git -C $repoRoot ls-files "src/core/*.cpp")) {
                $arguments = @($source, "--", "-std=c++20", "-Isrc")
                $gcc13Root = "/usr/lib/gcc/x86_64-linux-gnu/13"
                if (Test-Path -LiteralPath $gcc13Root) {
                    $arguments += "--gcc-install-dir=$gcc13Root"
                }
                Invoke-RepoCommand -FilePath $clangTidy -ArgumentList $arguments
            }
        }
    }
}

function Invoke-CoreBuild {
    Invoke-VerificationStage -Name "Core and CLI build" -Action {
        $cmake = Get-RepoCommand -Name "cmake" -Remediation "Install CMake 3.28 or newer."
        Invoke-RepoCommand -FilePath $cmake -ArgumentList @("--fresh", "--preset", "core-release")
        Invoke-RepoCommand -FilePath $cmake -ArgumentList @("--build", "--preset", "core-release")
    }
}

function Invoke-FastTests {
    Invoke-VerificationStage -Name "Fast tests" -Action {
        $ctest = Get-RepoCommand -Name "ctest" -Remediation "Install CMake 3.28 or newer."
        Invoke-RepoCommand -FilePath $ctest -ArgumentList @(
            "--test-dir", "build/core-release", "-C", "Release", "-L", "fast", "--output-on-failure"
        )
    }
}

function Invoke-DocsChecks {
    Invoke-VerificationStage -Name "Agent skill contract" -Action {
        & (Join-Path $repoRoot "tools\agent-skill-artifacts.ps1") -Mode Check
        if ($LASTEXITCODE -ne 0) {
            throw "Agent skill contract validation failed."
        }
    }
    Invoke-VerificationStage -Name "Docs type and build checks" -Action {
        $npm = Get-RepoCommand -Name "npm" -Remediation "Install Node.js 22 with npm."
        Invoke-RepoCommand -FilePath $npm -ArgumentList @("ci") -WorkingDirectory (Join-Path $repoRoot "docs")
        Invoke-RepoCommand -FilePath $npm -ArgumentList @("run", "build") -WorkingDirectory (Join-Path $repoRoot "docs")
        Invoke-RepoCommand -FilePath $npm -ArgumentList @("run", "build:pages") -WorkingDirectory (Join-Path $repoRoot "docs")
    }
}

function Invoke-QuickTier {
    Invoke-FormattingChecks
    Invoke-StaticChecks
    Invoke-CoreBuild
    Invoke-FastTests
    Invoke-DocsChecks
}

function Invoke-GuiStartupCheck {
    $ffmpeg = Get-RepoCommand -Name "ffmpeg" -Remediation "Install ffmpeg 7.1.1 and add it to PATH."
    $executable = Join-Path $repoRoot "build\windows-gui-release\Release\VidChopper.exe"
    if (-not (Test-Path -LiteralPath $executable)) {
        throw "GUI executable was not found at $executable."
    }

    $workspace = Join-Path $repoRoot "tmp\verify-gui-startup"
    $sample = Join-Path $workspace "sample.mp4"
    $ready = Join-Path $workspace "ready.txt"
    New-Item -ItemType Directory -Force -Path $workspace | Out-Null
    if (Test-Path -LiteralPath $ready) {
        Remove-Item -LiteralPath $ready -Force
    }
    Invoke-RepoCommand -FilePath $ffmpeg -ArgumentList @(
        "-y", "-f", "lavfi", "-i", "testsrc=size=320x180:rate=24", "-t", "1", "-pix_fmt", "yuv420p", $sample
    )

    $process = Start-Process -FilePath $executable -ArgumentList @(
        "--demo-scene=workspace", "--demo-source=$sample", "--window-size=800x600", "--demo-ready-file=$ready"
    ) -PassThru -WindowStyle Hidden
    try {
        $deadline = [DateTime]::UtcNow.AddSeconds(20)
        while ([DateTime]::UtcNow -lt $deadline) {
            if (Test-Path -LiteralPath $ready) {
                $status = (Get-Content -Raw -LiteralPath $ready).Trim()
                if ($status -ne "ready") {
                    throw "Seeded GUI startup returned '$status'."
                }
                return
            }
            if ($process.HasExited) {
                throw "Seeded GUI exited before writing its ready marker."
            }
            Start-Sleep -Milliseconds 200
        }
        throw "Seeded GUI did not write its ready marker within 20 seconds."
    }
    finally {
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id
        }
    }
}

function Set-VerificationQtEnvironment {
    Invoke-VerificationStage -Name "Qt 6.9 environment" -Action {
        $qtRoot = Set-RepoQtEnvironment
        Write-Host "Using Qt from $qtRoot"
    }
}

function Invoke-SlowTests {
    Invoke-VerificationStage -Name "Slow ffmpeg tests" -Action {
        $ctest = Get-RepoCommand -Name "ctest" -Remediation "Install CMake 3.28 or newer."
        Invoke-RepoCommand -FilePath $ctest -ArgumentList @(
            "--test-dir", "build/core-release", "-C", "Release", "-L", "slow", "--output-on-failure"
        )
    }
}

function Invoke-CliFixtures {
    Invoke-VerificationStage -Name "CLI end-to-end fixtures" -Action {
        $ctest = Get-RepoCommand -Name "ctest" -Remediation "Install CMake 3.28 or newer."
        Invoke-RepoCommand -FilePath $ctest -ArgumentList @(
            "--test-dir", "build/core-release", "-C", "Release", "-R",
            "vidchopper_test_cli_contract|vidchopper_test_ffmpeg_integration", "--output-on-failure"
        )
    }
}

function Invoke-GuiBuild {
    Invoke-VerificationStage -Name "GUI build" -Action {
        $cmake = Get-RepoCommand -Name "cmake" -Remediation "Install CMake 3.28 or newer."
        Invoke-RepoCommand -FilePath $cmake -ArgumentList @("--fresh", "--preset", "windows-gui-release")
        Invoke-RepoCommand -FilePath $cmake -ArgumentList @("--build", "--preset", "windows-gui-release")
    }
}

function Invoke-GuiTests {
    Invoke-VerificationStage -Name "Qt settings tests" -Action {
        $ctest = Get-RepoCommand -Name "ctest" -Remediation "Install CMake 3.28 or newer."
        Invoke-RepoCommand -FilePath $ctest -ArgumentList @(
            "--test-dir", "build/windows-gui-release", "-C", "Release", "-L", "qt", "--output-on-failure"
        )
    }
}

function Invoke-GuiStartup {
    Invoke-VerificationStage -Name "Seeded noninteractive GUI startup" -Action {
        Invoke-GuiStartupCheck
    }
}

function Invoke-FullTier {
    Invoke-QuickTier
    Set-VerificationQtEnvironment
    Invoke-SlowTests
    Invoke-CliFixtures
    Invoke-GuiBuild
    Invoke-GuiTests
    Invoke-GuiStartup
}

function Invoke-VersionChecks {
    $cmakeText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "CMakeLists.txt")
    $docsPackage = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "docs\package.json") | ConvertFrom-Json
    $vcpkgManifest = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "vcpkg.json") | ConvertFrom-Json
    if ($cmakeText -notmatch 'VIDCHOPPER_DISPLAY_VERSION "([^"]+)"') {
        throw "VIDCHOPPER_DISPLAY_VERSION was not found in CMakeLists.txt."
    }
    $displayVersion = $Matches[1]
    if ([string]$docsPackage.version -ne $displayVersion) {
        throw "docs/package.json version '$($docsPackage.version)' does not match display version '$displayVersion'."
    }
    if (-not $displayVersion.StartsWith([string]$vcpkgManifest.'version-semver', [System.StringComparison]::Ordinal)) {
        throw "vcpkg version '$($vcpkgManifest.'version-semver')' does not match display version '$displayVersion'."
    }
}

function Invoke-ReleaseTier {
    Invoke-FullTier

    Invoke-VerificationStage -Name "Demo capture validation" -Action {
        $output = Join-Path $repoRoot "tmp\verify-demo-assets-$PID"
        & (Join-Path $repoRoot "tools\capture-demo-assets.ps1") -OutputDir $output
        $manifest = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "tools\demo-capture-manifest.json") | ConvertFrom-Json
        foreach ($capture in $manifest.captures) {
            $asset = Join-Path $output $capture.asset
            if (-not (Test-Path -LiteralPath $asset) -or (Get-Item -LiteralPath $asset).Length -eq 0) {
                throw "Demo capture is missing or empty: $asset"
            }
        }
    }

    Invoke-VerificationStage -Name "Version consistency" -Action {
        Invoke-VersionChecks
    }

    Invoke-VerificationStage -Name "Manifest checks" -Action {
        $manifest = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "tools\demo-capture-manifest.json") | ConvertFrom-Json
        if ($manifest.captures.Count -eq 0) {
            throw "The demo capture manifest has no captures."
        }
        $duplicates = @($manifest.captures.asset | Group-Object | Where-Object Count -gt 1)
        if ($duplicates.Count -gt 0) {
            throw "The demo capture manifest contains duplicate asset paths."
        }
    }

    Invoke-VerificationStage -Name "Markdown and PDF freshness" -Action {
        $markdown = Get-Item -LiteralPath (Join-Path $repoRoot "features_plan.md")
        $pdf = Get-Item -LiteralPath (Join-Path $repoRoot "docs\vidchopper_cli_architecture_plan.pdf")
        if ($pdf.LastWriteTimeUtc -lt $markdown.LastWriteTimeUtc) {
            throw "docs/vidchopper_cli_architecture_plan.pdf is older than features_plan.md; regenerate the PDF."
        }
    }

    Invoke-VerificationStage -Name "Package assembly and audit" -Action {
        Invoke-VersionChecks
        $cmakeText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "CMakeLists.txt")
        [void]($cmakeText -match 'VIDCHOPPER_DISPLAY_VERSION "([^"]+)"')
        $version = $Matches[1]
        & (Join-Path $repoRoot "tools\package-windows.ps1") -Version $version
        $zipPath = Join-Path $repoRoot "artifacts\release\VidChopper-$version-windows-x64.zip"
        if (-not (Test-Path -LiteralPath $zipPath)) {
            throw "Windows package assembly failed."
        }
        & (Join-Path $repoRoot "tools\verify-release-archive.ps1") `
            -Version $version `
            -ArchivePath $zipPath
        if ($LASTEXITCODE -ne 0) {
            throw "Windows package clean-archive verification failed."
        }
    }
}

if ($Fix -and $Tier -ne "Quick") {
    throw "-Fix is supported only with -Tier Quick."
}
if ($Fix -and $CiLane -notin @("None", "Lint")) {
    throw "-Fix is supported only for the local Quick tier or the Lint CI lane."
}

switch ($CiLane) {
    "Lint" {
        Invoke-FormattingChecks
        Invoke-StaticChecks
    }
    "Core" {
        Set-RepoVcpkgRoot
        Invoke-CoreBuild
        Invoke-FastTests
        Invoke-SlowTests
        Invoke-CliFixtures
    }
    "Gui" {
        Set-RepoVcpkgRoot
        Set-VerificationQtEnvironment
        Invoke-GuiBuild
        Invoke-GuiTests
        Invoke-GuiStartup
    }
    "Docs" {
        Invoke-DocsChecks
    }
    "None" {
        Set-RepoVcpkgRoot
        switch ($Tier) {
            "Quick" { Invoke-QuickTier }
            "Full" { Invoke-FullTier }
            "Release" { Invoke-ReleaseTier }
        }
    }
}

if ($CiLane -eq "None") {
    Write-Host "`nVerification tier '$Tier' passed."
} else {
    Write-Host "`nCI verification lane '$CiLane' passed."
}
