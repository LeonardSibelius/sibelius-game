# retry_itch_push.ps1 - push 1.3.0 to itch as soon as itch.io comes back up.
#
# WHY THIS EXISTS. On 2026-09-03/04 itch.io returned HTTP 500 on every wharf endpoint -
# /wharf/builds, /wharf/channels, and /games/239683/uploads on broth.itch.zone, which is
# butler's own download listing. Butler could not even fetch its own upgrade. That is an
# itch-side outage, not a bad build and not a blocked account, and there is nothing to do
# about it but wait. Walt: "can't sleep, please retry."
#
# So: nobody waits up. This retries every RETRY_MINUTES until it works or MAX_HOURS is up,
# and stops the moment it succeeds. butler block-diffs against the last build, so the
# 7.25 GB archive goes up as a few MB.
#
# TO STOP IT: close the window, or Get-Process powershell | Where-Object CommandLine -like
# "*retry_itch_push*" | Stop-Process.
#
# The log is the record - read it in the morning rather than watching it tonight.

$butler  = "C:\Users\wpark\butler\butler.exe"
$build   = "C:\Users\wpark\builds\sibelius-v1.3.0\Windows"
$channel = "leonardsibelius/leonard-sibelius:windows"
$version = "1.3.0"
$log     = "C:\Users\wpark\projects\sibelius-game\itch-push-retry.log"

$RETRY_MINUTES = 10
$MAX_HOURS     = 10

$deadline = (Get-Date).AddHours($MAX_HOURS)
$attempt  = 0

"=== retry started $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') - every $RETRY_MINUTES min until $deadline ===" |
  Out-File -FilePath $log -Encoding utf8 -Append

while ((Get-Date) -lt $deadline) {
    $attempt++
    $stamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'

    $out = & $butler push $build $channel --userversion $version 2>&1 | Out-String
    $ok  = ($LASTEXITCODE -eq 0)

    if ($ok) {
        "[$stamp] attempt $attempt : SUCCESS" | Out-File -FilePath $log -Encoding utf8 -Append
        $out | Out-File -FilePath $log -Encoding utf8 -Append
        "=== 1.3.0 IS LIVE - https://leonardsibelius.itch.io/leonard-sibelius ===" |
          Out-File -FilePath $log -Encoding utf8 -Append
        exit 0
    }

    # One line per failure, not the whole stack: ten hours of this should stay readable.
    $first = ($out -split "`n" | Where-Object { $_ -match '\S' } | Select-Object -Last 1).Trim()
    "[$stamp] attempt $attempt : still down - $first" | Out-File -FilePath $log -Encoding utf8 -Append

    Start-Sleep -Seconds ($RETRY_MINUTES * 60)
}

"[$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] GAVE UP after $MAX_HOURS hours / $attempt attempts. itch is still down." |
  Out-File -FilePath $log -Encoding utf8 -Append
exit 1
