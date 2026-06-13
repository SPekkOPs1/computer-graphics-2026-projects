param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo")]
    [string]$Configuration = "Release",
    [ValidateSet("hw1", "hw2")]
    [string]$Homework = "hw2"
)

$vsInstallPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $vsInstallPath "VC\Auxiliary\Build\vcvarsall.bat"
$buildDir = "$PSScriptRoot\build\$Configuration"

# 1. Configure
cmd /c "`"$vcvars`" x64 && cmake -G Ninja -DCMAKE_BUILD_TYPE=$Configuration -B `"$buildDir`" -S `"$PSScriptRoot`""
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

# 2. Build only the selected homework target
cmd /c "`"$vcvars`" x64 && ninja -C `"$buildDir`" $Homework"
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

# 3. Run
& "$buildDir\$Homework.exe"
