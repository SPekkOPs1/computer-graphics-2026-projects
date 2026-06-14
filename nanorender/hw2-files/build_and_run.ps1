param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo")]
    [string]$Configuration = "Release"
)

& "$PSScriptRoot\..\build_and_run.ps1" -Homework hw2 -Configuration $Configuration
