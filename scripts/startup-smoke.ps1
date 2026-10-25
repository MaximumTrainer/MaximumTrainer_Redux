# Startup smoke test (#377) for Windows CI.
#
# Usage: scripts\startup-smoke.ps1 -App <path\to\MaximumTrainer.exe> -Mode first|second
#   first   delete HKCU\Software\MaximumTrainer and %APPDATA%\MaximumTrainer,
#           then expect the "default configuration created" line
#   second  keep the existing state and expect that line to be absent
#
# Runs the app with --startup-check (quits ~1 s after the login dialog shows)
# and checks the log: banner first, every startup step that began also ended,
# and the required R2.1 steps are present.
param(
    [Parameter(Mandatory = $true)][string]$App,
    [Parameter(Mandatory = $true)][ValidateSet('first', 'second')][string]$Mode
)
$ErrorActionPreference = 'Stop'

$logDir = Join-Path $env:APPDATA 'MaximumTrainer\MaximumTrainer_Redux'
$log = Join-Path $logDir 'MaximumTrainer.log'

function Fail([string]$message) {
    Write-Host "::error::startup smoke ($Mode): $message"
    exit 1
}

if ($Mode -eq 'first') {
    Remove-Item -Path 'HKCU:\Software\MaximumTrainer' -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -Path (Join-Path $env:APPDATA 'MaximumTrainer') -Recurse -Force -ErrorAction SilentlyContinue
}

$startLine = 0
if (Test-Path $log) { $startLine = @(Get-Content -LiteralPath $log -Encoding UTF8).Count }

$env:MT_NO_NETWORK = '1'
$proc = Start-Process -FilePath $App -ArgumentList '--startup-check' -PassThru
$null = $proc.Handle   # without this ExitCode is empty after WaitForExit
if (-not $proc.WaitForExit(60000)) {
    $proc.Kill()
    $timedOut = $true
}

if (-not (Test-Path $log)) { Fail "log file not created at $log" }
$runLog = @(Get-Content -LiteralPath $log -Encoding UTF8 | Select-Object -Skip $startLine)
Write-Host "=== log for this run ($log) ==="
$runLog | ForEach-Object { Write-Host $_ }
Write-Host '==============================='

if ($timedOut) { Fail 'app did not exit within 60 s' }
if ($proc.ExitCode -ne 0) { Fail "app exited with code $($proc.ExitCode)" }

if ($runLog.Count -eq 0 -or $runLog[0] -notmatch 'starting ====') { Fail "first line is not the startup banner: $($runLog[0])" }
foreach ($field in 'Qt runtime', 'OS: ', 'CPU architecture', 'Command line', 'Log file: ', 'Settings: ') {
    if (-not ($runLog | Where-Object { $_.Contains($field) })) { Fail "banner field missing: $field" }
}

$done = [string][char]0x2026 + ' done ('
$begin = [string][char]0x2026 + ' begin'
foreach ($step in 'configuration bootstrap', 'QWebEngineProfile init', 'metatype registration',
                  'GlobalVars: Account', 'CredentialStore load', 'GlobalVars: Settings',
                  'GlobalVars: SoundPlayer', 'network managers', 'GlobalVars: NetworkMonitor',
                  'stylesheet / theme', 'login dialog shown') {
    if (-not ($runLog | Where-Object { $_.Contains($step) -and $_.Contains($done) })) { Fail "no 'done' line for step: $step" }
}
foreach ($line in $runLog | Where-Object { $_.Contains('[Startup] ') -and $_.EndsWith($begin) }) {
    $label = $line.Substring($line.IndexOf('[Startup] ') + 10)
    $label = $label.Substring(0, $label.Length - $begin.Length).TrimEnd()
    if (-not ($runLog | Where-Object { $_.Contains("$label $([char]0x2026) done") })) { Fail "step began but never finished: $label" }
}

$created = $runLog | Where-Object { $_.Contains('default configuration created') }
if ($Mode -eq 'first' -and -not $created) { Fail 'first run did not create the default configuration' }
if ($Mode -eq 'second' -and $created) { Fail 'second run created the default configuration again' }
Write-Host "startup smoke ($Mode): OK"
