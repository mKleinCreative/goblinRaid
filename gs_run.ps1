# Runs D:\goblinRaid\gs_step.py inside the running Unreal editor via the in-editor MCP
# server on 127.0.0.1:8000/mcp. No param block, no pipeline tricks - this mirrors the
# exact shape that was proven working on 2026-08-02, because a param()-based variant of
# the same logic hung every time and was not worth debugging.
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
                       clientInfo = @{ name = 'gs'; version = '1' } } }
$r = Send-Rpc $init $null 30
$sid = $r.sid
if ($sid -is [array]) { $sid = $sid[0] }
Write-Output ('INIT ok=' + $r.ok)

$n = Send-Rpc @{ jsonrpc = '2.0'; method = 'notifications/initialized' } $sid 20
Write-Output ('NOTIFY ok=' + $n.ok)

$code = [System.IO.File]::ReadAllText('D:\goblinRaid\gs_step.py')
$c = Send-Rpc @{ jsonrpc = '2.0'; id = 2; method = 'tools/call'
                 params = @{ name = 'execute_python_code'; arguments = @{ code = $code } } } $sid 150
if ($c.ok) {
    $obj = $c.body | ConvertFrom-Json
    foreach ($item in $obj.result.content) { Write-Output $item.text }
} else {
    Write-Output ('CALL FAIL ' + $c.err)
}
