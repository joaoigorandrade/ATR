#!/usr/bin/env python3

import tkinter as tk
from tkinter import ttk
import json
import math
import time
from datetime import datetime
from collections import deque

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("Warning: paho-mqtt not installed. Install with: pip install paho-mqtt")
    mqtt = None

MQTT_BROKER_HOST = "localhost"
MQTT_BROKER_PORT = 1883
MQTT_TOPIC_SENSORS = "truck/+/sensors"
MQTT_TOPIC_STATE = "truck/+/state"
MQTT_TOPIC_COMMANDS = "truck/{}/commands"
MQTT_TOPIC_SETPOINT = "truck/{}/setpoint"

MAP_WIDTH_PIXELS = 1000
MAP_HEIGHT_PIXELS = 700
MAP_DISPLAY_SCALE = 1.5
GRID_SPACING_PIXELS = 100

TEMPERATURE_WARNING_THRESHOLD = 95
TEMPERATURE_CRITICAL_THRESHOLD = 120

POSITION_HISTORY_SIZE = 20
GUI_UPDATE_PERIOD_MS = 50

TRUCK_DISPLAY_SIZE = 15
DIRECTION_INDICATOR_LENGTH = 20
WAYPOINT_DISPLAY_RADIUS = 8

DEFAULT_TARGET_SPEED = 50


class TruckData:
    def __init__(self, truck_id):
        self.id = truck_id
        self.position_x = 0
        self.position_y = 0
        self.angle = 0
        self.temperature = 0
        self.fault_electrical = False
        self.fault_hydraulic = False
        self.mode = "MANUAL"
        self.fault_state = False
        self.last_update = None
        self.position_history = deque(maxlen=POSITION_HISTORY_SIZE)

        self.acceleration = 0
        self.steering = 0
        self.arrived = False
        
        self.dirty = True

    def update_sensors(self, data):
        self.position_x = data.get('position_x', 0)
        self.position_y = data.get('position_y', 0)
        self.angle = data.get('angle_x', 0)
        self.temperature = data.get('temperature', 0)
        self.fault_electrical = data.get('fault_electrical', False)
        self.fault_hydraulic = data.get('fault_hydraulic', False)
        self.last_update = datetime.now()

        self.position_history.append((self.position_x, self.position_y))
        self.dirty = True

    def update_state(self, data):
        self.mode = "AUTO" if data.get('automatic', False) else "MANUAL"
        self.fault_state = data.get('fault', False)
        self.last_update = datetime.now()
        self.dirty = True

    def update_commands(self, data):
        if 'acceleration' in data:
            self.acceleration = data['acceleration']
        if 'steering' in data:
            self.steering = data['steering']
        if 'arrived' in data:
            self.arrived = data['arrived']
        self.last_update = datetime.now()
        self.dirty = True

    def has_any_fault(self):
        return (self.fault_state or
                self.fault_electrical or
                self.fault_hydraulic or
                self.temperature > TEMPERATURE_CRITICAL_THRESHOLD)

    def is_temperature_warning(self):
        return self.temperature > TEMPERATURE_WARNING_THRESHOLD

    def get_display_color(self):
        if self.has_any_fault():
            return 'red'
        if self.is_temperature_warning():
            return 'orange'
        if self.mode == "AUTO":
            return 'green'
        return 'blue'


class MineManagementGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Mine Management - Stage 2")
        self.root.geometry("1400x800")

        self.trucks = {}
        self.selected_truck = None
        self.target_waypoint = None

        self.last_info_text = ""
        self.waypoint_dirty = False
        self.frame_times = deque(maxlen=30)
        self.fps = 0.0

        self.mqtt_client = None
        self.mqtt_connected = False

        self.canvas_items = {}
        self.waypoint_item_ids = None

        self.last_key_time = 0

        self.setup_gui()

        if mqtt:
            self.setup_mqtt()

        self.update_gui()
        self.check_heartbeat()

    def setup_gui(self):
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        left_frame = ttk.LabelFrame(main_frame, text="Mine Map", padding=10)
        left_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S), padx=5)

        canvas_width = int(MAP_WIDTH_PIXELS / MAP_DISPLAY_SCALE)
        canvas_height = int(MAP_HEIGHT_PIXELS / MAP_DISPLAY_SCALE)
        self.canvas = tk.Canvas(left_frame, width=canvas_width, height=canvas_height,
                               bg='#2C2C2C', highlightthickness=1, highlightbackground='gray')
        self.canvas.pack()
        self.canvas.bind('<Button-1>', self.on_map_click)

        self.draw_grid()

        right_frame = ttk.Frame(main_frame)
        right_frame.grid(row=0, column=1, sticky=(tk.W, tk.E, tk.N, tk.S), padx=5)

        status_frame = ttk.LabelFrame(right_frame, text="System Status", padding=10)
        status_frame.pack(fill=tk.X, pady=5)

        self.status_label = ttk.Label(status_frame, text="MQTT: Disconnected", foreground="red")
        self.status_label.pack()

        self.truck_count_label = ttk.Label(status_frame, text="Trucks: 0")
        self.truck_count_label.pack()

        self.fps_label = ttk.Label(status_frame, text="FPS: 0.0")
        self.fps_label.pack()

        self.avg_update_age_label = ttk.Label(status_frame, text="Avg Update Age: 0.0s")
        self.avg_update_age_label.pack()

        select_frame = ttk.LabelFrame(right_frame, text="Truck Selection", padding=10)
        select_frame.pack(fill=tk.X, pady=5)

        self.truck_combo = ttk.Combobox(select_frame, state='readonly', width=20)
        self.truck_combo.pack()
        self.truck_combo.bind('<<ComboboxSelected>>', self.on_truck_selected)

        info_frame = ttk.LabelFrame(right_frame, text="Truck Information", padding=10)
        info_frame.pack(fill=tk.BOTH, expand=True, pady=5)

        self.info_text = tk.Text(info_frame, height=15, width=35, state='disabled')
        self.info_text.pack(fill=tk.BOTH, expand=True)

        control_frame = ttk.LabelFrame(right_frame, text="Truck Control", padding=10)
        control_frame.pack(fill=tk.X, pady=5)

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

        mode_frame = ttk.Frame(control_frame)
        mode_frame.pack(fill=tk.X, pady=5)

        ttk.Button(mode_frame, text="Manual Mode", command=lambda: self.send_mode_command(False)).pack(side=tk.LEFT, padx=2)
        ttk.Button(mode_frame, text="Auto Mode", command=lambda: self.send_mode_command(True)).pack(side=tk.LEFT, padx=2)
        ttk.Button(mode_frame, text="Rearm Fault", command=self.send_rearm_command).pack(side=tk.LEFT, padx=2)

        ttk.Label(control_frame, text="Manual Controls: WASD / Arrows / Space", font=('Arial', 9, 'italic')).pack(pady=5)

        main_frame.columnconfigure(0, weight=2)
        main_frame.columnconfigure(1, weight=1)
        main_frame.rowconfigure(0, weight=1)

        self.root.bind('<KeyPress>', self.on_key_press)
        self.target_speed = 0

    def draw_grid(self):
        grid_size = GRID_SPACING_PIXELS / MAP_DISPLAY_SCALE
        canvas_width = int(MAP_WIDTH_PIXELS / MAP_DISPLAY_SCALE)
        canvas_height = int(MAP_HEIGHT_PIXELS / MAP_DISPLAY_SCALE)

        for x in range(0, canvas_width, int(grid_size)):
            self.canvas.create_line(x, 0, x, canvas_height, fill='#404040', dash=(2, 4))

        for y in range(0, canvas_height, int(grid_size)):
            self.canvas.create_line(0, y, canvas_width, y, fill='#404040', dash=(2, 4))

    def setup_mqtt(self):
        try:
            self.mqtt_client = mqtt.Client(client_id="mine_management")
            self.mqtt_client.on_connect = self.on_mqtt_connect
            self.mqtt_client.on_message = self.on_mqtt_message

            self.mqtt_client.connect(MQTT_BROKER_HOST, MQTT_BROKER_PORT, 60)
            self.mqtt_client.loop_start()
            print(f"[MQTT] Connecting to broker at {MQTT_BROKER_HOST}:{MQTT_BROKER_PORT}")
        except Exception as e:
            print(f"[MQTT] Connection failed: {e}")
            self.status_label.config(text=f"MQTT: Error - {e}", foreground="red")

    def on_mqtt_connect(self, client, userdata, flags, rc):
        if rc == 0:
            self.mqtt_connected = True
            print("[MQTT] Connected successfully")
            self.status_label.config(text="MQTT: Connected", foreground="green")

            client.subscribe(MQTT_TOPIC_SENSORS)
            client.subscribe(MQTT_TOPIC_STATE)
            client.subscribe(MQTT_TOPIC_COMMANDS.format("+"))
            print("[MQTT] Subscribed to truck topics (sensors, state, commands)")
        else:
            print(f"[MQTT] Connection failed with code {rc}")
            self.status_label.config(text=f"MQTT: Failed (code {rc})", foreground="red")

    def on_mqtt_message(self, client, userdata, msg):
        try:
            topic_parts = msg.topic.split('/')
            truck_id = int(topic_parts[1])

            if truck_id not in self.trucks:
                self.trucks[truck_id] = TruckData(truck_id)
                self.root.after_idle(self.update_truck_list)

            data = json.loads(msg.payload.decode())

            if 'sensors' in msg.topic:
                self.trucks[truck_id].update_sensors(data)
            elif 'state' in msg.topic:
                self.trucks[truck_id].update_state(data)
            elif 'commands' in msg.topic:
                self.trucks[truck_id].update_commands(data)

        except Exception as e:
            print(f"[MQTT] Error processing message: {e}")

    def update_truck_list(self):
        truck_ids = [f"Truck {tid}" for tid in sorted(self.trucks.keys())]
        self.truck_combo['values'] = truck_ids

        if truck_ids and not self.truck_combo.get():
            self.truck_combo.current(0)
            self.on_truck_selected(None)

        self.truck_count_label.config(text=f"Trucks: {len(self.trucks)}")

    def on_truck_selected(self, event):
        selection = self.truck_combo.get()
        if selection:
            truck_id = int(selection.split()[1])
            self.selected_truck = truck_id
            self.last_info_text = ""
            
            if truck_id in self.trucks:
                self.target_speed = self.trucks[truck_id].acceleration
            else:
                self.target_speed = 0

    def on_map_click(self, event):
        map_x = int(event.x * MAP_DISPLAY_SCALE)
        map_y = int(event.y * MAP_DISPLAY_SCALE)

        self.waypoint_x.delete(0, tk.END)
        self.waypoint_x.insert(0, str(map_x))
        self.waypoint_y.delete(0, tk.END)
        self.waypoint_y.insert(0, str(map_y))

        self.target_waypoint = (map_x, map_y)
        self.waypoint_dirty = True

    def send_waypoint(self):
        if not self.selected_truck or not self.mqtt_connected:
            return

        try:
            x = int(self.waypoint_x.get())
            y = int(self.waypoint_y.get())

            setpoint_data = {
                "target_x": x,
                "target_y": y,
                "target_speed": DEFAULT_TARGET_SPEED
            }

            topic = MQTT_TOPIC_SETPOINT.format(self.selected_truck)
            payload = json.dumps(setpoint_data)
            self.mqtt_client.publish(topic, payload)

            self.target_waypoint = (x, y)
            self.waypoint_dirty = True
            print(f"[Management] Sent waypoint ({x}, {y}) to Truck {self.selected_truck}")

        except ValueError:
            print("[Management] Invalid waypoint coordinates")

    def send_mode_command(self, automatic):
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
        if not self.selected_truck or not self.mqtt_connected:
            return

        command_data = {"rearm": True}

        topic = MQTT_TOPIC_COMMANDS.format(self.selected_truck)
        payload = json.dumps(command_data)
        self.mqtt_client.publish(topic, payload)

        print(f"[Management] Sent REARM command to Truck {self.selected_truck}")

    def on_key_press(self, event):
        if not self.selected_truck or not self.mqtt_connected:
            return

        truck = self.trucks.get(self.selected_truck)
        if not truck or truck.mode == "AUTO":
            return

        self.last_key_time = time.time()
        key = event.keysym
        cmd_data = {}

        if key == 'Up' or key == 'w':
            self.target_speed = min(100, self.target_speed + 5)
            cmd_data['accelerate'] = self.target_speed

        elif key == 'Down' or key == 's':
            self.target_speed = max(-100, self.target_speed - 5)
            cmd_data['accelerate'] = self.target_speed

        elif key == 'space':
            self.target_speed = 0
            cmd_data['accelerate'] = 0

        elif key == 'Left' or key == 'a':
            cmd_data['steer_left'] = 5
            cmd_data['accelerate'] = self.target_speed

        elif key == 'Right' or key == 'd':
            cmd_data['steer_right'] = 5
            cmd_data['accelerate'] = self.target_speed

        if cmd_data:
            self.send_manual_command(cmd_data)

    def send_manual_command(self, data):
        if not self.selected_truck:
            return

        topic = MQTT_TOPIC_COMMANDS.format(self.selected_truck)
        payload = json.dumps(data)
        self.mqtt_client.publish(topic, payload)

    def canvas_x(self, map_x):
        return map_x / MAP_DISPLAY_SCALE

    def canvas_y(self, map_y):
        return map_y / MAP_DISPLAY_SCALE

    def draw_waypoint(self):
        if not self.waypoint_dirty:
            return

        if not self.target_waypoint:
            if self.waypoint_item_ids:
                self.canvas.delete(self.waypoint_item_ids['oval'])
                self.canvas.delete(self.waypoint_item_ids['text'])
                self.waypoint_item_ids = None
            self.waypoint_dirty = False
            return

        wx, wy = self.target_waypoint
        cx = self.canvas_x(wx)
        cy = self.canvas_y(wy)

        if self.waypoint_item_ids is None:
            oval_id = self.canvas.create_oval(
                cx - WAYPOINT_DISPLAY_RADIUS,
                cy - WAYPOINT_DISPLAY_RADIUS,
                cx + WAYPOINT_DISPLAY_RADIUS,
                cy + WAYPOINT_DISPLAY_RADIUS,
                fill='yellow',
                outline='orange',
                width=2,
                tags='waypoint'
            )
            text_id = self.canvas.create_text(
                cx, cy - 15,
                text="TARGET",
                fill='yellow',
                font=('Arial', 10, 'bold'),
                tags='waypoint'
            )
            self.waypoint_item_ids = {'oval': oval_id, 'text': text_id}
        else:
            self.canvas.coords(
                self.waypoint_item_ids['oval'],
                cx - WAYPOINT_DISPLAY_RADIUS,
                cy - WAYPOINT_DISPLAY_RADIUS,
                cx + WAYPOINT_DISPLAY_RADIUS,
                cy + WAYPOINT_DISPLAY_RADIUS
            )
            self.canvas.coords(self.waypoint_item_ids['text'], cx, cy - 15)
        
        self.waypoint_dirty = False

    def draw_truck_trail(self, truck, truck_items):
        if len(truck.position_history) < 2:
            if 'trail' in truck_items:
                self.canvas.delete(truck_items['trail'])
                del truck_items['trail']
            return

        trail_coords = []
        for px, py in truck.position_history:
            trail_coords.extend([self.canvas_x(px), self.canvas_y(py)])

        if 'trail' not in truck_items:
            trail_id = self.canvas.create_line(
                *trail_coords,
                fill='cyan',
                width=1,
                tags='trail'
            )
            truck_items['trail'] = trail_id
        else:
            self.canvas.coords(truck_items['trail'], *trail_coords)

    def draw_truck_body(self, truck, truck_items, x, y):
        color = truck.get_display_color()

        if 'body' not in truck_items:
            body_id = self.canvas.create_rectangle(
                x - TRUCK_DISPLAY_SIZE,
                y - TRUCK_DISPLAY_SIZE,
                x + TRUCK_DISPLAY_SIZE,
                y + TRUCK_DISPLAY_SIZE,
                fill=color,
                outline='white',
                width=2,
                tags='truck'
            )
            truck_items['body'] = body_id
        else:
            self.canvas.coords(
                truck_items['body'],
                x - TRUCK_DISPLAY_SIZE,
                y - TRUCK_DISPLAY_SIZE,
                x + TRUCK_DISPLAY_SIZE,
                y + TRUCK_DISPLAY_SIZE
            )
            self.canvas.itemconfig(truck_items['body'], fill=color)

    def draw_truck_direction(self, truck, truck_items, x, y):
        rad = math.radians(truck.angle)
        end_x = x + DIRECTION_INDICATOR_LENGTH * math.cos(rad)
        end_y = y + DIRECTION_INDICATOR_LENGTH * math.sin(rad)

        if 'direction' not in truck_items:
            direction_id = self.canvas.create_line(
                x, y, end_x, end_y,
                fill='yellow',
                width=2,
                tags='truck'
            )
            truck_items['direction'] = direction_id
        else:
            self.canvas.coords(truck_items['direction'], x, y, end_x, end_y)

    def draw_truck_label(self, truck, truck_items, x, y):
        label_text = f"T{truck.id}"
        if truck.arrived:
            label_text += " ✓"

        if 'label' not in truck_items:
            label_id = self.canvas.create_text(
                x, y - TRUCK_DISPLAY_SIZE - 10,
                text=label_text,
                fill='white',
                font=('Arial', 10, 'bold'),
                tags='truck'
            )
            truck_items['label'] = label_id
        else:
            self.canvas.coords(truck_items['label'], x, y - TRUCK_DISPLAY_SIZE - 10)
            self.canvas.itemconfig(truck_items['label'], text=label_text)

    def draw_trucks(self):
        self.draw_waypoint()

        current_truck_ids = set(self.trucks.keys())
        cached_truck_ids = set(self.canvas_items.keys())

        removed_trucks = cached_truck_ids - current_truck_ids
        for truck_id in removed_trucks:
            for item_id in self.canvas_items[truck_id].values():
                self.canvas.delete(item_id)
            del self.canvas_items[truck_id]

        for truck_id, truck in self.trucks.items():
            if not truck.dirty and truck_id in self.canvas_items:
                continue

            if truck_id not in self.canvas_items:
                self.canvas_items[truck_id] = {}

            truck_items = self.canvas_items[truck_id]
            x = self.canvas_x(truck.position_x)
            y = self.canvas_y(truck.position_y)

            self.draw_truck_trail(truck, truck_items)
            self.draw_truck_body(truck, truck_items, x, y)
            self.draw_truck_direction(truck, truck_items, x, y)
            self.draw_truck_label(truck, truck_items, x, y)
            
            truck.dirty = False

    def format_truck_info(self, truck):
        info = f"Truck {truck.id} Information\n"
        info += "=" * 30 + "\n\n"

        info += f"Position: ({truck.position_x}, {truck.position_y})\n"
        info += f"Heading: {truck.angle}°\n"
        info += f"Temperature: {truck.temperature}°C\n"

        info += f"\n--- Status ---\n"
        info += f"Mode: {truck.mode}\n"
        info += f"Fault State: {'FAULT' if truck.fault_state else 'OK'}\n"

        info += f"\n--- Control ---\n"
        info += f"Acceleration: {truck.acceleration}%\n"
        info += f"Steering: {truck.steering}°\n"
        info += f"Arrived: {'YES' if truck.arrived else 'NO'}\n"

        info += f"\n--- Faults ---\n"
        info += f"Electrical: {'FAULT' if truck.fault_electrical else 'OK'}\n"
        info += f"Hydraulic: {'FAULT' if truck.fault_hydraulic else 'OK'}\n"

        if truck.temperature > TEMPERATURE_CRITICAL_THRESHOLD:
            info += f"\n⚠ CRITICAL TEMPERATURE!\n"
        elif truck.is_temperature_warning():
            info += f"\n⚠ High temperature\n"

        if truck.last_update:
            age = (datetime.now() - truck.last_update).total_seconds()
            info += f"\n--- Updates ---\n"
            info += f"Last update: {age:.1f}s ago\n"

        return info

    def update_info_panel(self):
        if not self.selected_truck or self.selected_truck not in self.trucks:
            if self.last_info_text != "No truck selected":
                self.info_text.config(state='normal')
                self.info_text.delete('1.0', tk.END)
                self.info_text.insert('1.0', "No truck selected")
                self.info_text.config(state='disabled')
                self.last_info_text = "No truck selected"
            return

        truck = self.trucks[self.selected_truck]
        new_info = self.format_truck_info(truck)

        if new_info != self.last_info_text:
            self.info_text.config(state='normal')
            self.info_text.delete('1.0', tk.END)
            self.info_text.insert('1.0', new_info)
            self.info_text.config(state='disabled')
            self.last_info_text = new_info

    def update_gui(self):
        current_time = time.time()
        self.frame_times.append(current_time)

        if len(self.frame_times) >= 2:
            time_span = self.frame_times[-1] - self.frame_times[0]
            if time_span > 0:
                self.fps = (len(self.frame_times) - 1) / time_span
                self.fps_label.config(text=f"FPS: {self.fps:.1f}")

        if self.trucks:
            now = datetime.now()
            total_age = 0
            count = 0
            for truck in self.trucks.values():
                if truck.last_update:
                    age = (now - truck.last_update).total_seconds()
                    total_age += age
                    count += 1
            if count > 0:
                avg_age = total_age / count
                color = "green" if avg_age < 0.1 else ("orange" if avg_age < 0.5 else "red")
                self.avg_update_age_label.config(
                    text=f"Avg Update Age: {avg_age:.3f}s",
                    foreground=color
                )

        self.draw_trucks()
        self.update_info_panel()
        self.root.after(GUI_UPDATE_PERIOD_MS, self.update_gui)

    def check_heartbeat(self):
        # Send heartbeat command every 200ms if idle to prevent timeout
        if self.selected_truck and self.mqtt_connected:
            truck = self.trucks.get(self.selected_truck)
            if truck and truck.mode == "MANUAL":
                idle_time = time.time() - self.last_key_time
                if idle_time > 0.3:  # Only send if no key press for 300ms
                    # Send current target speed to keep alive
                    # Don't send steering (it will default to 0 in C++)
                    self.send_manual_command({'accelerate': self.target_speed})
        
        self.root.after(200, self.check_heartbeat)

    def on_closing(self):
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
