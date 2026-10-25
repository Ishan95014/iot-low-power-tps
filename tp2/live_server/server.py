#!/usr/bin/env python3
"""Small loopback-only telemetry API behind the existing HTTPS tunnel."""
import hmac
import json
import math
import os
import sqlite3
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

PREFIX = '/apps/iot-live/'
STATIC = Path(__file__).parent / 'static'
DATABASE = os.environ.get('IOT_DATABASE', '/var/lib/iot-live/readings.sqlite3')
KEY_FILE = Path(os.environ.get('IOT_KEY_FILE', str(Path(os.environ.get('CREDENTIALS_DIRECTORY', '.')) / 'device-key.txt')))

def connect():
    return sqlite3.connect(DATABASE, timeout=10)

def initialize():
    with connect() as db:
        db.execute('CREATE TABLE IF NOT EXISTS readings (id INTEGER PRIMARY KEY, timestamp REAL NOT NULL, temperature_c REAL NOT NULL, humidity_pct REAL NOT NULL, uptime_s INTEGER NOT NULL)')

def validate(data):
    if not isinstance(data, dict):
        raise ValueError('Expected a JSON object')
    for name, low, high in [('temperature_c', -40, 85), ('humidity_pct', 0, 100)]:
        value = data.get(name)
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or not low <= value <= high:
            raise ValueError('Invalid ' + name)
    uptime = data.get('uptime_s')
    if isinstance(uptime, bool) or not isinstance(uptime, int) or not 0 <= uptime <= 4294967295:
        raise ValueError('Invalid uptime_s')
    return data['temperature_c'], data['humidity_pct'], uptime

def store_reading(data):
    reading = validate(data)
    with connect() as db:
        latest = db.execute('SELECT timestamp FROM readings ORDER BY id DESC LIMIT 1').fetchone()
        now = time.time()
        if latest and now - latest[0] < 1:
            return None
        db.execute('INSERT INTO readings(timestamp, temperature_c, humidity_pct, uptime_s) VALUES(?,?,?,?)', (now, *reading))
        db.execute('DELETE FROM readings WHERE id NOT IN (SELECT id FROM readings ORDER BY id DESC LIMIT 720)')
    return now

class Handler(BaseHTTPRequestHandler):
    server_version = 'IoTLive'
    sys_version = ''

    def log_message(self, fmt, *args):
        print(self.command, urlsplit(self.path).path, flush=True)

    def respond(self, status, value, mime='application/json'):
        body = json.dumps(value, allow_nan=False).encode() if mime == 'application/json' else value
        self.send_response(status)
        self.send_header('Content-Type', mime)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('Referrer-Policy', 'no-referrer')
        self.send_header('Content-Security-Policy', "default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = urlsplit(self.path).path
        if path in ('/health', PREFIX + 'health'):
            with connect() as db:
                db.execute('SELECT 1').fetchone()
            return self.respond(200, {'status': 'ok'})
        if path == PREFIX.rstrip('/'):
            self.send_response(308)
            self.send_header('Location', PREFIX)
            self.send_header('Content-Length', '0')
            self.end_headers()
            return
        if path == PREFIX + 'api/readings':
            with connect() as db:
                rows = db.execute('SELECT timestamp, temperature_c, humidity_pct, uptime_s FROM readings ORDER BY id DESC LIMIT 720').fetchall()
            fields = ('timestamp', 'temperature_c', 'humidity_pct', 'uptime_s')
            return self.respond(200, {'readings': [dict(zip(fields, row)) for row in rows], 'server_time': time.time()})
        files = {PREFIX: ('index.html', 'text/html; charset=utf-8'), PREFIX + 'app.js': ('app.js', 'text/javascript; charset=utf-8'), PREFIX + 'style.css': ('style.css', 'text/css; charset=utf-8')}
        if path in files:
            filename, mime = files[path]
            return self.respond(200, (STATIC / filename).read_bytes(), mime)
        self.respond(404, {'error': 'Not found'})

    def do_POST(self):
        if urlsplit(self.path).path != PREFIX + 'api/readings':
            return self.respond(404, {'error': 'Not found'})
        expected = 'Bearer ' + KEY_FILE.read_text().strip()
        if not hmac.compare_digest(self.headers.get('Authorization', '').encode(), expected.encode()):
            return self.respond(401, {'error': 'Invalid device key'})
        if self.headers.get('Content-Type', '').split(';')[0] != 'application/json':
            return self.respond(415, {'error': 'Use application/json'})
        try:
            size = int(self.headers.get('Content-Length', '0'))
            if not 0 < size <= 1024:
                return self.respond(413, {'error': 'Body must be 1–1024 bytes'})
            data = json.loads(self.rfile.read(size))
            now = store_reading(data)
        except (ValueError, UnicodeDecodeError):
            return self.respond(400, {'error': 'Invalid reading'})
        if now is None:
            return self.respond(429, {'error': 'Send at most once per second'})
        self.respond(201, {'status': 'saved', 'timestamp': now})

if __name__ == '__main__':
    initialize()
    if os.environ.get('IOT_MQTT_CONFIG'):
        from mqtt_ingest import start
        mqtt_client = start(os.environ['IOT_MQTT_CONFIG'], store_reading)
    server = ThreadingHTTPServer(('127.0.0.1', int(os.environ.get('IOT_PORT', '5048'))), Handler)
    server.timeout = 15
    server.serve_forever()
