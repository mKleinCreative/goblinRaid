# gs_ue.ps1 - run a Python file inside the RUNNING Unreal editor by talking to the
# in-editor MCP server directly over HTTP, bypassing the desktop app's MCP proxy.
#
# The editor's ModelContextProtocol server listens on 127.0.0.1:8000/mcp and works
# fine even when the desktop app fails to surface the unreal tools (three drops on
# 2026-08-02 alone). Requires 'ModelContextProtocol.StartServer' to have been run
# once in the editor console - or launch the editor with
#   -ExecCmds=ModelContextProtocol.StartServer
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File D:\goblinRaid\gs_ue.ps1 -PyFile D:\goblinRaid\gs_step.py
#
# ---------------------------------------------------------------------------------------------
# REWRITTEN 2026-08-05: transport is curl.exe, not Invoke-WebRequest.
#
# Two separate bugs made every call hang forever with NO output at all - not even the
# "INIT FAILED" line - which reads exactly like "the editor is busy" and is not:
#
#   1. Accept: text/event-stream. MCP's Streamable HTTP lets the server choose between one JSON
#      response and an SSE stream, and it picks the stream if the client says it accepts one.
#      The stream is then held open, and the client blocks reading it forever.
#
#   2. Invoke-WebRequest itself. Even with Accept: application/json, it hung on this endpoint
#      under Windows PowerShell 5.1, and -TimeoutSec did not fire. curl.exe against the exact
#      same URL, headers and body returns in 0.34 s.
#
# So: PowerShell still builds the JSON (ConvertTo-Json escapes arbitrary Python source correctly,
# which is the one hard part), and curl.exe carries it. Bodies go through --data-binary @file
# rather than -d, because PowerShell mangles inner double quotes when passing a JSON string as a
# native-command argument - that produces a -32700 "Invalid JSON body!" from the server.
# ---------------------------------------------------------------------------------------------

param([Parameter(Mandatory = $true)][string]$PyFile,
      [int]$TimeoutSec = 180)

$ErrorActionPreference = 'Continue'
$url = 'http://127.0.0.1:8000/mcp'

if (-not (Test-Path -LiteralPath $PyFile)) {
    Write-Output ("PY FILE NOT FOUND: " + $PyFile)
    exit 1
}

$tmp = Join-Path $env:TEMP ("gs_ue_" + [guid]::NewGuid().ToString('N'))
$bodyFile = $tmp + '.json'
$hdrFile  = $tmp + '.hdr'

function Invoke-Mcp($obj, $sid, $timeout, $captureHeaders) {
    # -Depth 20 so a deeply nested arguments object is not silently truncated to "...".
    $json = $obj | ConvertTo-Json -Depth 20 -Compress
    # UTF8 without BOM: a BOM at the head of the body is itself invalid JSON to the server.
    [System.IO.File]::WriteAllText($bodyFile, $json, (New-Object System.Text.UTF8Encoding($false)))

    $args = @('-s', '--max-time', "$timeout", '-X', 'POST', $url,
              '-H', 'Content-Type: application/json',
              '-H', 'Accept: application/json',
              '--data-binary', "@$bodyFile")
    if ($sid) { $args += @('-H', "Mcp-Session-Id: $sid") }
    if ($captureHeaders) { $args += @('-D', $hdrFile) }

    return (& curl.exe @args | Out-String)
}

# ---- 1. initialize (captures the session id from the response headers)
$init = @{ jsonrpc = '2.0'; id = 1; method = 'initialize'
           params = @{ protocolVersion = '2024-11-05'; capabilities = @{}
                       clientInfo = @{ name = 'gs-ue'; version = '2' } } }
$initResp = Invoke-Mcp $init $null 30 $true

if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $hdrFile)) {
    Write-Output ("INIT FAILED (curl exit " + $LASTEXITCODE + "). Is the editor running with ModelContextProtocol.StartServer?")
    Remove-Item -LiteralPath $bodyFile, $hdrFile -Force -ErrorAction SilentlyContinue
    exit 1
}

$sid = $null
foreach ($line in (Get-Content -LiteralPath $hdrFile)) {
    if ($line -match '^(?i)Mcp-Session-Id:\s*(\S+)') { $sid = $Matches[1] }
}
if (-not $sid) {
    Write-Output 'INIT FAILED: no Mcp-Session-Id in response headers.'
    Write-Output $initResp
    Remove-Item -LiteralPath $bodyFile, $hdrFile -Force -ErrorAction SilentlyContinue
    exit 1
}

# ---- 2. the initialized notification (fire and forget)
Invoke-Mcp @{ jsonrpc = '2.0'; method = 'notifications/initialized' } $sid 20 $false | Out-Null

# ---- 3. run the Python
$code = Get-Content -LiteralPath $PyFile -Raw
$callResp = Invoke-Mcp @{ jsonrpc = '2.0'; id = 2; method = 'tools/call'
                          params = @{ name = 'execute_python_code'
                                      arguments = @{ code = $code } } } $sid $TimeoutSec $false

Remove-Item -LiteralPath $bodyFile, $hdrFile -Force -ErrorAction SilentlyContinue

if ($LASTEXITCODE -ne 0) {
    Write-Output ("CALL FAILED (curl exit " + $LASTEXITCODE + ") - likely exceeded -TimeoutSec " + $TimeoutSec)
    exit 2
}

# ---- 4. unwrap: JSON-RPC envelope -> content[].text -> the tool's own JSON blob
try {
    $obj = $callResp | ConvertFrom-Json
    if ($obj.error) {
        Write-Output ('RPC ERROR: ' + ($obj.error | ConvertTo-Json -Depth 6 -Compress))
        exit 3
    }
    $failed = $false
    foreach ($item in $obj.result.content) {
        try {
            $inner = $item.text | ConvertFrom-Json
            if ($null -ne $inner.output -and $inner.output -ne '') { Write-Output $inner.output }
            if ($inner.success -eq $false) {
                Write-Output ('PYTHON FAILED: ' + $item.text)
                $failed = $true
            }
        } catch {
            Write-Output $item.text
        }
    }
    if ($failed) { exit 4 }
} catch {
    Write-Output $callResp
}
