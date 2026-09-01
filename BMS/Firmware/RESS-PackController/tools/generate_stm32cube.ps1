param(
    [string]$CubeMxExecutable = $env:CUBEMX_EXECUTABLE,
    [string]$FirmwarePackagePath = $env:STM32CUBE_G4_1_5_2_PATH
)

$ErrorActionPreference = 'Stop'

$repository = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$canonicalIoc = Join-Path $repository 'PackController.ioc'
$cubeRoot = Join-Path $repository 'generated\stm32cube\PackController'
$generatedIoc = Join-Path $cubeRoot 'PackController.ioc'

if ([string]::IsNullOrWhiteSpace($CubeMxExecutable)) {
    $installedCubeMx = 'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeMX\STM32CubeMX.exe'
    if (Test-Path -LiteralPath $installedCubeMx) {
        $CubeMxExecutable = $installedCubeMx
    } else {
        $cubeMxCommand = Get-Command STM32CubeMX -ErrorAction SilentlyContinue
        if ($null -ne $cubeMxCommand) {
            $CubeMxExecutable = $cubeMxCommand.Source
        }
    }
}

if ([string]::IsNullOrWhiteSpace($CubeMxExecutable) -or
    -not (Test-Path -LiteralPath $CubeMxExecutable)) {
    throw 'STM32CubeMX not found; pass -CubeMxExecutable or set CUBEMX_EXECUTABLE.'
}

$cubeVersion = (Get-Item -LiteralPath $CubeMxExecutable).VersionInfo.ProductVersion
if ($cubeVersion -notmatch '6\.9\.2') {
    throw "STM32CubeMX 6.9.2 is required; found '$cubeVersion'."
}

if ([string]::IsNullOrWhiteSpace($FirmwarePackagePath)) {
    $FirmwarePackagePath = Join-Path $env:USERPROFILE `
        'STM32Cube\Repository\STM32Cube_FW_G4_V1.5.2'
}
$firmwareManifest = Join-Path $FirmwarePackagePath 'package.xml'
if (-not (Test-Path -LiteralPath $firmwareManifest)) {
    throw "STM32CubeG4 1.5.2 not found at '$FirmwarePackagePath'."
}

$cubeJava = Join-Path (Split-Path $CubeMxExecutable -Parent) 'jre\bin\java.exe'
if (-not (Test-Path -LiteralPath $cubeJava)) {
    throw "CubeMX Java launcher not found at '$cubeJava'."
}

New-Item -ItemType Directory -Path $cubeRoot -Force | Out-Null
$temporaryDirectory = Join-Path ([IO.Path]::GetTempPath()) (
    'packcontroller-cubemx-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temporaryDirectory | Out-Null
$batchScript = Join-Path $temporaryDirectory 'generate.script'
$backupRoot = Join-Path $temporaryDirectory 'PackController-backup'
Copy-Item -LiteralPath $cubeRoot -Destination $backupRoot -Recurse
Copy-Item -LiteralPath $canonicalIoc -Destination $generatedIoc -Force

try {
    $iocForCubeMx = $generatedIoc.Replace('\', '/')
    $outputForCubeMx = (Split-Path $cubeRoot -Parent).Replace('\', '/')
    $firmwareForCubeMx = $FirmwarePackagePath.Replace('\', '/')
    @(
        "config load $iocForCubeMx"
        "project setCustomFWPath `"$firmwareForCubeMx`""
        "project path $outputForCubeMx"
        'project generate'
        'exit'
    ) | Set-Content -LiteralPath $batchScript -Encoding Ascii

    $cubeStdout = Join-Path $temporaryDirectory 'cubemx.stdout.log'
    $cubeStderr = Join-Path $temporaryDirectory 'cubemx.stderr.log'
    $cubeArguments = @(
        '-jar'
        "`"$CubeMxExecutable`""
        '-q'
        "`"$batchScript`""
    )
    $cubeProcess = Start-Process -FilePath $cubeJava `
        -ArgumentList $cubeArguments `
        -WorkingDirectory (Split-Path $CubeMxExecutable -Parent) `
        -RedirectStandardOutput $cubeStdout `
        -RedirectStandardError $cubeStderr `
        -WindowStyle Hidden `
        -Wait `
        -PassThru
    if ($cubeProcess.ExitCode -ne 0) {
        Get-Content -LiteralPath $cubeStdout, $cubeStderr -Tail 100 | Write-Host
        throw "CubeMX generation failed with exit code $($cubeProcess.ExitCode)."
    }

    $generatedDrivers = Join-Path $cubeRoot 'Drivers'
    $packageDrivers = Join-Path $FirmwarePackagePath 'Drivers'
    $driverPrefixLength = $generatedDrivers.TrimEnd('\').Length + 1
    foreach ($driverFile in Get-ChildItem -LiteralPath $generatedDrivers -File -Recurse) {
        $relativeDriver = $driverFile.FullName.Substring($driverPrefixLength)
        $packageDriver = Join-Path $packageDrivers $relativeDriver
        if (-not (Test-Path -LiteralPath $packageDriver)) {
            throw "Generated vendor file has no STM32CubeG4 1.5.2 source: $relativeDriver"
        }
        $generatedHash = (Get-FileHash -LiteralPath $driverFile.FullName -Algorithm SHA256).Hash
        $packageHash = (Get-FileHash -LiteralPath $packageDriver -Algorithm SHA256).Hash
        if ($generatedHash -ne $packageHash) {
            throw "Generated vendor file differs from STM32CubeG4 1.5.2: $relativeDriver"
        }
    }

    $coreSource = Join-Path $cubeRoot 'Core\Src'
    $coreStartup = Join-Path $cubeRoot 'Core\Startup'
    New-Item -ItemType Directory -Path $coreSource -Force | Out-Null
    New-Item -ItemType Directory -Path $coreStartup -Force | Out-Null

    $normalizations = @(
        @{
            Name = 'startup_stm32g483vetx.s'
            Destination = Join-Path $coreStartup 'startup_stm32g483vetx.s'
        }
        @{
            Name = 'syscalls.c'
            Destination = Join-Path $coreSource 'syscalls.c'
        }
    )
    foreach ($normalization in $normalizations) {
        $name = $normalization.Name
        $destination = $normalization.Destination
        $candidate = Get-ChildItem -LiteralPath $cubeRoot -Recurse -File -Filter $name |
            Where-Object { $_.FullName -ne $destination } |
            Select-Object -First 1
        if ($null -ne $candidate) {
            Move-Item -LiteralPath $candidate.FullName -Destination $destination -Force
        }
        if (-not (Test-Path -LiteralPath $destination)) {
            throw "CubeMX did not generate $name."
        }
    }

    $cubeIde = Join-Path $cubeRoot 'STM32CubeIDE'
    Get-ChildItem -LiteralPath $cubeIde -Directory -Force |
        Remove-Item -Recurse -Force
    foreach ($metadata in @('.project', '.cproject')) {
        $metadataPath = Join-Path $cubeIde $metadata
        if (Test-Path -LiteralPath $metadataPath) {
            Remove-Item -LiteralPath $metadataPath -Force
        }
    }

    foreach ($unusedFile in @(
        (Join-Path $coreSource 'sysmem.c'),
        (Join-Path $cubeRoot '.mxproject')
    )) {
        if (Test-Path -LiteralPath $unusedFile) {
            Remove-Item -LiteralPath $unusedFile -Force
        }
    }

    # CubeMX may rewrite its package selection according to per-user updater
    # preferences. The custom path and byte-for-byte vendor check above define
    # the package provenance; keep the canonical, path-independent IOC intact.
    Copy-Item -LiteralPath $canonicalIoc -Destination $generatedIoc -Force
    Write-Output 'Generated STM32Cube sources with CubeMX 6.9.2 / STM32CubeG4 1.5.2.'
} catch {
    if (Test-Path -LiteralPath $backupRoot) {
        Remove-Item -LiteralPath $cubeRoot -Recurse -Force
        Copy-Item -LiteralPath $backupRoot -Destination $cubeRoot -Recurse
    }
    throw
} finally {
    if (Test-Path -LiteralPath $temporaryDirectory) {
        Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
    }
}
