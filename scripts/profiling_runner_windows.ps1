# profiling_runner_windows.ps1
# Automated profiling runner for Windows

$BenchmarkBin = ".\tests\RelWithDebInfo\test_demuxer_basic.exe"
$OutputDir = ".\profiling_results"
$Timestamp = Get-Date -Format "yyyyMMdd_HHmmss"

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

Write-Host "=== Starting profiling session: $Timestamp ==="

# 1. Visual Studio Performance Profiler (if available)
Write-Host "[1/5] Checking for VSPerfCmd..."
if (Get-Command VSPerfCmd -ErrorAction SilentlyContinue) {
    Write-Host "Running VSPerfCmd profiling..."
    VSPerfCmd /start:trace /output:"$OutputDir\profile_$Timestamp.vsp"
    & $BenchmarkBin
    VSPerfCmd /shutdown
    VSPerfReport "$OutputDir\profile_$Timestamp.vsp" /summary:all /output:"$OutputDir\vsperfcmd_report_$Timestamp.csv"
} else {
    Write-Host "VSPerfCmd not found, skipping"
}

# 2. Windows Performance Toolkit (if available)
Write-Host "[2/5] Checking for wpr..."
if (Get-Command wpr -ErrorAction SilentlyContinue) {
    Write-Host "Running Windows Performance Recorder..."
    wpr -start CPU
    & $BenchmarkBin
    wpr -stop "$OutputDir\trace_$Timestamp.etl"

    # Convert to text if xperf available
    if (Get-Command xperf -ErrorAction SilentlyContinue) {
        xperf -i "$OutputDir\trace_$Timestamp.etl" -o "$OutputDir\trace_$Timestamp.txt"
    }
} else {
    Write-Host "wpr not found, skipping"
}

# 3. Dr. Memory (if available)
Write-Host "[3/5] Checking for Dr. Memory..."
if (Test-Path "C:\Program Files\Dr. Memory\bin\drmemory.exe") {
    Write-Host "Running Dr. Memory..."
    & "C:\Program Files\Dr. Memory\bin\drmemory.exe" `
        -batch `
        -logdir "$OutputDir\drmemory_$Timestamp" `
        -- $BenchmarkBin

    # Copy results
    Copy-Item "$OutputDir\drmemory_$Timestamp\results.txt" "$OutputDir\drmemory_report_$Timestamp.txt"
} else {
    Write-Host "Dr. Memory not found, skipping"
}

# 4. Simple timing with Measure-Command
Write-Host "[4/5] Running timing measurement..."
$TimingResult = Measure-Command { & $BenchmarkBin }
$TimingResult | Out-File "$OutputDir\timing_$Timestamp.txt"

# 5. Create summary JSON
Write-Host "[5/5] Creating summary..."
$Summary = @{
    timestamp = $Timestamp
    platform = "windows"
    hostname = $env:COMPUTERNAME
    cpu = (Get-WmiObject Win32_Processor).Name
    files = @()
}

# List generated files
Get-ChildItem "$OutputDir\*_$Timestamp.*" | ForEach-Object {
    $Summary.files += $_.Name
}

$Summary | ConvertTo-Json | Out-File "$OutputDir\summary_$Timestamp.json"

Write-Host "=== Profiling complete! ==="
Write-Host "Results saved to: $OutputDir"
Write-Host ""
Write-Host "Files to copy:"
Get-ChildItem "$OutputDir\*_$Timestamp.*" | ForEach-Object {
    Write-Host "  - $($_.Name) ($([math]::Round($_.Length/1KB, 2)) KB)"
}
