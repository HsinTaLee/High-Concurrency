# ./record.ps1 -ProcessName "server" -LogFile "server.log" -Interval 2
param(
    [string]$ProcessName = "server",
    [string]$LogFile = "server_monitor.log",
    [int]$Interval = 2   # 每隔幾秒紀錄一次
)

Write-Host "監控 $ProcessName，每 $Interval 秒紀錄一次 → $LogFile"

# 建立/清空 log 檔
"" | Out-File -FilePath $LogFile -Encoding utf8

while ($true) {
    $proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue
    if ($null -eq $proc) {
        Write-Host "找不到 $ProcessName，監控結束。"
        break
    }

    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $cpu = $proc.CPU
    $mem = [math]::Round($proc.WorkingSet64 / 1MB, 2)
    $threads = $proc.Threads.Count

    "$timestamp, CPU=$cpu, RAM=${mem}MB, Threads=$threads" | Out-File -FilePath $LogFile -Append -Encoding utf8

    Start-Sleep -Seconds $Interval
}