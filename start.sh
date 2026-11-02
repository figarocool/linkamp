#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# Pi panel: portrait HDMI output rotated to landscape 1280x400
if wlr-randr --output HDMI-A-1 &>/dev/null; then
    wlr-randr --output HDMI-A-1 --transform 270
# UTM / virtio display: landscape 1280x400 (no rotation)
elif wlr-randr --output Virtual-1 &>/dev/null; then
    if ! wlr-randr --output Virtual-1 2>/dev/null | grep -q '1280x400'; then
        wlr-randr --output Virtual-1 --custom-mode 1280x400@60 || true
    fi
fi

source "$SCRIPT_DIR/venv/bin/activate"

# yt-dlp uses the user's Node.js runtime for YouTube's JavaScript challenges.
for node_bin in "$HOME"/.config/nvm/versions/node/*/bin; do
    if [[ -x "$node_bin/node" ]]; then
        PATH="$node_bin:$PATH"
    fi
done
export PATH

# Prefer Qt's FFmpeg media backend on desktop builds; the system GStreamer
# backend emits a GLib critical when opening remote YouTube audio streams.
export QT_MEDIA_BACKEND="${QT_MEDIA_BACKEND:-ffmpeg}"

exec env PYTHONPATH="$SCRIPT_DIR/python" "$SCRIPT_DIR/build/player" "$@"
