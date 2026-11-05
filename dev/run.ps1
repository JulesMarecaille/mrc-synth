<#
.SYNOPSIS
Build the browser emulator and serve it -- the one command for an iteration, from PowerShell.

.DESCRIPTION
The build itself is dev/run.sh, so there is one script to keep correct. This finds the POSIX
shell that Git for Windows installs and hands over to it, from the repository root whichever
directory you are standing in.

.EXAMPLE
.\dev\run.ps1               # build, then http://localhost:8790/
.EXAMPLE
.\dev\run.ps1 9000          # .. on another port
.EXAMPLE
.\dev\run.ps1 -NoBuild      # serve what is already in build/emu
#>
param(
    [int]$Port = 8790,
    [switch]$NoBuild
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot

# Git for Windows ships sh.exe; prefer the one beside the git we are actually using.
$sh = $null
$gitCmd = Get-Command git -ErrorAction SilentlyContinue
if ($gitCmd) {
    $gitRoot = Split-Path (Split-Path $gitCmd.Source)
    foreach ($c in @("$gitRoot\bin\sh.exe", "$gitRoot\usr\bin\sh.exe")) {
        if (Test-Path $c) { $sh = $c; break }
    }
}
if (-not $sh) {
    $onPath = Get-Command sh.exe -ErrorAction SilentlyContinue
    if ($onPath) { $sh = $onPath.Source }
}
if (-not $sh) {
    throw "No sh.exe found. Install Git for Windows, or run dev/run.sh from Git Bash."
}

$shArgs = @('dev/run.sh', "$Port")
if ($NoBuild) { $shArgs += '--no-build' }

Push-Location $repo
try { & $sh @shArgs } finally { Pop-Location }
