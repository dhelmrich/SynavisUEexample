@echo off
start .\MinimalWebRTC.exe -PixelStreamingIP=127.0.0.1 -PixelStreamingPort=8888 -RenderOffscreen -ForceRes -ResX=2000 -RexY=1000 -Unattended -PixelStreamingH264Profile=HIGH -PixelStreamingWebRTCDegradationPreference=MAINTAIN_QUALITY

