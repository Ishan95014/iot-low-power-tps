#!/bin/sh
set -eu
release=/opt/apps/iot-live/releases/20261013-live-v1
mkdir -p "$release" /etc/iot-live
tar -xzf /tmp/iot-live-release.tgz -C "$release"
if [ ! -e /etc/iot-live/device-key.txt ]; then
    install -m 600 /tmp/iot-live-device-key.txt /etc/iot-live/device-key.txt
else
    cmp /tmp/iot-live-device-key.txt /etc/iot-live/device-key.txt
fi
ln -sfn "$release" /opt/apps/iot-live/current
cat > /etc/systemd/system/iot-live.service <<'UNIT'
[Unit]
Description=ESP32 live readings microapp
After=network.target
[Service]
DynamicUser=yes
StateDirectory=iot-live
WorkingDirectory=/opt/apps/iot-live/current
ExecStart=/usr/bin/python3 /opt/apps/iot-live/current/server.py
Environment=IOT_PORT=5048
Environment=IOT_DATABASE=/var/lib/iot-live/readings.sqlite3
LoadCredential=device-key.txt:/etc/iot-live/device-key.txt
Restart=on-failure
NoNewPrivileges=yes
ProtectSystem=strict
ProtectHome=yes
PrivateTmp=yes
UMask=0077
[Install]
WantedBy=multi-user.target
UNIT
systemctl daemon-reload
systemctl enable --now iot-live.service
systemctl restart iot-live.service
python3 - <<'PY'
from pathlib import Path
import shutil
p=Path('/etc/cloudflared/config.yml')
text=p.read_text()
route='  - hostname: iot.example.invalid\n    path: ^/apps/iot-live(/.*)?$\n    service: http://127.0.0.1:5048\n'
if 'path: ^/apps/iot-live' not in text:
    backup=Path('/etc/cloudflared/config.yml.bak-iot-live-20261013')
    if not backup.exists(): shutil.copy2(p,backup)
    marker='  - hostname: iot.example.invalid\n'
    if marker not in text: raise SystemExit('Expected hostname route missing')
    p.write_text(text.replace(marker,route+marker,1))
PY
if ! cloudflared tunnel ingress validate; then
    cp /etc/cloudflared/config.yml.bak-iot-live-20261013 /etc/cloudflared/config.yml
    exit 1
fi
systemctl restart cloudflared
rm -f /tmp/iot-live-device-key.txt /tmp/iot-live-release.tgz
systemctl is-active iot-live cloudflared
