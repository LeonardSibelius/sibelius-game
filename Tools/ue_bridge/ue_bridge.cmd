@echo off
REM Convenience wrapper: run the bridge with the engine's bundled Python (no standalone Python needed).
REM Usage: Tools\ue_bridge\ue_bridge.cmd get-transform BookRain
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" "%~dp0ue_bridge.py" %*
