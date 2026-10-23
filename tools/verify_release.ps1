param(
    [Parameter(Mandatory = $true)][string[]]$GameImages,
    [string[]]$RejectedImages = @(),
    [string]$RetainedDll,
    [switch]$Package
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$testedDllHash = '92F2C95A28BB317FA6B2F6E2B02119CF300760E20769C1CCBCF4934F250324A8'
if ($Package -and -not $RetainedDll) { throw 'Stable packaging requires -RetainedDll; a rebuild is not the accepted release binary' }
if ($RetainedDll) { $RetainedDll = [System.IO.Path]::GetFullPath($RetainedDll) }
function Assert-RetainedDll {
    if (-not $RetainedDll) { return }
    $retained = Get-Item -LiteralPath $RetainedDll
    if ($retained.PSIsContainer -or ($retained.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) { throw 'Refusing a linked/non-file retained DLL' }
    if ((Get-FileHash -LiteralPath $retained.FullName -Algorithm SHA256).Hash -ne $testedDllHash) { throw 'Retained DLL does not match the gameplay-tested artifact' }
}
Assert-RetainedDll
$acceptedHashes = @(
    '008408E6F544D7987F9CDE4E34C8E853690A65AFD97F8498DA2CD6ED24C4793B',
    '4143182F113D774062809039154248E7AF7578A81C92E54323F7D53361F8EFA1',
    '4A92BFA3CE2428B40EE517B7C1125D9F6F79274382DE03846351992663B2E2E4',
    '0B66101DC873434E853A898329CB85CEE0DE52D0AE91AC796EFA0FE05B61522C',
    '258EC563FD37B3EFCF67C2C5207AA0DF6AB8E20DC09ACF2394380D697CC24DAC',
    '1ECE1D37E7762F6B32C20DB355A7F0B199A85FC6A60C59CF693922C999A16AE7'
)
$rejectedHashes = @()
$images = @(foreach ($path in $GameImages) {
    $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    if ($hash -notin $acceptedHashes) { throw "Unexpected accepted image: $path" }
    @{Path=$path;Hash=$hash;Accepted=$true}
})
if (@($images.Hash | Sort-Object -Unique).Count -ne $acceptedHashes.Count) { throw 'All six exact accepted images are required' }
$images += @(foreach ($path in $RejectedImages) {
    $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    if ($hash -notin $rejectedHashes) { throw "Unexpected rejection fixture: $path" }
    @{Path=$path;Hash=$hash;Accepted=$false}
})
function Assert-ProtectedImages {
    foreach ($image in $images) {
        $actual = (Get-FileHash -LiteralPath $image.Path -Algorithm SHA256).Hash
        if ($actual -ne $image.Hash) { throw "Protected image hash mismatch: $($image.Path)" }
    }
    Write-Host "Protected game image hashes match: $($images.Count)"
}
function Invoke-Checked([string]$Command, [string[]]$Arguments) {
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Command failed ($LASTEXITCODE): $($Arguments -join ' ')" }
}
Assert-ProtectedImages
Push-Location -LiteralPath $projectRoot
try {
    Invoke-Checked 'py' @('-3.11', '-m', 'unittest', 'discover', '-s', 'tests', '-p', 'test_artifacts.py')
    $tests = @('Core', 'Native', 'Container', 'Profile', 'Bridge', 'World', 'Render', 'Hook', 'Diagnostic', 'Remote', 'RemoteNative', 'HeldUse', 'EntityUse', 'Compatibility')
    foreach ($mode in @('debug', 'release')) {
        Write-Host "Fresh $mode build and offline tests"
        Invoke-Checked 'xmake' @('f', '-y', '-m', $mode)
        Invoke-Checked 'xmake' @('build', '-r', '-y')
        foreach ($test in $tests) {
            $target = "ContainerPreview${test}Tests"
            if ($test -eq 'Profile') {
                foreach ($image in $images) {
                    $testArgs = @('run', $target, $image.Path)
                    if (-not $image.Accepted) { $testArgs += 'reject' }
                    Invoke-Checked 'xmake' $testArgs
                }
            } else { Invoke-Checked 'xmake' @('run', $target) }
        }
        foreach ($test in @('Container', 'Diagnostic', 'RemoteNative', 'HeldUse', 'EntityUse')) {
            Invoke-Checked 'xmake' @('run', "ContainerPreview${test}Tests", '51')
        }
        Invoke-Checked 'py' @('-3.11', 'tools/verify_artifacts.py', '--dll', "build/windows/x64/$mode/ContainerPreview.dll")
    }
    Assert-ProtectedImages
    if ($Package) {
        $releaseRoot = Join-Path $projectRoot 'releases'
        if (-not (Test-Path -LiteralPath $releaseRoot)) { New-Item -ItemType Directory -Path $releaseRoot | Out-Null }
        if ((Get-Item -LiteralPath $releaseRoot).Attributes -band [System.IO.FileAttributes]::ReparsePoint) { throw 'Refusing a linked release directory' }
        $stem = 'ContainerPreview-0.1.0-mc26.45-26.51-26.52-win-x64-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
        $folder = Join-Path $releaseRoot $stem
        $archive = "$folder.zip"
        if ((Test-Path -LiteralPath $folder) -or (Test-Path -LiteralPath $archive)) { throw 'Refusing to overwrite an existing release' }
        New-Item -ItemType Directory -Path $folder | Out-Null
        Assert-RetainedDll
        Copy-Item -LiteralPath $RetainedDll -Destination (Join-Path $folder 'ContainerPreview.dll')
        $payload = @('ContainerPreview.dll', 'config.json', 'config.schema.json', 'mod.json', 'manifest.json', 'README.md', 'THIRD_PARTY_NOTICES.txt')
        foreach ($name in $payload | Where-Object { $_ -ne 'ContainerPreview.dll' }) {
            $source = Join-Path $projectRoot $name
            if ((Get-Item -LiteralPath $source).Attributes -band [System.IO.FileAttributes]::ReparsePoint) { throw "Refusing linked source $name" }
            Copy-Item -LiteralPath $source -Destination (Join-Path $folder $name)
        }
        $lines = foreach ($name in $payload | Sort-Object -CaseSensitive) {
            (Get-FileHash -LiteralPath (Join-Path $folder $name) -Algorithm SHA256).Hash + '  ' + $name
        }
        [System.IO.File]::WriteAllText((Join-Path $folder 'SHA256SUMS.txt'), ($lines -join "`n") + "`n", [System.Text.Encoding]::ASCII)
        Invoke-Checked 'py' @('-3.11', 'tools/verify_artifacts.py', '--package', $folder, '--required-dll-sha256', $testedDllHash)
        Compress-Archive -LiteralPath @((Get-ChildItem -LiteralPath $folder -File).FullName) -DestinationPath $archive -CompressionLevel Optimal
        Invoke-Checked 'py' @('-3.11', 'tools/verify_artifacts.py', '--zip', $archive, '--required-dll-sha256', $testedDllHash)
        $archiveHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
        [System.IO.File]::WriteAllText("$archive.sha256", "$archiveHash  $stem.zip`n", [System.Text.Encoding]::ASCII)
        Write-Host "Local stable ZIP (retained gameplay-tested DLL): $archive"
        Write-Host "ZIP SHA256: $archiveHash"
    }
    Assert-RetainedDll
    Write-Host 'OFFLINE VERIFICATION PASSED. Rebuilds are offline evidence only; stable packaging retains the exact user-tested DLL. No installation or game launch was performed.'
} finally {
    Pop-Location
    Assert-ProtectedImages
    Assert-RetainedDll
}
