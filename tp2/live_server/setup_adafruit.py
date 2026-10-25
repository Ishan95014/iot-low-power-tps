"""Create only the requested two feeds and group dashboard; never print keys."""
import json
import ssl
import urllib.error
import urllib.request
from pathlib import Path

credentials = json.loads(Path(__file__).with_name('adafruit-secrets.json').read_text())
username = credentials['username']
base = 'https://io.adafruit.com/api/v2/' + username
context = ssl.create_default_context()
context.load_verify_locations(cafile=str(Path(__file__).with_name('adafruit-ca.pem')))

def request(path, body=None):
    req = urllib.request.Request(base + path,
        data=None if body is None else json.dumps(body).encode(),
        headers={'X-AIO-Key': credentials['key'], 'Content-Type': 'application/json', 'User-Agent': 'IoT-Group4-Setup/1.0'})
    try:
        with urllib.request.urlopen(req, timeout=20, context=context) as response:
            return json.load(response)
    except urllib.error.HTTPError as error:
        raise SystemExit('Adafruit setup request failed: HTTP %d on %s' % (error.code, path)) from None

existing = {feed['key']: feed for feed in request('/feeds')}
feeds = {}
for name in ('temperature', 'humidity'):
    feeds[name] = existing.get(name) or request('/feeds', {'feed': {'name': name, 'description': 'Groupe 4 - Lison-Ishan, ESP32 BME680 via MQTT'}})
    print('Feed ready:', name)

dashboard_key = 'groupe-4-lison-ishan'
dashboards = request('/dashboards')
dashboard = next((item for item in dashboards if item['key'] == dashboard_key), None)
created = dashboard is None
if created:
    dashboard = request('/dashboards', {'dashboard': {'name': 'Groupe 4 - Lison-Ishan', 'key': dashboard_key, 'description': 'TP2 - BME680 temperature and humidity over MQTT', 'visibility': 'private', 'color_mode': 'light'}})
blocks = request('/dashboards/' + dashboard_key + '/blocks')
positions = []
for index, (name, label, high) in enumerate([('temperature', 'Temperature (°C)', 50), ('humidity', 'Humidity (%)', 100)]):
    block_name = 'Group 4 ' + name
    block = next((item for item in blocks if item.get('name') == block_name), None)
    if block is None:
        block = request('/dashboards/' + dashboard_key + '/blocks', {'block': {'name': block_name, 'visual_type': 'gauge', 'block_feeds': [{'feed_id': feeds[name]['id']}], 'properties': {'minValue': '0', 'maxValue': str(high), 'ringWidth': '20', 'label': label, 'decimalPlaces': '2', 'showIcon': False}, 'column': index * 4, 'row': 0, 'size_x': 4, 'size_y': 4}})
    positions.append({'i': str(block['id']), 'x': index * 4, 'y': 0, 'w': 4, 'h': 4})
if created:
    mobile = [{**item, 'x': 0, 'y': index * 4} for index, item in enumerate(positions)]
    request('/dashboards/' + dashboard_key + '/update_layouts', {'layouts': {'xl': positions, 'lg': positions, 'md': positions, 'sm': mobile, 'xs': mobile}})
print('Dashboard ready: https://io.adafruit.com/' + username + '/dashboards/' + dashboard_key)
