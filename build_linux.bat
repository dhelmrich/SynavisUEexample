@echo off

REM store current directory
set current_dir=%cd%
set uat_file=%UEPATH%\..\..\Build\BatchFiles\RunUAT.bat

echo %uat_file%

"%uat_file%" BuildCookRun -project=%current_dir%\MinimalWebRTC.uproject -noP4 -platform=Linux -clientconfig=Development -cook -allmaps -build -stage -pak -archive -archivedirectory=%current_dir% -nocompileeditor
