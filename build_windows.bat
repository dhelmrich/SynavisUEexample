@echo off

REM store current directory
set current_dir=%cd%

"C:\work\UE_5.2\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project=%current_dir%\MinimalWebRTC.uproject -noP4 -platform=Win64 -clientconfig=Development -cook -allmaps -build -stage -pak -archive -archivedirectory="D:/Packages/photo"

