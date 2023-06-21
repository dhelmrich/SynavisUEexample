@echo off
start .\MinimalWebRTC.exe -PixelStreamingIP=127.0.0.1 -PixelStreamingPort=8888 ^
 -RenderOffscreen -ForceRes -ResX=1280 -RexY=720 -Unattended -PixelStreamingH264Profile=HIGH ^
 -PixelStreamingWebRTCDegradationPreference=MAINTAIN_QUALITY ^
-ExecCmds="PixelStreaming.WebRTC.DisableTransmitAudio true, PixelStreaming.Encoder.Codec H265, PixelStreaming.WebRTC.DisableAudioSync true, PixelStreaming.AllowPixelStreamingCommands true, PixelStreaming.Encoder.KeyframeInterval 1, PixelStreaming.Encoder.EnableFillerData true, PixelStreaming.WebRTC.DegradationPreference MAINTAIN_QUALITY, PixelStreaming.WebRTC.Fps 10, t.MaxFPS 10"

