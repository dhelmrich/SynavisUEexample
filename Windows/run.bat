@echo off

REM save directory of this script
set SCRIPT_DIR=%~dp0

start %SCRIPT_DIR%\MinimalWebRTC.exe -PixelStreamingIP=127.0.0.1 -PixelStreamingPort=8888 ^
 -RenderOffscreen -ForceRes -ResX=400 -RexY=300 -Unattended ^
 -PixelStreamingWebRTCDegradationPreference=MAINTAIN_QUALITY ^
 -PixelStreamingWebRTCVideoEncoder=H264 ^
-ExecCmds="PixelStreaming.WebRTC.DisableTransmitAudio true, PixelStreaming.Encoder.Codec H264, PixelStreaming.WebRTC.DisableAudioSync true, PixelStreaming.AllowPixelStreamingCommands true, PixelStreaming.Encoder.EnableFillerData true, PixelStreaming.WebRTC.DegradationPreference MAINTAIN_QUALITY, t.MaxFPS 15"

REM , PixelStreaming.Encoder.KeyframeInterval 1, PixelStreaming.WebRTC.Fps 10
