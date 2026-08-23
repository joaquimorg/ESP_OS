param(
    [int]$Port = 8000
)

$ErrorActionPreference = 'Stop'
$directory = $PSScriptRoot
$page = "http://127.0.0.1:$Port/minios-webshell.html"

$python = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $python) {
    $python = Get-Command py.exe -ErrorAction SilentlyContinue
}
if ($null -eq $python) {
    throw 'Python was not found. Open minios-webshell.html directly in the browser.'
}

if ($python.Name -eq 'py.exe') {
    $arguments = @('-3', '-m', 'http.server', $Port, '--directory', $directory)
} else {
    $arguments = @('-m', 'http.server', $Port, '--directory', $directory)
}

$server = Start-Process -FilePath $python.Source -ArgumentList $arguments `
    -PassThru -WindowStyle Hidden
try {
    Start-Sleep -Milliseconds 400
    Start-Process $page
    Write-Host "MiniOS WebShell: $page (press Ctrl+C to stop)"
    Wait-Process -Id $server.Id
} finally {
    if (-not $server.HasExited) {
        Stop-Process -Id $server.Id
    }
}
