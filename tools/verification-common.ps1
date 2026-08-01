Set-StrictMode -Version Latest

$script:RepoRoot = Split-Path -Parent $PSScriptRoot
$script:ToolVersions = @{
    Clang = "18.1.8"
    CMake = [version]"3.28"
    Qt = [version]"6.9"
    Node = [version]"22.0"
    Ffmpeg = "7.1.1"
}

function Get-RepoRoot {
    return $script:RepoRoot
}

function Get-RepoQtRoot {
    $candidates = [System.Collections.Generic.List[string]]::new()
    foreach ($candidate in @($env:Qt6_ROOT, $env:Qt6_DIR, $env:QTDIR)) {
        if (-not [string]::IsNullOrWhiteSpace($candidate)) {
            $candidates.Add($candidate)
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($env:CMAKE_PREFIX_PATH)) {
        foreach ($candidate in $env:CMAKE_PREFIX_PATH.Split([IO.Path]::PathSeparator)) {
            if (-not [string]::IsNullOrWhiteSpace($candidate)) {
                $candidates.Add($candidate)
            }
        }
    }

    $standardRoots = @(
        "C:\Qt",
        (Join-Path $env:ProgramFiles "Qt"),
        (Join-Path $env:USERPROFILE "Qt")
    )
    foreach ($standardRoot in $standardRoots) {
        if (-not (Test-Path -LiteralPath $standardRoot -PathType Container)) {
            continue
        }

        $versionDirectories = @(Get-ChildItem -LiteralPath $standardRoot -Directory -ErrorAction SilentlyContinue |
            Where-Object Name -Like "6.9*" |
            Sort-Object { try { [version]$_.Name } catch { [version]"0.0" } } -Descending)
        foreach ($versionDirectory in $versionDirectories) {
            $candidates.Add((Join-Path $versionDirectory.FullName "msvc2022_64"))
        }
    }

    foreach ($candidate in $candidates) {
        if (-not (Test-Path -LiteralPath $candidate)) {
            continue
        }

        $resolved = (Resolve-Path -LiteralPath $candidate).Path
        if ((Split-Path -Leaf $resolved) -eq "Qt6" -and
            (Test-Path -LiteralPath (Join-Path $resolved "Qt6Config.cmake"))) {
            $resolved = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $resolved))
        }

        $config = Join-Path $resolved "lib\cmake\Qt6\Qt6Config.cmake"
        $qmake = Join-Path $resolved "bin\qmake.exe"
        if (-not (Test-Path -LiteralPath $config) -or -not (Test-Path -LiteralPath $qmake)) {
            continue
        }

        $versionText = (& $qmake -query QT_VERSION).Trim()
        if ($LASTEXITCODE -eq 0 -and $versionText -match '^6\.9(?:\.|$)') {
            return $resolved
        }
    }

    return $null
}

function Set-RepoQtEnvironment {
    param([string]$QtRoot = "")

    if ([string]::IsNullOrWhiteSpace($QtRoot)) {
        $QtRoot = Get-RepoQtRoot
    }
    if ([string]::IsNullOrWhiteSpace($QtRoot)) {
        throw "Qt 6.9 for MSVC 2022 was not found. Install it under C:\Qt or set Qt6_ROOT, Qt6_DIR, or QTDIR."
    }

    $qtBin = Join-Path $QtRoot "bin"
    $env:Qt6_ROOT = $QtRoot
    $env:Qt6_DIR = Join-Path $QtRoot "lib\cmake\Qt6"
    $env:QTDIR = $QtRoot

    $prefixes = @($env:CMAKE_PREFIX_PATH -split [regex]::Escape([string][IO.Path]::PathSeparator) |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    if ($QtRoot -notin $prefixes) {
        $env:CMAKE_PREFIX_PATH = (@($QtRoot) + $prefixes) -join [IO.Path]::PathSeparator
    }

    $pathEntries = @($env:PATH -split [regex]::Escape([string][IO.Path]::PathSeparator))
    if ($qtBin -notin $pathEntries) {
        $env:PATH = (@($qtBin) + $pathEntries) -join [IO.Path]::PathSeparator
    }

    return $QtRoot
}

function Get-RepoCommand {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Remediation
    )

    if ($Name -in @("clang-format", "clang-tidy")) {
        $localPath = Join-Path $script:RepoRoot ".venv-tools\Scripts\$Name.exe"
        if (Test-Path -LiteralPath $localPath) {
            return $localPath
        }
    }

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        throw "$Name was not found. $Remediation"
    }

    return $command.Source
}

function Invoke-RepoCommand {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [string[]]$ArgumentList = @(),
        [string]$WorkingDirectory = $script:RepoRoot
    )

    Push-Location -LiteralPath $WorkingDirectory
    try {
        & $FilePath @ArgumentList
        if ($LASTEXITCODE -ne 0) {
            throw "Command exited with code $LASTEXITCODE`: $FilePath $($ArgumentList -join ' ')"
        }
    }
    finally {
        Pop-Location
    }
}

function Invoke-VerificationStage {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][scriptblock]$Action
    )

    Write-Host "`n==> $Name"
    try {
        & $Action
        Write-Host "PASS: $Name"
    }
    catch {
        Write-Error "FAILED STAGE: $Name`n$($_.Exception.Message)"
        exit 1
    }
}

function Set-RepoVcpkgRoot {
    if (-not [string]::IsNullOrWhiteSpace($env:VCPKG_ROOT)) {
        return
    }

    $repoVcpkg = Join-Path $script:RepoRoot ".vcpkg"
    if (Test-Path -LiteralPath (Join-Path $repoVcpkg "scripts\buildsystems\vcpkg.cmake")) {
        $env:VCPKG_ROOT = $repoVcpkg
        return
    }

    throw "VCPKG_ROOT is not set and .vcpkg is not bootstrapped. Run tools/bootstrap.ps1."
}
