[CmdletBinding()]
param(
    [Parameter()]
    [Alias("f")]
    [switch]$Fresh,

    [Parameter()]
    [Alias("p")]
    [switch]$Pro,

    [Parameter()]
    [Alias("d")]
    [switch]$bDebug,

    [Parameter()]
    [Alias("x")]
    [switch]$Win32,

    [Parameter()]
    [Alias("m")]
    [switch]$Msys2
)

$BUILD_TYPE = "MinSizeRel"
$SDLGPU_FLAG = "-DBUILD_SDLGPU=On"
$PRO_VERSION_FLAG = ""
$ARCH_FLAGS = "-A x64"
$CMAKE_GENERATOR = ""

if ($Fresh) {
    Remove-Item -Path .cache, CMakeCache.txt, CMakeFiles, cmake_install.cmake -Force -Recurse -ErrorAction SilentlyContinue
    Remove-Item -Path ".\build\" -Force -Recurse -ErrorAction Stop
    Remove-Item -Path ".\vendor\janet\src\conf\janetconf.h" -Force -ErrorAction SilentlyContinue
    git restore '.\build\*'
    git submodule deinit -f -- .\vendor\janet
    git submodule update --init --recursive
}

if ($Pro) {
    $PRO_VERSION_FLAG = "-DBUILD_PRO=On"
}

if ($bDebug) {
    $BUILD_TYPE = "Debug"
}

if ($Win32) {
    $ARCH_FLAGS = "-A Win32 -T v141_xp"
    Copy-Item -Path "build\janet\janetconf.h" -Destination "vendor\janet\src\conf\janetconf.h" -Force
    $SDLGPU_FLAG = ""
}

if ($Msys2) {
    $ARCH_FLAGS = ""
    $CMAKE_GENERATOR = '-G "MinGW Makefiles"'
} else {
    $CMAKE_GENERATOR = '-G "Visual Studio 16 2019"'
}

Write-Host
Write-Host "+-------------------------------+-------+"
Write-Host "|         Build Options         | Value |"
Write-Host "+-------------------------------+-------+"
Write-Host "| Fresh build (-f, -fresh)      | $(if ($Fresh) { "Yes" } else { "No " })   |"
Write-Host "| Pro version (-p, -pro)        | $(if ($Pro) { "Yes" } else { "No " })   |"
Write-Host "| Debug build (-d, -bdebug)     | $(if ($bDebug) { "Yes" } else { "No " })   |"
Write-Host "| Win32 (-x, -win32)            | $(if ($Win32) { "Yes" } else { "No " })   |"
Write-Host "| MSYS2 (-m, -msys2)            | $(if ($Msys2) { "Yes" } else { "No " })   |"
Write-Host "+-------------------------------+-------+"
Write-Host

Set-Location -Path ./build -ErrorAction Stop

$cmakeCommand = "cmake $CMAKE_GENERATOR " +
                "$ARCH_FLAGS " +
                "-DCMAKE_BUILD_TYPE=$BUILD_TYPE " +
                "$PRO_VERSION_FLAG " +
                "$SDLGPU_FLAG " +
                "-DBUILD_WITH_ALL=On " +
                "-DCMAKE_EXPORT_COMPILE_COMMANDS=On " +
                ".."

Write-Host "Executing CMake Command: $cmakeCommand"
Invoke-Expression $cmakeCommand

$numCPUs = [Environment]::ProcessorCount

if ($LASTEXITCODE -eq 0) {
    if ($Msys2) {
        mingw32-make "-j$numCPUs"
    } else {
        cmake --build . --config "$BUILD_TYPE" --parallel
    }
} else {
    Write-Host "(C)Make failed. Exiting."
    exit 1
}

Set-Location -Path .. -ErrorAction Stop
