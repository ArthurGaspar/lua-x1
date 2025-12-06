# setup-dotnet.ps1
# Configure local .NET SDK usage
# RUN IT IN POWERSHELL, NOT IN BASH
# YOU ALSO NEED EXECUTION PERMISSION
# YOU CAN USE: Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

$DOTNET_ROOT = (Resolve-Path "$PSScriptRoot/../../../vendor/dotnet").Path
$env:DOTNET_ROOT = $DOTNET_ROOT
$env:PATH = "$DOTNET_ROOT;$env:PATH"

Write-Host "DOTNET_ROOT set to $DOTNET_ROOT"
Write-Host "You can now run 'dotnet --version' to confirm."