#!/bin/sh
set -eu
mkdir -p /etc/mosquitto/iot /etc/iot-live
python3 - <<'PY'
import json
from pathlib import Path
c=json.loads(Path('/tmp/iot-mqtt-secrets.json').read_text())
p=Path('/etc/mosquitto/iot-passwords')
p.write_text(c['device_username']+':'+c['device_password']+'\n'+c['username']+':'+c['password']+'\n')
p.chmod(0o600)
p=Path('/etc/iot-live/mqtt.json')
p.write_text(json.dumps({'username':c['username'],'password':c['password']}))
p.chmod(0o600)
Path('/etc/mosquitto/iot-acl').write_text('user '+c['device_username']+'\ntopic write iot/tp2/readings\n\nuser '+c['username']+'\ntopic read iot/tp2/readings\n')
PY
mosquitto_passwd -U /etc/mosquitto/iot-passwords
if [ ! -e /etc/mosquitto/iot/ca.crt ]; then
    openssl req -x509 -newkey rsa:2048 -nodes -days 3650 -keyout /etc/mosquitto/iot/ca.key -out /etc/mosquitto/iot/ca.crt -subj '/CN=Groupe 4 IoT MQTT CA' >/dev/null 2>&1
    openssl req -newkey rsa:2048 -nodes -keyout /etc/mosquitto/iot/server.key -out /etc/mosquitto/iot/server.csr -subj '/CN=192.0.2.1' >/dev/null 2>&1
    printf 'subjectAltName=IP:192.0.2.1,DNS:192.0.2.1\nextendedKeyUsage=serverAuth\n' > /etc/mosquitto/iot/server.ext
    openssl x509 -req -in /etc/mosquitto/iot/server.csr -CA /etc/mosquitto/iot/ca.crt -CAkey /etc/mosquitto/iot/ca.key -CAcreateserial -out /etc/mosquitto/iot/server.crt -days 730 -extfile /etc/mosquitto/iot/server.ext >/dev/null 2>&1
fi
chown root:mosquitto /etc/mosquitto/iot/server.key /etc/mosquitto/iot-passwords /etc/mosquitto/iot-acl
chmod 640 /etc/mosquitto/iot/server.key /etc/mosquitto/iot-passwords /etc/mosquitto/iot-acl
chmod 600 /etc/mosquitto/iot/ca.key
mkdir -p /etc/mosquitto/certs
install -o root -g mosquitto -m 640 /etc/mosquitto/iot/server.key /etc/mosquitto/certs/iot-server.key
install -m 644 /etc/mosquitto/iot/server.crt /etc/mosquitto/certs/iot-server.crt
cat > /etc/mosquitto/conf.d/iot-group4.conf <<'MQTT'
allow_anonymous false
password_file /etc/mosquitto/iot-passwords
acl_file /etc/mosquitto/iot-acl
listener 1883 127.0.0.1
listener 8883 0.0.0.0
certfile /etc/mosquitto/certs/iot-server.crt
keyfile /etc/mosquitto/certs/iot-server.key
tls_version tlsv1.2
MQTT
systemctl enable mosquitto
systemctl reset-failed mosquitto
systemctl restart mosquitto
python3 -m venv /opt/apps/iot-live/venv
/opt/apps/iot-live/venv/bin/pip install --quiet paho-mqtt==2.1.0
release=/opt/apps/iot-live/releases/20261013-mqtt-v1
mkdir -p "$release"
tar -xzf /tmp/iot-live-mqtt.tgz -C "$release"
mkdir -p /etc/systemd/system/iot-live.service.d
cat > /etc/systemd/system/iot-live.service.d/mqtt.conf <<'UNIT'
[Service]
ExecStart=
ExecStart=/opt/apps/iot-live/venv/bin/python /opt/apps/iot-live/current/server.py
LoadCredential=mqtt.json:/etc/iot-live/mqtt.json
Environment=IOT_MQTT_CONFIG=/run/credentials/iot-live.service/mqtt.json
UNIT
ln -sfn "$release" /opt/apps/iot-live/current
systemctl daemon-reload
systemctl restart iot-live
rm -f /tmp/iot-mqtt-secrets.json /tmp/iot-live-mqtt.tgz
systemctl is-active mosquitto iot-live
