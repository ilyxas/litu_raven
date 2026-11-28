[CmdletBinding(PositionalBinding=$false)]
param(
    [Parameter(Mandatory=$true)][ValidateSet('mc','node')][string]$App,
    [string]$Target = '',
    [Parameter(ValueFromRemainingArguments=$true)][string[]]$IdfArguments
)
$ErrorActionPreference = 'Stop'
$env:PYTHONUTF8 = '1'
if (-not $Target) { $Target = if ($App -eq 'mc') { 'esp32s3' } else { 'esp32' } }
if (-not $env:IDF_PATH) { $env:IDF_PATH = 'C:/esp/v6.1.1/esp-idf' }
if (-not $env:IDF_TOOLS_PATH) { $env:IDF_TOOLS_PATH = 'C:/Espressif' }
if (-not $env:IDF_PYTHON_ENV_PATH) { $env:IDF_PYTHON_ENV_PATH = 'C:/Espressif/tools/python/v6.1.1/venv' }
$python = Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts/python.exe'
$exports = & $python (Join-Path $env:IDF_PATH 'tools/idf_tools.py') export --format key-value
if ($LASTEXITCODE) { throw 'ESP-IDF environment export failed' }
foreach ($entry in $exports) {
    if ($entry -match '^([A-Z_][A-Z0-9_]*)=(.*)$') {
        $key = $Matches[1]; $value = $Matches[2].Replace('%PATH%', $env:PATH)
        [Environment]::SetEnvironmentVariable($key, $value, 'Process')
    }
}
$espRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$repoRoot = (Resolve-Path (Join-Path $espRoot '..')).Path
$buildDir = Join-Path $repoRoot "build/$App-$Target"
$projectDir = Join-Path $espRoot "examples/device_communication/$App"
if (-not $IdfArguments) { $IdfArguments = @('build') }
& $python (Join-Path $env:IDF_PATH 'tools/idf.py') -C $projectDir -B $buildDir "-DIDF_TARGET=$Target" "-DSDKCONFIG=$buildDir/sdkconfig" @IdfArguments
exit $LASTEXITCODE
