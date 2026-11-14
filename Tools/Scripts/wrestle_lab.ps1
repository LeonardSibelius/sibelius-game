# Run the wrestling lab offscreen: lab.ps1 <tag> <seconds> <pair> <shot interval>
param([string]$Tag = "run", [int]$Seconds = 80, [int]$Pair = 0, [string]$Every = "0.25")
$proj = "C:\Users\wpark\projects\sibelius-game"
$shots = "$proj\Saved\Screenshots\WindowsEditor"
$out = "$proj\Saved\WrestleLab\$Tag"
Remove-Item "$shots\Wrestle_*.png" -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $out | Out-Null
Remove-Item "$out\*" -ErrorAction SilentlyContinue
$a = "`"$proj\SibeliusGame.uproject`" /Game/Developers/wpark/WrestleLab/L_WrestleLab -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed -WrestleShots=$Every -WrestleLog -WrestlePair=$Pair -unattended -nosplash -nosound -nop4 -log -abslog=`"$out\lab.log`""
$p = Start-Process -FilePath "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList $a -PassThru
Start-Sleep -Seconds $Seconds
Stop-Process -Id $p.Id -Force
Start-Sleep -Seconds 1
Move-Item "$shots\Wrestle_*.png" $out -ErrorAction SilentlyContinue
"shots: " + (Get-ChildItem "$out\Wrestle_*.png").Count
