# 如需自訂boost/vcpkgmake 路徑參數為
#.\publish.ps1 -BoostPath 'D:/libs/boost_1_89_0'
#.\publish.ps1 -Vcpkgmake 'D:/vcpkg/scripts/buildsystems/vcpkg.cmake'
# ========================
# publish.ps1
# 功能：一鍵產生 Visual Studio 專案、編譯程式、輸出乾淨的 exe
# 並檢查 Boost 是否存在
# ========================

param(
    # 指定編譯模式，預設是 Release（也可以傳 Debug）
    [string]$Config = "Release",

    # Boost 路徑，可手動指定，或用預設路徑
    [string]$BoostPath = "C:/Program Files/boost/boost_1_89_0",

    #Vcpkgcmake檔案路徑，可手動指定，或用預設路徑
    [string]$Vcpkgmake = "D:/vcpkg/scripts/buildsystems/vcpkg.cmake"
)

# === 路徑設定 ===
$rootDir  = Split-Path -Parent $MyInvocation.MyCommand.Definition
$srcDir   = Join-Path $rootDir "High-Concurrency"
$buildDir = Join-Path $srcDir "build"
$distDir  = Join-Path $rootDir "dist"

# === Step 0: 檢查 DCMAKE_TOOLCHAIN_FILE 路徑 ===
if (-not (Test-Path $Vcpkgmake)) {
    Write-Error "找不到 Vcpkg：$Vcpkgmake"
    Write-Host "請確認 Vcpkg 是否安裝，或用參數指定路徑，例如："
    Write-Host ".\publish.ps1 -Vcpkgmake 'D:/vcpkg/scripts/buildsystems/vcpkg.cmake'"
    exit 1
}
Write-Host "已找到 Vcpkg：$Vcpkgmake"

# === Step 0: 檢查 Boost 路徑 ===
if (-not (Test-Path $BoostPath)) {
    Write-Error "找不到 Boost：$BoostPath"
    Write-Host "請確認 Boost 是否安裝，或用參數指定路徑，例如："
    Write-Host ".\publish.ps1 -BoostPath 'D:/libs/boost_1_89_0'"
    exit 1
}
Write-Host "已找到 Boost：$BoostPath"

# === Step 1: 準備 build 目錄 ===
if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
}

# === Step 2: 用 CMake 產生 Visual Studio 專案檔 (.sln) ===
Write-Host "產生 Visual Studio 專案檔 (.sln)..."
Push-Location $buildDir
cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_TOOLCHAIN_FILE="$Vcpkgmake" -DBOOST_ROOT="$BoostPath" .. | Out-Null
# ↑ 新增 -DCMAKE_TOOLCHAIN_FILE 參數，讓 CMake 知道 Vcpkg 的位置
# ↑ 新增 -DBOOST_ROOT 參數，讓 CMake 知道 Boost 的位置

Pop-Location

# === Step 3: 編譯專案 ===
Write-Host "開始編譯 ($Config)..."
cmake --build $buildDir --config $Config

# === Step 4: 準備 dist 目錄 ===
if (Test-Path $distDir) {
    Remove-Item -Recurse -Force $distDir
}
New-Item -ItemType Directory -Force -Path $distDir | Out-Null

# === Step 5: 複製編譯好的 exe 到 dist ===
$exeFiles = Get-ChildItem -Path "$buildDir" -Recurse -Filter "*.exe"
foreach ($exe in $exeFiles) {
    Copy-Item $exe.FullName $distDir
    Write-Host "開始複製 ($exe)..."
}
#Step 6: 複製產log檔的ps1到dist
Copy-Item 'record.ps1' $distDir

Write-Host "發佈完成！檔案已輸出到 $distDir"