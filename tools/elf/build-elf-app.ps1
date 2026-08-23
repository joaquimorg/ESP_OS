param(
    [Parameter(Mandatory = $true)]
    [string]$Source,

    [Parameter(Mandatory = $false)]
    [string]$Output
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sourcePath = (Resolve-Path $Source).Path

if ([string]::IsNullOrWhiteSpace($Output)) {
    $Output = [System.IO.Path]::ChangeExtension($sourcePath, '.elf')
}
$outputPath = [System.IO.Path]::GetFullPath($Output)
$objectPath = [System.IO.Path]::ChangeExtension($outputPath, '.o')
$outputDirectory = Split-Path -Parent $outputPath
if (-not (Test-Path -LiteralPath $outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory | Out-Null
}

$compilerCommand = Get-Command 'riscv32-esp-elf-gcc.exe' -ErrorAction SilentlyContinue
if ($null -ne $compilerCommand) {
    $compiler = $compilerCommand.Source
} else {
    $toolsRoots = @()
    if (-not [string]::IsNullOrWhiteSpace($env:IDF_TOOLS_PATH)) {
        $toolsRoots += $env:IDF_TOOLS_PATH
    }
    $toolsRoots += 'C:\Espressif'
    $compiler = $toolsRoots |
        ForEach-Object {
            Get-ChildItem -LiteralPath (Join-Path $_ 'tools\riscv32-esp-elf') `
                -Recurse -Filter 'riscv32-esp-elf-gcc.exe' -File `
                -ErrorAction SilentlyContinue
        } |
        Sort-Object FullName -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}
if ([string]::IsNullOrWhiteSpace($compiler)) {
    throw 'riscv32-esp-elf-gcc.exe was not found. Export the ESP-IDF environment first.'
}

$common = @(
    '-march=rv32imc_zicsr_zifencei',
    '-mabi=ilp32'
)
$compileArguments = $common + @(
    '-fPIC',
    '-ffreestanding',
    '-fno-builtin',
    '-fno-stack-protector',
    '-msmall-data-limit=0',
    '-mno-relax',
    '-Os',
    '-Wall',
    '-Wextra',
    '-Werror',
    '-I', (Join-Path $projectRoot 'components\minios_api\include'),
    '-c', $sourcePath,
    '-o', $objectPath
)
& $compiler $compileArguments
if ($LASTEXITCODE -ne 0) {
    throw "Compilation failed with exit code $LASTEXITCODE."
}

$linkArguments = $common + @(
    '-nostdlib',
    '-shared',
    '-Wl,-z,max-page-size=16',
    '-Wl,--entry=minios_app_main',
    '-Wl,--unresolved-symbols=ignore-all',
    '-Wl,--emit-relocs',
    '-Wl,--no-relax',
    $objectPath,
    '-o', $outputPath
)
& $compiler $linkArguments
if ($LASTEXITCODE -ne 0) {
    throw "Link failed with exit code $LASTEXITCODE."
}

Remove-Item -LiteralPath $objectPath
Write-Output "Created $outputPath"
