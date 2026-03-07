# LargePageDrivers Setup for Codecave (Signed PING Execution)
# Adds beep.sys to LargePageDrivers so PING runs from signed Beep .data (low detection).
# Requires: Run as Administrator. Reboot required after setup.
#
# Registry: HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Memory Management
# Value: LargePageDrivers (REG_MULTI_SZ)
# Content: beep.sys

$ErrorActionPreference = "Stop"
$RegPath = "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Memory Management"
$ValueName = "LargePageDrivers"
$DriverName = "beep.sys"

# Check admin
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "ERROR: Run as Administrator." -ForegroundColor Red
    Write-Host "Right-click PowerShell -> Run as Administrator" -ForegroundColor Yellow
    exit 1
}

Write-Host "LargePageDrivers Setup for Codecave" -ForegroundColor Cyan
Write-Host "==================================" -ForegroundColor Cyan

# Check if value exists and already contains beep.sys
$existing = $null
try {
    $existing = Get-ItemProperty -Path $RegPath -Name $ValueName -ErrorAction SilentlyContinue | Select-Object -ExpandProperty $ValueName
} catch {}

if ($existing) {
    $list = @($existing)
    if ($list -contains $DriverName) {
        Write-Host "[OK] $DriverName is already in LargePageDrivers." -ForegroundColor Green
        Write-Host "Reboot if you haven't since last change." -ForegroundColor Yellow
        exit 0
    }
    $newList = $list + $DriverName
} else {
    $newList = @($DriverName)
}

# Set REG_MULTI_SZ - each string as separate array element
Set-ItemProperty -Path $RegPath -Name $ValueName -Value $newList -Type MultiString -Force
Write-Host "[OK] Added $DriverName to LargePageDrivers." -ForegroundColor Green
Write-Host ""
Write-Host "REBOOT REQUIRED for changes to take effect." -ForegroundColor Yellow
Write-Host "After reboot, FLUSHCOMM_USE_CODECAVE will run PING from signed Beep memory." -ForegroundColor Gray
Write-Host ""
$r = Read-Host "Reboot now? (y/n)"
if ($r -eq 'y' -or $r -eq 'Y') {
    Write-Host "Rebooting in 10 seconds... (Ctrl+C to cancel)" -ForegroundColor Yellow
    Start-Sleep -Seconds 10
    Restart-Computer -Force
}
