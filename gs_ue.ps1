# gs_ue.ps1 - run a Python file inside the RUNNING Unreal editor by talking to the
# in-editor MCP server directly over HTTP, bypassing the desktop app's MCP proxy.
#
# The editor's ModelContextProtocol server listens on 127.0.0.1:8000/mcp and works
# fine even when the desktop app fails to surface the unreal tools (three drops on
# 2026-08-02 alone). Requires 'ModelContextProtocol.StartServer' to have been run
# once in the editor console.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File D:\goblinRaid\gs_ue.ps1 -PyFile D:\goblinRaid\gs_step.py

param([Parameter(Mandatory = $true)][string]$PyFile,
      [int]$TimeoutSec = 180)

$ErrorActionPreference = 'Continue'
$url = 'http://127.0.0.1:8000/mcp'
$headers = @{ 'Accept' = 'application/json, text/event-stream' }

function Send-Rpc($body, $sid, $timeout) {
    $h = $headers.Clone()
    if ($sid) { $h['Mcp-Session-Id'] = $sid }
    $json = $body | ConvertTo-Json -Depth 20 -Compress
    try {
        $r = Invoke-WebRequest -Uri $url -Method Post -Body $json -ContentType 'application/json' -Headers $h -TimeoutSec $timeout -UseBasicParsing
        return @{ ok = $true; body = $r.Content; sid = $r.Headers['Mcp-Session-Id'] }
    } catch {
        return @{ ok = $false; err = $_.Exception.Message }
    }
}

$init = @{ jsonrpc = '2.0'; id = 1; method = 'initialize'
           params = @{ protocolVersion = '2024-11-05'; capabilities = @{}
                       clientInfo = @{ name = 'gs-ue'; version = '1' } } }
$r = Send-Rpc $init $null 30
if (-not $r.ok) { Write-Output ('INIT FAILED: ' + $r.err); exit 1 }
$sid = $r.sid
if ($sid -is [array]) { $sid = $sid[0] }

Send-Rpc @{ jsonrpc = '2.0'; method = 'notifications/initialized' } $sid 20 | Out-Null

$code = Get-Content -LiteralPath $PyFile -Raw
$c = Send-Rpc @{ jsonrpc = '2.0'; id = 2; method = 'tools/call'
                 params = @{ name = 'execute_python_code'; arguments = @{ code = $code } } } $sid $TimeoutSec
if (-not $c.ok) { Write-Output ('CALL FAILED: ' + $c.err); exit 2 }

# Unwrap: JSON-RPC envelope -> content[0].text -> the tool's own JSON blob.
try {
    $obj = $c.body | ConvertFrom-Json
    if ($obj.error) { Write-Output ('RPC ERROR: ' + ($obj.error | ConvertTo-Json -Depth 6 -Compress)); exit 3 }
    foreach ($item in $obj.result.content) {
        try {
            $inner = $item.text | ConvertFrom-Json
            if ($null -ne $inner.output -and $inner.output -ne '') { Write-Output $inner.output }
            if ($inner.success -eq $false) { Write-Output ('PYTHON FAILED: ' + $item.text) }
        } catch { Write-Output $item.text }
    }
} catch { Write-Output $c.body }
