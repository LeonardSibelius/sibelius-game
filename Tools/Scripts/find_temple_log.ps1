# Locate the packaged runtime log and surface map-load / L_AI_Temple lines.
$root = "C:\Users\wpark\builds\sibelius-travel-test"
$log = (Get-ChildItem $root -Recurse -Filter "SibeliusGame.log" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
Write-Output "LOG: $log"
if (-not $log) { return }
$pat = 'L_AI_Temple|Failed to load|LoadMap|could.?n.?.?t find|BrowseToDefaultMap|travel fail|Temple|UEngine::LoadMap'
Select-String -Path $log -Pattern $pat -AllMatches |
  Select-Object -Last 30 |
  ForEach-Object { ($_.Line -replace '^\[[0-9.\-:]+\]\[\s*\d+\]\s*','').Trim() }
