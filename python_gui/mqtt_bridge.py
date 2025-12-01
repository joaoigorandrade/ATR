#!/usr/bin/env python3

import os
import json
import time
import glob
from pathlib import Path

class Colors:
    RESET = '\033[0m'
    BOLD = '\033[1m'
    DIM = '\033[2m'

    BLACK = '\033[30m'
    RED = '\033[31m'
    GREEN = '\033[32m'
    YELLOW = '\033[33m'
    BLUE = '\033[34m'
    MAGENTA = '\033[35m'
    CYAN = '\033[36m'
    WHITE = '\033[37m'

    BG_BLACK = '\033[40m'
    BG_RED = '\033[41m'
    BG_GREEN = '\033[42m'
    BG_YELLOW = '\033[43m'
    BG_BLUE = '\033[44m'
    BG_MAGENTA = '\033[45m'
    BG_CYAN = '\033[46m'
    BG_WHITE = '\033[47m'

    BRIGHT_BLACK = '\033[90m'
    BRIGHT_RED = '\033[91m'
    BRIGHT_GREEN = '\033[92m'
    BRIGHT_YELLOW = '\033[93m'
    BRIGHT_BLUE = '\033[94m'
    BRIGHT_MAGENTA = '\033[95m'
    BRIGHT_CYAN = '\033[96m'
    BRIGHT_WHITE = '\033[97m'

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print(f"{Colors.BRIGHT_RED}✖ Error: paho-mqtt not installed{Colors.RESET}")
    print(f"{Colors.BRIGHT_YELLOW}  Install with: pip3 install paho-mqtt{Colors.RESET}")
    exit(1)

BRIDGE_DIR = "bridge"
TO_MQTT_DIR = os.path.join(BRIDGE_DIR, "to_mqtt")
FROM_MQTT_DIR = os.path.join(BRIDGE_DIR, "from_mqtt")

MQTT_BROKER = "localhost"
MQTT_PORT = 1883

TOPIC_SENSORS = "truck/+/sensors"
TOPIC_STATE = "truck/+/state"
TOPIC_COMMANDS = "truck/+/commands"
TOPIC_SETPOINT = "truck/+/setpoint"

class MQTTBridge:
    """MQTT Bridge for file-based C++/MQTT communication"""

    def __init__(self):
        Path(TO_MQTT_DIR).mkdir(parents=True, exist_ok=True)
        Path(FROM_MQTT_DIR).mkdir(parents=True, exist_ok=True)

        self.clean_directories()
        
        self.truck_positions = {}

        self.mqtt_client = mqtt.Client(client_id="mqtt_bridge")
        self.mqtt_client.on_connect = self.on_connect
        self.mqtt_client.on_message = self.on_message

        try:
            self.mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
            self.mqtt_client.loop_start()
            print(f"{Colors.BRIGHT_GREEN}✓ Connected to MQTT broker at {Colors.BRIGHT_CYAN}{MQTT_BROKER}:{MQTT_PORT}{Colors.RESET}")
        except Exception as e:
            print(f"{Colors.BRIGHT_RED}✖ Failed to connect to MQTT broker: {Colors.BRIGHT_YELLOW}{e}{Colors.RESET}")
            print(f"{Colors.BRIGHT_YELLOW}  Make sure mosquitto is running: {Colors.BRIGHT_WHITE}mosquitto{Colors.RESET}")
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
        if rc == 0:
            print(f"{Colors.BRIGHT_GREEN}✓ MQTT connected successfully{Colors.RESET}")
            client.subscribe(TOPIC_SENSORS)
            client.subscribe(TOPIC_COMMANDS)
            client.subscribe(TOPIC_SETPOINT)
            print(f"{Colors.BRIGHT_BLUE}→ Subscribed to topics:{Colors.RESET}")
            print(f"  {Colors.BRIGHT_CYAN}• {TOPIC_SENSORS}{Colors.RESET}")
            print(f"  {Colors.BRIGHT_CYAN}• {TOPIC_COMMANDS}{Colors.RESET}")
            print(f"  {Colors.BRIGHT_CYAN}• {TOPIC_SETPOINT}{Colors.RESET}")
        else:
            print(f"{Colors.BRIGHT_RED}✖ MQTT connection failed with code {rc}{Colors.RESET}")

    def on_message(self, client, userdata, msg):
        try:
            data = json.loads(msg.payload.decode())

            timestamp = int(time.time() * 1000)
            topic_name = msg.topic.replace('/', '_')
            filename = f"{timestamp}_{topic_name}.json"
            filepath = os.path.join(FROM_MQTT_DIR, filename)

            message_data = {
                "topic": msg.topic,
                "payload": data,
                "timestamp": timestamp
            }

            with open(filepath, 'w') as f:
                json.dump(message_data, f)

            if 'commands' in msg.topic and ('auto_mode' in data or 'manual_mode' in data):
                mode = "AUTO" if data.get('auto_mode') else "MANUAL"
                mode_color = Colors.BRIGHT_GREEN if data.get('auto_mode') else Colors.BRIGHT_CYAN
                print(f"{Colors.BRIGHT_MAGENTA}← Mode command:{Colors.RESET} {mode_color}{mode}{Colors.RESET}")
            elif 'setpoint' in msg.topic:
                x = data.get('target_x')
                y = data.get('target_y')
                print(f"{Colors.BRIGHT_YELLOW}← Setpoint:{Colors.RESET} {Colors.BRIGHT_WHITE}({x}, {y}){Colors.RESET}")
            
            if 'sensors' in msg.topic:
                try:
                    parts = msg.topic.split('/')
                    if len(parts) >= 2:
                        truck_id = int(parts[1])
                        self.truck_positions[truck_id] = {
                            'x': data.get('position_x', 0),
                            'y': data.get('position_y', 0),
                            'id': truck_id
                        }
                        self.generate_obstacle_files(truck_id)
                except ValueError:
                    pass

        except Exception as e:
            print(f"{Colors.BRIGHT_RED}✖ Error processing MQTT message: {Colors.YELLOW}{e}{Colors.RESET}")

    def generate_obstacle_files(self, source_truck_id):
        timestamp = int(time.time() * 1000)
        
        for dest_truck_id in self.truck_positions.keys():
            if dest_truck_id == source_truck_id:
                continue
            obstacles = []
            for other_id, pos in self.truck_positions.items():
                if other_id != dest_truck_id:
                    obstacles.append(pos)
            
            if not obstacles:
                continue
                
            filename = f"{timestamp}_truck_{dest_truck_id}_obstacles.json"
            filepath = os.path.join(FROM_MQTT_DIR, filename)
            
            data = {
                "topic": f"truck/{dest_truck_id}/obstacles",
                "payload": {
                    "obstacles": obstacles
                },
                "timestamp": timestamp
            }
            
            try:
                with open(filepath, 'w') as f:
                    json.dump(data, f)
            except Exception as e:
                print(f"Error writing obstacle file: {e}")

    def check_outgoing_messages(self):
        try:
            json_files = []
            try:
                with os.scandir(TO_MQTT_DIR) as entries:
                    for entry in entries:
                        if entry.name.endswith('.json') and entry.is_file():
                            json_files.append(entry.path)
            except FileNotFoundError:
                return

            for filepath in json_files:
                try:
                    # Read message
                    with open(filepath, 'r') as f:
                        message = json.load(f)

                    topic = message.get('topic', TOPIC_SENSORS)
                    payload = message.get('payload', {})

                    # Publish via MQTT
                    self.mqtt_client.publish(topic, json.dumps(payload))

                    # Delete processed file
                    os.remove(filepath)

                except Exception as e:
                    print(f"{Colors.BRIGHT_RED}✖ Error processing file {Colors.BRIGHT_YELLOW}{filepath}{Colors.RESET}: {e}")
                    try:
                        os.remove(filepath)
                    except:
                        pass

        except Exception as e:
            print(f"{Colors.BRIGHT_RED}✖ Error checking outgoing messages: {Colors.YELLOW}{e}{Colors.RESET}")

    def run(self):
        print()
        print(f"{Colors.BRIGHT_CYAN}╔{'═' * 58}╗{Colors.RESET}")
        print(f"{Colors.BRIGHT_CYAN}║{Colors.RESET} {Colors.BOLD}{Colors.BRIGHT_WHITE}MQTT Bridge Running{Colors.RESET}{' ' * 38}{Colors.BRIGHT_CYAN}║{Colors.RESET}")
        print(f"{Colors.BRIGHT_CYAN}╠{'═' * 58}╣{Colors.RESET}")
        print(f"{Colors.BRIGHT_CYAN}║{Colors.RESET} {Colors.BRIGHT_BLUE}→{Colors.RESET} Watching: {Colors.BRIGHT_WHITE}{TO_MQTT_DIR}/{Colors.RESET}{' ' * (35 - len(TO_MQTT_DIR))}{Colors.BRIGHT_CYAN}║{Colors.RESET}")
        print(f"{Colors.BRIGHT_CYAN}║{Colors.RESET} {Colors.BRIGHT_BLUE}←{Colors.RESET} Writing:  {Colors.BRIGHT_WHITE}{FROM_MQTT_DIR}/{Colors.RESET}{' ' * (35 - len(FROM_MQTT_DIR))}{Colors.BRIGHT_CYAN}║{Colors.RESET}")
        print(f"{Colors.BRIGHT_CYAN}║{Colors.RESET}{' ' * 58}{Colors.BRIGHT_CYAN}║{Colors.RESET}")
        print(f"{Colors.BRIGHT_CYAN}║{Colors.RESET} {Colors.DIM}Press Ctrl+C to stop{Colors.RESET}{' ' * 36}{Colors.BRIGHT_CYAN}║{Colors.RESET}")
        print(f"{Colors.BRIGHT_CYAN}╚{'═' * 58}╝{Colors.RESET}")
        print()

        try:
            while self.running:
                self.check_outgoing_messages()
                time.sleep(0.01)

        except KeyboardInterrupt:
            print(f"\n{Colors.BRIGHT_YELLOW}⏸ Shutting down...{Colors.RESET}")

        finally:
            self.mqtt_client.loop_stop()
            self.mqtt_client.disconnect()
            print(f"{Colors.BRIGHT_GREEN}✓ Bridge stopped{Colors.RESET}\n")

if __name__ == "__main__":
    bridge = MQTTBridge()
    bridge.run()
