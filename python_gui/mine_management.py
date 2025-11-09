#!/usr/bin/env python3
"""
Mine Management GUI

Supervisory control interface for monitoring and managing all trucks:
- Real-time map showing all truck positions
- Display truck states, telemetry, and faults
- Set waypoint targets for each truck
- Send commands to trucks (mode changes)
- MQTT subscriber for truck data

Real-Time Automation Concepts:
- Supervisory control and data acquisition (SCADA)
- MQTT pub/sub for distributed systems
- Real-time monitoring and control
"""

import tkinter as tk
from tkinter import ttk
import json
import math
from datetime import datetime
from collections import deque

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("Warning: paho-mqtt not installed. Install with: pip install paho-mqtt")
    mqtt = None

# MQTT Configuration
MQTT_BROKER = "localhost"
MQTT_PORT = 1883
MQTT_TOPIC_SENSORS = "truck/+/sensors"
MQTT_TOPIC_STATE = "truck/+/state"
MQTT_TOPIC_COMMANDS = "truck/{}/commands"
MQTT_TOPIC_SETPOINT = "truck/{}/setpoint"

# Map dimensions (matching simulation)
MAP_WIDTH = 1000
MAP_HEIGHT = 700

class TruckData:
    """Stores data for a single truck"""

    def __init__(self, truck_id):
        self.id = truck_id
        self.position_x = 0
        self.position_y = 0
        self.angle = 0
        self.temperature = 0
        self.fault_electrical = False
        self.fault_hydraulic = False
        self.mode = "UNKNOWN"
        self.fault_state = False
        self.last_update = None
        self.history = deque(maxlen=50)  # Position history for trail

    def update_sensors(self, data):
        """Update from sensor data"""
        self.position_x = data.get('position_x', 0)
        self.position_y = data.get('position_y', 0)
        self.angle = data.get('angle_x', 0)
        self.temperature = data.get('temperature', 0)
        self.fault_electrical = data.get('fault_electrical', False)
        self.fault_hydraulic = data.get('fault_hydraulic', False)
        self.last_update = datetime.now()

        # Add to history
        self.history.append((self.position_x, self.position_y))

    def update_state(self, data):
        """Update from state data"""
        self.mode = "AUTO" if data.get('automatic', False) else "MANUAL"
        self.fault_state = data.get('fault', False)
        self.last_update = datetime.now()

    def has_any_fault(self):
        """Check if truck has any fault"""
        return self.fault_state or self.fault_electrical or self.fault_hydraulic or self.temperature > 120

class MineManagementGUI:
    """Main management GUI application"""

    def __init__(self, root):
        self.root = root
        self.root.title("Mine Management - Stage 2")
        self.root.geometry("1400x800")

        # Data
        self.trucks = {}  # truck_id -> TruckData
        self.selected_truck = None
        self.target_waypoint = None

        # MQTT
        self.mqtt_client = None
        self.mqtt_connected = False

        # Setup GUI
        self.setup_gui()

        # Setup MQTT
        if mqtt:
            self.setup_mqtt()

        # Start update loop
        self.update_gui()

    def setup_gui(self):
        """Initialize GUI components"""
        # Main layout: left panel (map), right panel (controls)
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # Left panel: Map
        left_frame = ttk.LabelFrame(main_frame, text="Mine Map", padding=10)
        left_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S), padx=5)

        self.canvas = tk.Canvas(left_frame, width=MAP_WIDTH//1.5, height=MAP_HEIGHT//1.5,
                               bg='#2C2C2C', highlightthickness=1, highlightbackground='gray')
        self.canvas.pack()
        self.canvas.bind('<Button-1>', self.on_map_click)

        # Draw grid
        self.draw_grid()

        # Right panel: Controls and Info
        right_frame = ttk.Frame(main_frame)
        right_frame.grid(row=0, column=1, sticky=(tk.W, tk.E, tk.N, tk.S), padx=5)

        # Status panel
        status_frame = ttk.LabelFrame(right_frame, text="System Status", padding=10)
        status_frame.pack(fill=tk.X, pady=5)

        self.status_label = ttk.Label(status_frame, text="MQTT: Disconnected", foreground="red")
        self.status_label.pack()

        self.truck_count_label = ttk.Label(status_frame, text="Trucks: 0")
        self.truck_count_label.pack()

        # Truck selection
        select_frame = ttk.LabelFrame(right_frame, text="Truck Selection", padding=10)
        select_frame.pack(fill=tk.X, pady=5)

        self.truck_combo = ttk.Combobox(select_frame, state='readonly', width=20)
        self.truck_combo.pack()
        self.truck_combo.bind('<<ComboboxSelected>>', self.on_truck_selected)

        # Truck info panel
        info_frame = ttk.LabelFrame(right_frame, text="Truck Information", padding=10)
        info_frame.pack(fill=tk.BOTH, expand=True, pady=5)

        self.info_text = tk.Text(info_frame, height=15, width=35, state='disabled')
        self.info_text.pack(fill=tk.BOTH, expand=True)

        # Control panel
        control_frame = ttk.LabelFrame(right_frame, text="Truck Control", padding=10)
        control_frame.pack(fill=tk.X, pady=5)

        # Waypoint controls
        waypoint_frame = ttk.Frame(control_frame)
        waypoint_frame.pack(fill=tk.X, pady=5)

        ttk.Label(waypoint_frame, text="Set Waypoint:").pack()
        ttk.Label(waypoint_frame, text="(Click on map or enter coordinates)").pack()

        coord_frame = ttk.Frame(control_frame)
        coord_frame.pack(fill=tk.X, pady=5)

        ttk.Label(coord_frame, text="X:").grid(row=0, column=0)
        self.waypoint_x = ttk.Entry(coord_frame, width=8)
        self.waypoint_x.grid(row=0, column=1, padx=5)

        ttk.Label(coord_frame, text="Y:").grid(row=0, column=2)
        self.waypoint_y = ttk.Entry(coord_frame, width=8)
        self.waypoint_y.grid(row=0, column=3, padx=5)

        ttk.Button(control_frame, text="Send Waypoint", command=self.send_waypoint).pack(pady=5)

        # Mode control
        mode_frame = ttk.Frame(control_frame)
        mode_frame.pack(fill=tk.X, pady=5)

        ttk.Button(mode_frame, text="Manual Mode", command=lambda: self.send_mode_command(False)).pack(side=tk.LEFT, padx=2)
        ttk.Button(mode_frame, text="Auto Mode", command=lambda: self.send_mode_command(True)).pack(side=tk.LEFT, padx=2)
        ttk.Button(mode_frame, text="Rearm Fault", command=self.send_rearm_command).pack(side=tk.LEFT, padx=2)

        # Configure grid weights
        main_frame.columnconfigure(0, weight=2)
        main_frame.columnconfigure(1, weight=1)
        main_frame.rowconfigure(0, weight=1)

    def draw_grid(self):
        """Draw grid on map canvas"""
        scale = 1.5
        grid_size = 100 / scale

        for x in range(0, int(MAP_WIDTH//scale), int(grid_size)):
            self.canvas.create_line(x, 0, x, MAP_HEIGHT//scale, fill='#404040', dash=(2, 4))

        for y in range(0, int(MAP_HEIGHT//scale), int(grid_size)):
            self.canvas.create_line(0, y, MAP_WIDTH//scale, y, fill='#404040', dash=(2, 4))

    def setup_mqtt(self):
        """Initialize MQTT connection"""
        try:
            self.mqtt_client = mqtt.Client(client_id="mine_management")
            self.mqtt_client.on_connect = self.on_mqtt_connect
            self.mqtt_client.on_message = self.on_mqtt_message

            self.mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
            self.mqtt_client.loop_start()
            print(f"[MQTT] Connecting to broker at {MQTT_BROKER}:{MQTT_PORT}")
        except Exception as e:
            print(f"[MQTT] Connection failed: {e}")
            self.status_label.config(text=f"MQTT: Error - {e}", foreground="red")

    def on_mqtt_connect(self, client, userdata, flags, rc):
        """MQTT connection callback"""
        if rc == 0:
            self.mqtt_connected = True
            print("[MQTT] Connected successfully")
            self.status_label.config(text="MQTT: Connected", foreground="green")

            # Subscribe to all truck topics
            client.subscribe(MQTT_TOPIC_SENSORS)
            client.subscribe(MQTT_TOPIC_STATE)
            print("[MQTT] Subscribed to truck topics")
        else:
            print(f"[MQTT] Connection failed with code {rc}")
            self.status_label.config(text=f"MQTT: Failed (code {rc})", foreground="red")

    def on_mqtt_message(self, client, userdata, msg):
        """MQTT message callback"""
        try:
            topic_parts = msg.topic.split('/')
            truck_id = int(topic_parts[1])

            # Ensure truck exists
            if truck_id not in self.trucks:
                self.trucks[truck_id] = TruckData(truck_id)
                self.update_truck_list()

            # Parse data
            data = json.loads(msg.payload.decode())

            # Update appropriate data
            if 'sensors' in msg.topic:
                self.trucks[truck_id].update_sensors(data)
            elif 'state' in msg.topic:
                self.trucks[truck_id].update_state(data)

        except Exception as e:
            print(f"[MQTT] Error processing message: {e}")

    def update_truck_list(self):
        """Update truck selection combobox"""
        truck_ids = [f"Truck {tid}" for tid in sorted(self.trucks.keys())]
        self.truck_combo['values'] = truck_ids

        if truck_ids and not self.truck_combo.get():
            self.truck_combo.current(0)
            self.on_truck_selected(None)

        self.truck_count_label.config(text=f"Trucks: {len(self.trucks)}")

    def on_truck_selected(self, event):
        """Handle truck selection"""
        selection = self.truck_combo.get()
        if selection:
            truck_id = int(selection.split()[1])
            self.selected_truck = truck_id

    def on_map_click(self, event):
        """Handle map click for waypoint setting"""
        # Convert canvas coordinates to map coordinates
        scale = 1.5
        map_x = int(event.x * scale)
        map_y = int(event.y * scale)

        self.waypoint_x.delete(0, tk.END)
        self.waypoint_x.insert(0, str(map_x))
        self.waypoint_y.delete(0, tk.END)
        self.waypoint_y.insert(0, str(map_y))

        self.target_waypoint = (map_x, map_y)

    def send_waypoint(self):
        """Send waypoint to selected truck"""
        if not self.selected_truck or not self.mqtt_connected:
            return

        try:
            x = int(self.waypoint_x.get())
            y = int(self.waypoint_y.get())

            setpoint_data = {
                "target_x": x,
                "target_y": y,
                "target_speed": 50
            }

            topic = MQTT_TOPIC_SETPOINT.format(self.selected_truck)
            payload = json.dumps(setpoint_data)
            self.mqtt_client.publish(topic, payload)

            self.target_waypoint = (x, y)
            print(f"[Management] Sent waypoint ({x}, {y}) to Truck {self.selected_truck}")

        except ValueError:
            print("[Management] Invalid waypoint coordinates")

    def send_mode_command(self, automatic):
        """Send mode change command"""
        if not self.selected_truck or not self.mqtt_connected:
            return

        command_data = {
            "auto_mode": automatic,
            "manual_mode": not automatic
        }

        topic = MQTT_TOPIC_COMMANDS.format(self.selected_truck)
        payload = json.dumps(command_data)
        self.mqtt_client.publish(topic, payload)

        mode_str = "AUTOMATIC" if automatic else "MANUAL"
        print(f"[Management] Sent {mode_str} mode command to Truck {self.selected_truck}")

    def send_rearm_command(self):
        """Send fault rearm command"""
        if not self.selected_truck or not self.mqtt_connected:
            return

        command_data = {
            "rearm": True
        }

        topic = MQTT_TOPIC_COMMANDS.format(self.selected_truck)
        payload = json.dumps(command_data)
        self.mqtt_client.publish(topic, payload)

        print(f"[Management] Sent REARM command to Truck {self.selected_truck}")

    def draw_trucks(self):
        """Draw all trucks on map"""
        self.canvas.delete('truck')
        self.canvas.delete('trail')
        self.canvas.delete('waypoint')

        scale = 1.5

        # Draw waypoint if set
        if self.target_waypoint:
            wx, wy = self.target_waypoint
            cx = wx / scale
            cy = wy / scale
            self.canvas.create_oval(cx-8, cy-8, cx+8, cy+8,
                                   fill='yellow', outline='orange', width=2, tags='waypoint')
            self.canvas.create_text(cx, cy-15, text="TARGET", fill='yellow',
                                   font=('Arial', 10, 'bold'), tags='waypoint')

        # Draw trucks
        for truck in self.trucks.values():
            x = truck.position_x / scale
            y = truck.position_y / scale

            # Draw trail
            if len(truck.history) > 1:
                trail_points = [(px/scale, py/scale) for px, py in truck.history]
                for i in range(len(trail_points) - 1):
                    self.canvas.create_line(trail_points[i][0], trail_points[i][1],
                                           trail_points[i+1][0], trail_points[i+1][1],
                                           fill='cyan', width=1, tags='trail')

            # Determine color
            if truck.has_any_fault():
                color = 'red'
            elif truck.mode == "AUTO":
                color = 'green'
            else:
                color = 'blue'

            # Draw truck
            size = 15
            self.canvas.create_rectangle(x-size, y-size, x+size, y+size,
                                         fill=color, outline='white', width=2, tags='truck')

            # Draw direction indicator
            rad = math.radians(truck.angle)
            dir_len = 20
            end_x = x + dir_len * math.cos(rad)
            end_y = y + dir_len * math.sin(rad)
            self.canvas.create_line(x, y, end_x, end_y, fill='yellow', width=2, tags='truck')

            # Draw label
            self.canvas.create_text(x, y-size-10, text=f"T{truck.id}",
                                   fill='white', font=('Arial', 10, 'bold'), tags='truck')

    def update_info_panel(self):
        """Update truck information display"""
        self.info_text.config(state='normal')
        self.info_text.delete('1.0', tk.END)

        if self.selected_truck and self.selected_truck in self.trucks:
            truck = self.trucks[self.selected_truck]

            info = f"Truck {truck.id} Information\n"
            info += "=" * 30 + "\n\n"
            info += f"Position: ({truck.position_x}, {truck.position_y})\n"
            info += f"Heading: {truck.angle}°\n"
            info += f"Temperature: {truck.temperature}°C\n"
            info += f"\nMode: {truck.mode}\n"
            info += f"Fault State: {'FAULT' if truck.fault_state else 'OK'}\n"
            info += f"\nElectrical: {'FAULT' if truck.fault_electrical else 'OK'}\n"
            info += f"Hydraulic: {'FAULT' if truck.fault_hydraulic else 'OK'}\n"

            if truck.temperature > 120:
                info += f"\nWARNING: CRITICAL TEMPERATURE!\n"
            elif truck.temperature > 95:
                info += f"\nWARNING: High temperature\n"

            if truck.last_update:
                age = (datetime.now() - truck.last_update).total_seconds()
                info += f"\nLast update: {age:.1f}s ago\n"

            self.info_text.insert('1.0', info)

        self.info_text.config(state='disabled')

    def update_gui(self):
        """Periodic GUI update"""
        self.draw_trucks()
        self.update_info_panel()

        # Schedule next update
        self.root.after(100, self.update_gui)

    def on_closing(self):
        """Handle window close"""
        if self.mqtt_client:
            self.mqtt_client.loop_stop()
            self.mqtt_client.disconnect()
        self.root.destroy()

def main():
    root = tk.Tk()
    app = MineManagementGUI(root)
    root.protocol("WM_DELETE_WINDOW", app.on_closing)

    print("=" * 50)
    print("Mine Management GUI Started")
    print("=" * 50)
    print("Waiting for truck data from MQTT...")
    print("=" * 50)

    root.mainloop()

if __name__ == "__main__":
    main()
