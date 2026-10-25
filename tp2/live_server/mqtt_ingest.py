"""Subscribe to Mosquitto and store validated live sensor samples."""
import json
import logging
import threading
from pathlib import Path
import paho.mqtt.client as mqtt

def start(config_path, store):
    config = json.loads(Path(config_path).read_text())
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id='iot-live-dashboard')
    client.username_pw_set(config['username'], config['password'])
    client.reconnect_delay_set(1, 30)

    def connected(client, userdata, flags, reason, properties):
        if reason == 0:
            client.subscribe('iot/tp2/readings', qos=1)
            print('Mosquitto connected; subscribed to iot/tp2/readings', flush=True)
        else:
            print('Mosquitto connection rejected', flush=True)

    def received(client, userdata, message):
        # Do not ingest stale retained messages as newly received samples.
        if message.retain or len(message.payload) > 1024:
            return
        try:
            data = json.loads(message.payload)
            store(data)
            print('MQTT sample stored', flush=True)
        except (ValueError, UnicodeDecodeError):
            logging.warning('Rejected invalid MQTT sample')
        except Exception:
            logging.exception('MQTT sample storage failed')

    client.on_connect = connected
    client.on_message = received
    client.connect_async('127.0.0.1', 1883, 60)
    client.loop_start()
    return client
