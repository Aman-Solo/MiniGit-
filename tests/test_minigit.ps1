$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$temp = Join-Path $env:TEMP 'minigit-automated-test'
$executable = Join-Path $temp 'minigit-test.exe'

function Assert-Contains {
    param([string]$Text, [string]$Expected)
    if ($Text -notlike "*$Expected*") {
        throw "Expected output to contain: $Expected"
    }
}

function Invoke-MiniGit {
    param([string[]]$Commands)
    Push-Location $temp
    try {
        return (($Commands -join "`n") | & $executable 2>&1 | Out-String)
    } finally {
        Pop-Location
    }
}

try {
    Remove-Item $temp -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Path $temp | Out-Null

    $compileOutput = & g++ -std=c++17 -Wall -Wextra -pedantic (Join-Path $root 'main.cpp') -o $executable 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "Compilation failed:`n$compileOutput"
    }

    Set-Content (Join-Path $temp 'test.txt') 'base content'
    $output = Invoke-MiniGit @('init', 'add test.txt', 'commit -m base', 'exit')
    Assert-Contains $output 'Created commit 0: base'

    $output = Invoke-MiniGit @('log', 'exit')
    Assert-Contains $output 'commit 0'
    Assert-Contains $output 'base'

    Set-Content (Join-Path $temp 'test.txt') 'main content'
    $output = Invoke-MiniGit @('branch feature', 'status', 'exit')
    Assert-Contains $output "Created branch 'feature'."
    $output = Invoke-MiniGit @('status', 'add test.txt', 'commit -m main update', 'diff 0 1', 'exit')
    Assert-Contains $output 'unstaged modification: test.txt'
    Assert-Contains $output 'Created commit 1: main update'
    Assert-Contains $output 'Changes in test.txt:'

    $output = Invoke-MiniGit @('checkout feature', 'exit')
    Assert-Contains $output "Switched to branch 'feature'."
    Set-Content (Join-Path $temp 'test.txt') 'feature content'
    $output = Invoke-MiniGit @('add test.txt', 'commit -m feature update', 'merge main', 'exit')
    Assert-Contains $output 'Created commit 2: feature update'
    Assert-Contains $output 'CONFLICT: test.txt'
    Assert-Contains (Get-Content (Join-Path $temp 'test.txt') -Raw) '<<<<<<< HEAD'

    Set-Content (Join-Path $temp 'test.txt') 'resolved content'
    $output = Invoke-MiniGit @('add test.txt', 'commit -m resolve conflict', 'log', 'exit')
    Assert-Contains $output 'Created commit 3: resolve conflict'

    $output = Invoke-MiniGit @('rm test.txt', 'status', 'commit -m delete file', 'exit')
    Assert-Contains $output 'deleted:  test.txt'
    Assert-Contains $output 'Created commit 4: delete file'
    if (Test-Path (Join-Path $temp 'test.txt')) {
        throw 'Expected test.txt to remain deleted after the deletion commit.'
    }

    Write-Host 'All MiniGit tests passed.'
} finally {
    Remove-Item $temp -Recurse -Force -ErrorAction SilentlyContinue
}
