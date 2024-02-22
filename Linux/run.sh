# extract the IP address of the machine
adrline=$(ifconfig | awk /ib0/,/^$/ | awk /inet/,/$/)
# extract the IP address, which is the second field seperated by space
adr=$(echo $adrline | awk '{print $2}')

./MinimalWebRTC.sh -PixelStreamingIP=$adr -PixelStreamingPort=8888 \
                   -RenderOffscreen -ForceRes -ResX=$2 -RexY=$3     \
                   -Unattended -PixelStreamingH264Profile=HIGH          \
                   -PixelStreamingDegradationPreference=MAINTAIN_QUALITY \
                   -ExecCmds="PixelStreaming.WebRTC.DisableTransmitAudio true, PixelStreaming.Encoder.Codec VP9, PixelStreaming.WebRTC.DisableAudioSync true, PixelStreaming.AllowPixelStreamingCommands true, PixelStreaming.Encoder.KeyframeInterval 1, PixelStreaming.Encoder.EnableFillerData true, PixelStreaming.WebRTC.DegradationPreference MAINTAIN_QUALITY, PixelStreaming.WebRTC.Fps $4, t.MaxFPS $4"


