#!/usr/bin/env python3
"""
MQTT Bridge for C++ Truck Control

This bridge allows the C++ truck control system to communicate via MQTT
without requiring C++ MQTT libraries. It uses file-based communication:

C++ writes to: bridge/to_mqtt/*.json
Bridge reads, publishes via MQTT

MQTT receives messages
Bridge writes to: bridge/from_mqtt/*.json
C++ reads and processes

This is a simplified approach for educational purposes.
In production, use native C++ MQTT libraries.
"""

import os
import json
import time
import glob
from pathlib import Path

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("Error: paho-mqtt not installed")
    print("Install with: pip3 install paho-mqtt")
    exit(1)

# Directories for file-based communication
BRIDGE_DIR = "bridge"
TO_MQTT_DIR = os.path.join(BRIDGE_DIR, "to_mqtt")
FROM_MQTT_DIR = os.path.join(BRIDGE_DIR, "from_mqtt")

# MQTT Configuration
MQTT_BROKER = "localhost"
MQTT_PORT = 1883

# Topics
TOPIC_SENSORS = "truck/1/sensors"
TOPIC_STATE = "truck/1/state"
TOPIC_COMMANDS = "truck/1/commands"
TOPIC_SETPOINT = "truck/1/setpoint"

class MQTTBridge:
    """MQTT Bridge for file-based C++/MQTT communication"""

    def __init__(self):
        # Create bridge directories
        Path(TO_MQTT_DIR).mkdir(parents=True, exist_ok=True)
        Path(FROM_MQTT_DIR).mkdir(parents=True, exist_ok=True)

        # Clean old files
        self.clean_directories()

        # MQTT setup
        self.mqtt_client = mqtt.Client(client_id="mqtt_bridge")
        self.mqtt_client.on_connect = self.on_connect
        self.mqtt_client.on_message = self.on_message

        try:
            self.mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
            self.mqtt_client.loop_start()
            print(f"[Bridge] Connected to MQTT broker at {MQTT_BROKER}:{MQTT_PORT}")
        except Exception as e:
            print(f"[Bridge] Failed to connect to MQTT broker: {e}")
            print(f"[Bridge] Make sure mosquitto is running: mosquitto")
            exit(1)

        self.running = True

    def clean_directories(self):
        """Remove old message files"""
        for dir_path in [TO_MQTT_DIR, FROM_MQTT_DIR]:
            for file in glob.glob(os.path.join(dir_path, "*.json")):
                try:
                    os.remove(file)
                except:
                    pass

    def on_connect(self, client, userdata, flags, rc):
        """MQTT connection callback"""
        if rc == 0:
            print("[Bridge] MQTT connected successfully")
            # Subscribe to topics that C++ needs to receive
            client.subscribe(TOPIC_COMMANDS)
            client.subscribe(TOPIC_SETPOINT)
            print(f"[Bridge] Subscribed to: {TOPIC_COMMANDS}, {TOPIC_SETPOINT}")
        else:
            print(f"[Bridge] MQTT connection failed with code {rc}")

    def on_message(self, client, userdata, msg):
        """MQTT message callback - write to file for C++ to read"""
        try:
            # Parse message
            data = json.loads(msg.payload.decode())

            # Create filename with timestamp
            timestamp = int(time.time() * 1000)
            topic_name = msg.topic.replace('/', '_')
            filename = f"{timestamp}_{topic_name}.json"
            filepath = os.path.join(FROM_MQTT_DIR, filename)

            # Write to file for C++ to read
            message_data = {
                "topic": msg.topic,
                "payload": data,
                "timestamp": timestamp
            }

            with open(filepath, 'w') as f:
                json.dump(message_data, f)

            print(f"[Bridge] MQTT -> File: {filename}")

        except Exception as e:
            print(f"[Bridge] Error processing MQTT message: {e}")

    def check_outgoing_messages(self):
        """Check for messages from C++ to publish via MQTT"""
        try:
            # Look for JSON files written by C++
            json_files = glob.glob(os.path.join(TO_MQTT_DIR, "*.json"))

            for filepath in json_files:
                try:
                    # Read message
                    with open(filepath, 'r') as f:
                        message = json.load(f)

                    topic = message.get('topic', TOPIC_SENSORS)
                    payload = json.dumps(message.get('payload', {}))

                    # Publish via MQTT
                    self.mqtt_client.publish(topic, payload)

                    filename = os.path.basename(filepath)
                    print(f"[Bridge] File -> MQTT: {filename} on {topic}")

                    # Delete processed file
                    os.remove(filepath)

                except Exception as e:
                    print(f"[Bridge] Error processing file {filepath}: {e}")
                    # Remove corrupted file
                    try:
                        os.remove(filepath)
                    except:
                        pass

        except Exception as e:
            print(f"[Bridge] Error checking outgoing messages: {e}")

    def run(self):
        """Main bridge loop"""
        print("=" * 50)
        print("MQTT Bridge Running")
        print("=" * 50)
        print(f"Watching: {TO_MQTT_DIR}/ for outgoing messages")
        print(f"Writing:  {FROM_MQTT_DIR}/ for incoming messages")
        print("Press Ctrl+C to stop")
        print("=" * 50)

        try:
            while self.running:
                # Check for outgoing messages from C++
                self.check_outgoing_messages()

                # Sleep briefly
                time.sleep(0.1)

        except KeyboardInterrupt:
            print("\n[Bridge] Shutting down...")

        finally:
            self.mqtt_client.loop_stop()
            self.mqtt_client.disconnect()
            print("[Bridge] Stopped")

if __name__ == "__main__":
    bridge = MQTTBridge()
    bridge.run()
