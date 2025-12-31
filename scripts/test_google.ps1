$ErrorActionPreference = "Stop"

# 定义路径
$ClashBin = ".\bin\Debug\clash-core-cpp.exe"
$LogFile = "$env:USERPROFILE\.config\clash\clash.log"
$ErrFile = "$env:USERPROFILE\.config\clash\clash.err"

# 确保日志文件存在
$LogDir = Split-Path $LogFile
if (-not (Test-Path $LogDir)) {
    New-Item -ItemType Directory -Path $LogDir | Out-Null
}
"" | Set-Content $LogFile
"" | Set-Content $ErrFile

Write-Host "Starting Clash Core..."
# 启动 Clash (后台运行)
$ClashProcess = Start-Process -FilePath $ClashBin -RedirectStandardOutput $LogFile -RedirectStandardError $ErrFile -PassThru -NoNewWindow

# 等待启动
Start-Sleep -Seconds 3

Write-Host "Clash started with PID $($ClashProcess.Id)"
Write-Host "Monitoring log file: $LogFile"

Write-Host "Testing connection to www.google.com via proxy (127.0.0.1:7890)..."

# 测试连接
# 使用 curl.exe 避免 PowerShell 的 curl 别名
try {
    & curl.exe -x http://127.0.0.1:7890 -I https://www.google.com --connect-timeout 15
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Connection successful!" -ForegroundColor Green
    } else {
        Write-Host "Connection failed with exit code $LASTEXITCODE" -ForegroundColor Red
    }
} catch {
    Write-Host "Error running curl: $_" -ForegroundColor Red
}

# 清理
Write-Host "Stopping Clash..."
Stop-Process -Id $ClashProcess.Id -Force

# 显示最后的日志摘要
Write-Host "--- Last 10 lines of log ---"
Get-Content $LogFile -Tail 10
