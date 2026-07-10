param(
    [string]$ExecutablePath = "",
    [string]$ManifestPath = "",
    [string]$OutputDir = "",
    [switch]$KeepArtifacts
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ExecutablePath)) {
    $ExecutablePath = Join-Path $repoRoot "build\windows-gui-release\Release\VidChopper.exe"
}
if ([string]::IsNullOrWhiteSpace($ManifestPath)) {
    $ManifestPath = Join-Path $repoRoot "tools\demo-capture-manifest.json"
}
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repoRoot "docs\src\assets"
}

if (!(Test-Path -LiteralPath $ExecutablePath)) {
    throw "VidChopper executable not found at $ExecutablePath"
}
if (!(Test-Path -LiteralPath $ManifestPath)) {
    throw "Capture manifest not found at $ManifestPath"
}

$ffmpegCommand = Get-Command ffmpeg -ErrorAction Stop
[void](Get-Command ffprobe -ErrorAction Stop)

$manifest = Get-Content -Raw -LiteralPath $ManifestPath | ConvertFrom-Json
$workspaceRoot = Join-Path $repoRoot "tmp\demo-capture"
$readyFile = Join-Path $workspaceRoot "demo-ready.txt"
$sampleVideo = Join-Path $workspaceRoot $manifest.sampleVideo.filename

New-Item -ItemType Directory -Force -Path $workspaceRoot | Out-Null
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class Win32Capture {
    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdcBlt, int nFlags);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }
}
"@

function Invoke-External {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$ArgumentList
    )

    $process = Start-Process -FilePath $FilePath -ArgumentList $ArgumentList -NoNewWindow -PassThru -Wait
    if ($process.ExitCode -ne 0) {
        throw "Command failed: $FilePath $($ArgumentList -join ' ')"
    }
}

function New-SampleVideo {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (Test-Path -LiteralPath $Path) {
        return
    }

    Invoke-External -FilePath $ffmpegCommand.Source -ArgumentList @(
        "-y",
        "-f", "lavfi",
        "-i", "testsrc=size=$($manifest.sampleVideo.size):rate=$($manifest.sampleVideo.frameRate)",
        "-f", "lavfi",
        "-i", "sine=frequency=$($manifest.sampleVideo.audioFrequency):sample_rate=48000",
        "-t", "$($manifest.sampleVideo.durationSeconds)",
        "-c:v", "libx264",
        "-pix_fmt", "yuv420p",
        "-c:a", "aac",
        $Path
    )
}

function Wait-ForMainWindow {
    param(
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process,
        [int]$TimeoutSeconds = 20
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $Process.Refresh()
        if ($Process.HasExited) {
            throw "VidChopper exited before the window was ready."
        }
        if ($Process.MainWindowHandle -ne 0) {
            return $Process.MainWindowHandle
        }
        Start-Sleep -Milliseconds 200
    }

    throw "Timed out waiting for the VidChopper window."
}

function Wait-ForReadyMarker {
    param(
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process,
        [Parameter(Mandatory = $true)][string]$Path,
        [int]$TimeoutSeconds = 20
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        if (Test-Path -LiteralPath $Path) {
            $status = (Get-Content -Raw -LiteralPath $Path).Trim()
            if ($status -eq "ready") {
                return
            }
            throw "Demo scene failed with marker status '$status'."
        }

        $Process.Refresh()
        if ($Process.HasExited) {
            throw "VidChopper exited before writing the ready marker."
        }
        Start-Sleep -Milliseconds 200
    }

    throw "Timed out waiting for demo ready marker."
}

function Get-WindowBitmap {
    param([Parameter(Mandatory = $true)][IntPtr]$Handle)

    $rect = New-Object Win32Capture+RECT
    if (-not [Win32Capture]::GetWindowRect($Handle, [ref]$rect)) {
        throw "Unable to read window bounds."
    }

    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    if ($width -le 0 -or $height -le 0) {
        throw "Invalid window bounds: ${width}x${height}"
    }

    $bitmap = New-Object System.Drawing.Bitmap $width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $hdc = $graphics.GetHdc()
        try {
            if (-not [Win32Capture]::PrintWindow($Handle, $hdc, 0)) {
                throw "PrintWindow failed."
            }
        }
        finally {
            $graphics.ReleaseHdc($hdc)
        }
    }
    finally {
        $graphics.Dispose()
    }

    return $bitmap
}

function Save-CaptureBitmap {
    param(
        [Parameter(Mandatory = $true)][System.Drawing.Bitmap]$Bitmap,
        [Parameter(Mandatory = $true)]$Capture,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    $finalBitmap = $Bitmap
    if ($Capture.capture -eq "crop") {
        $crop = $Capture.crop
        $rectangle = New-Object System.Drawing.Rectangle $crop.x, $crop.y, $crop.width, $crop.height
        $finalBitmap = $Bitmap.Clone($rectangle, $Bitmap.PixelFormat)
    }

    try {
        $finalBitmap.Save($Destination, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        if ($finalBitmap -ne $Bitmap) {
            $finalBitmap.Dispose()
        }
    }
}

function Invoke-DemoCapture {
    param([Parameter(Mandatory = $true)]$Capture)

    if (Test-Path -LiteralPath $readyFile) {
        Remove-Item -LiteralPath $readyFile -Force
    }

    $arguments = @(
        "--demo-scene=$($Capture.scene)",
        "--demo-source=$sampleVideo",
        "--window-size=$($Capture.windowSize)",
        "--demo-ready-file=$readyFile"
    )

    $process = Start-Process -FilePath $ExecutablePath -ArgumentList $arguments -PassThru -WindowStyle Hidden
    try {
        $windowHandle = Wait-ForMainWindow -Process $process
        Wait-ForReadyMarker -Process $process -Path $readyFile
        Start-Sleep -Milliseconds 250

        $bitmap = Get-WindowBitmap -Handle $windowHandle
        try {
            $destination = Join-Path $OutputDir $Capture.asset
            Save-CaptureBitmap -Bitmap $bitmap -Capture $Capture -Destination $destination
        }
        finally {
            $bitmap.Dispose()
        }
    }
    finally {
        if (-not $process.HasExited) {
            $process.CloseMainWindow() | Out-Null
            if (-not $process.WaitForExit(1000)) {
                Stop-Process -Id $process.Id -Force
            }
        }
    }
}

New-SampleVideo -Path $sampleVideo

foreach ($capture in $manifest.captures) {
    Invoke-DemoCapture -Capture $capture
}

if (-not $KeepArtifacts) {
    Remove-Item -LiteralPath $workspaceRoot -Recurse -Force
}

Write-Host "Captured demo assets into $OutputDir"
