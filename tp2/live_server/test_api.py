import importlib.util
import json
import os
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from pathlib import Path

class API(unittest.TestCase):
    def test_ingestion_and_rejections(self):
        with tempfile.TemporaryDirectory() as tmp:
            key = Path(tmp) / 'key'
            key.write_text('test-device-key')
            os.environ['IOT_DATABASE'] = tmp + '/data.sqlite3'
            os.environ['IOT_KEY_FILE'] = str(key)
            spec = importlib.util.spec_from_file_location('iotserver', Path(__file__).with_name('server.py'))
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            module.initialize()
            server = module.ThreadingHTTPServer(('127.0.0.1', 0), module.Handler)
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            url = 'http://127.0.0.1:%d/apps/iot-live/api/readings' % server.server_port
            def post(data, token='test-device-key'):
                request = urllib.request.Request(url, json.dumps(data).encode(), {'Content-Type': 'application/json', 'Authorization': 'Bearer ' + token})
                try:
                    with urllib.request.urlopen(request) as response:
                        return response.status
                except urllib.error.HTTPError as error:
                    return error.code
            try:
                sample = {'temperature_c': 23.5, 'humidity_pct': 45, 'uptime_s': 12}
                self.assertEqual(post(sample, 'wrong'), 401)
                self.assertEqual(post({**sample, 'humidity_pct': 101}), 400)
                self.assertEqual(post({**sample, 'temperature_c': float('nan')}), 400)
                self.assertEqual(post(sample), 201)
                self.assertEqual(post(sample), 429)
                with urllib.request.urlopen(url) as response:
                    rows = json.load(response)['readings']
                self.assertEqual(len(rows), 1)
                self.assertEqual(rows[0]['temperature_c'], 23.5)
            finally:
                server.shutdown()
                server.server_close()
                thread.join()

if __name__ == '__main__':
    unittest.main()
