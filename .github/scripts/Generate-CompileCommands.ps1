[CmdletBinding()]
param()

$ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
$ws = $ProjectRoot.Path.Replace('\', '/')

$cppFiles = Get-ChildItem -Path "$ProjectRoot/src" -Recurse -Filter "*.cpp"
$compileCommands = foreach ($file in $cppFiles) {
    $relPath = $file.FullName.Substring($ProjectRoot.Path.Length + 1).Replace('\', '/')
    [PSCustomObject]@{
        directory = $ws
        file      = $relPath
        arguments = @(
            "clang++",
            "-std=c++20",
            "-I$ws/src",
            "-I$ws/src/core",
            "-I$ws/src/exporters",
            "-I$ws/src/obs",
            "-I$ws/src/ui",
            "-I$ws/build_x64",
            "-I$ws/build_x64/obs-timestamp-memo_autogen/include_RelWithDebInfo",
            "-I$ws/.deps/include",
            "-I$ws/.deps/obs-deps-qt6-2025-07-11-x64/include",
            "-I$ws/.deps/obs-deps-qt6-2025-07-11-x64/include/QtCore",
            "-I$ws/.deps/obs-deps-qt6-2025-07-11-x64/include/QtGui",
            "-I$ws/.deps/obs-deps-qt6-2025-07-11-x64/include/QtWidgets",
            "-c",
            $relPath
        )
    }
}

$outputPath = "$ProjectRoot/compile_commands.json"
$json = ConvertTo-Json -Depth 5 $compileCommands
[System.IO.File]::WriteAllText($outputPath, $json, [System.Text.Encoding]::UTF8)
Write-Host "Successfully generated compile_commands.json with $($compileCommands.Count) entries at $outputPath"
