# Sets up a Windows 11 ARM64 VM for check.py, as SYSTEM (check.py windows
# --setup runs it): Git (its bash runs Bazel's shell steps), Python (runs
# windows_run.py), Bazelisk, Visual Studio 2022 Build Tools with MSVC's
# ARM64 libraries, the Windows SDK and its Clang (clang-cl), Windows MIDI
# Services' tools (MidiInputPadd_test's loopback keyboard), and Developer
# Mode (for symbolic links). Running it again skips what's there. The
# Build Tools take a while (several GB).
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$downloads = 'C:\sakbox-setup'
New-Item -ItemType Directory -Force $downloads, 'C:\tools' | Out-Null

function Install($name, $test, $url, $arguments) {
  if (Test-Path $test) { "$name is there."; return }
  "Installing $name..."
  $file = Join-Path $downloads ($url -split '/')[-1]
  Invoke-WebRequest $url -OutFile $file
  $process = Start-Process $file -ArgumentList $arguments -Wait -PassThru
  if ($process.ExitCode -ne 0 -and $process.ExitCode -ne 3010) { throw "$name's installer failed: $($process.ExitCode)" }
}

Install 'Git' 'C:\Program Files\Git\bin\bash.exe' `
  'https://github.com/git-for-windows/git/releases/download/v2.56.0.windows.4/Git-2.56.0.4-arm64.exe' `
  @('/VERYSILENT', '/NORESTART', '/NOCANCEL', '/SP-')
Install 'Python' 'C:\Program Files\Python314-arm64\python.exe' `
  'https://www.python.org/ftp/python/3.14.10/python-3.14.10-arm64.exe' `
  @('/quiet', 'InstallAllUsers=1', 'PrependPath=1', 'Include_test=0')
Install 'Visual Studio Build Tools' `
  'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\Llvm\ARM64\bin\clang-cl.exe' `
  'https://aka.ms/vs/17/release/vs_buildtools.exe' `
  @('--quiet', '--wait', '--norestart', '--nocache',
    '--add', 'Microsoft.VisualStudio.Workload.VCTools', '--includeRecommended',
    '--add', 'Microsoft.VisualStudio.Component.VC.Tools.ARM64',
    '--add', 'Microsoft.VisualStudio.Component.VC.Llvm.Clang',
    '--add', 'Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset')
# Windows MIDI Services' tools: its console (midi.exe) makes the loopback
# ports MidiInputPadd_test plays on. The service itself is part of Windows
# 11 24H2 and later; on older Windows the test skips.
Install 'Windows MIDI Services tools' 'C:\Program Files\Windows MIDI Services\Tools\midi.exe' `
  'https://github.com/microsoft/MIDI/releases/download/inbox-preview-13/Windows.MIDI.Services.Tools.0.99.96-preview.13-arm64.exe' `
  @('/quiet', '/norestart')

# Developer Mode, which lets an ordinary account make symbolic links: Bazel's
# runfiles (the files a test reads) and rules_python's tools are made of them
$unlock = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock'
if (-not (Test-Path $unlock)) { New-Item $unlock | Out-Null }
# (both values, as Settings' Developer Mode switch sets them)
Set-ItemProperty $unlock -Name AllowDevelopmentWithoutDevLicense -Value 1 -Type DWord
Set-ItemProperty $unlock -Name AllowAllTrustedApps -Value 1 -Type DWord
'Developer Mode is on.'

# Bazelisk, which runs the Bazel version in .bazelversion
if (-not (Test-Path 'C:\tools\bazelisk.exe')) {
  'Installing Bazelisk...'
  Invoke-WebRequest 'https://github.com/bazelbuild/bazelisk/releases/download/v1.25.0/bazelisk-windows-arm64.exe' `
    -OutFile 'C:\tools\bazelisk.exe'
}
'Set up.'
