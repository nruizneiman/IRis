# Builds and runs the host (PC) unit tests. No ESP32 needed.
$ErrorActionPreference = 'Stop'
$env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
Set-Location (Split-Path $PSScriptRoot -Parent)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
& ./build/iris_unit_tests.exe @args
$code = $LASTEXITCODE
if (Test-Path ./build/iris_ir_tests.exe) {
  & ./build/iris_ir_tests.exe
  if ($LASTEXITCODE -ne 0) { $code = $LASTEXITCODE }
} else {
  Write-Warning "iris_ir_tests not built: run pio once so the IRremoteESP8266 sources exist"
}
exit $code
