# Build Script for Constraint-Based Package Dependency Resolution (PowerShell)

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  Compiling Package Dependency Resolution Engine (C++)       " -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

$CXX = "g++"
$CXXFLAGS = "-std=c++14 -O2 -Wall -Wextra"

# 1. Compile Main CLI
Write-Host "`n[1/2] Compiling CLI executable (pkg-resolver.exe)..." -ForegroundColor Yellow
& $CXX $CXXFLAGS.Split(' ') -I include src/main.cpp -o pkg-resolver.exe

if ($LASTEXITCODE -ne 0) {
    Write-Host "`nError compiling pkg-resolver.exe" -ForegroundColor Red
    exit 1
}
Write-Host ">>> Successfully built pkg-resolver.exe!" -ForegroundColor Green

# 2. Compile Unit Test Suite
Write-Host "`n[2/2] Compiling Unit Test Suite (test_suite.exe)..." -ForegroundColor Yellow
& $CXX $CXXFLAGS.Split(' ') -I include tests/test_resolver.cpp -o test_suite.exe

if ($LASTEXITCODE -ne 0) {
    Write-Host "`nError compiling test_suite.exe" -ForegroundColor Red
    exit 1
}
Write-Host ">>> Successfully built test_suite.exe!" -ForegroundColor Green

Write-Host "`n============================================================" -ForegroundColor Cyan
Write-Host "  Running Test Suite...                                     " -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
.\test_suite.exe

if ($LASTEXITCODE -ne 0) {
    Write-Host "`nSome tests failed!" -ForegroundColor Red
    exit 1
}

Write-Host "`nBuild and tests complete! Try running: .\pkg-resolver.exe demo" -ForegroundColor Green
