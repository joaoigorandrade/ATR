#!/usr/bin/env python3

import json
import math
import random
import sys
from datetime import datetime

import pygame

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("Warning: paho-mqtt not installed. Install with: pip install paho-mqtt")
    mqtt = None

WINDOW_WIDTH_PIXELS = 1300
WINDOW_HEIGHT_PIXELS = 700
MAP_WIDTH_PIXELS = 1000
MAP_HEIGHT_PIXELS = 700
SIDEBAR_WIDTH = 300
SIMULATION_FPS = 60

COLOR_BG = (11, 12, 16)         # #0b0c10
COLOR_SURFACE = (31, 40, 51)      # #1f2833
COLOR_SURFACE_LIGHT = (43, 54, 69) # #2b3645
COLOR_TEXT = (230, 230, 230)      # #e6e6e6
COLOR_ACCENT = (102, 252, 241)    # #66fcf1
COLOR_SUCCESS = (69, 162, 158)    # #45a29e
COLOR_WARNING = (255, 215, 0)     # #ffd700
COLOR_ERROR = (255, 56, 63)       # #ff383f
COLOR_MANUAL = (69, 162, 158)     # #45a29e
COLOR_AUTO = (102, 252, 241)      # #66fcf1
COLOR_GRID = (31, 40, 51)         # #1f2833

MQTT_BROKER_HOST = "localhost"
MQTT_BROKER_PORT = 1883
MQTT_TOPIC_SENSORS = "truck/{}/sensors"
MQTT_TOPIC_COMMANDS = "truck/{}/commands"

TRUCK_MAX_SPEED = 5.0
TRUCK_ACCELERATION_RATE = 0.3
TRUCK_WIDTH_PIXELS = 40
TRUCK_HEIGHT_PIXELS = 60
TRUCK_DIRECTION_LINE_LENGTH = 30

TEMPERATURE_MIN = 20
TEMPERATURE_MAX = 150
TEMPERATURE_BASE = 75.0
TEMPERATURE_INCREASE_RATE = 0.1
TEMPERATURE_DECREASE_RATE = 0.05
TEMPERATURE_WARNING_THRESHOLD = 95
TEMPERATURE_CRITICAL_THRESHOLD = 120
TEMPERATURE_TEST_INCREMENT = 20

VELOCITY_HEATING_THRESHOLD = 2.0

SENSOR_NOISE_POSITION = 2
SENSOR_NOISE_ANGLE = 1
SENSOR_NOISE_TEMPERATURE = 2

ANGLE_NORMALIZATION = 360
MAX_TURN_RATE_DEGREES = 5.0

GRID_SPACING_PIXELS = 100

SENSOR_PUBLISH_FRAME_INTERVAL = 1

DEFAULT_TRUCK_ID = 1
DEFAULT_TRUCK_X = 100
DEFAULT_TRUCK_Y = 200


class Truck:
    def __init__(self, truck_id, x, y):
        self.id = truck_id
        self.x = float(x)
        self.y = float(y)
        self.angle = 0.0
        self.velocity = 0.0
        self.acceleration = 0
        self.steering = 0

        self.temperature = TEMPERATURE_BASE
        self.fault_electrical = False
        self.fault_hydraulic = False

        self.max_speed = TRUCK_MAX_SPEED
        self.accel_rate = TRUCK_ACCELERATION_RATE

        self.width = TRUCK_WIDTH_PIXELS
        self.height = TRUCK_HEIGHT_PIXELS
        self.color = COLOR_MANUAL
        self.image = None

    def update_velocity(self):
        if self.acceleration != 0:
            self.velocity += self.accel_rate * (self.acceleration / 100.0)
        else:
            self.velocity = 0

        self.velocity = max(-self.max_speed, min(self.max_speed, self.velocity))

    def normalize_angle_difference(self, target_angle):
        angle_diff = target_angle - self.angle
        while angle_diff > 180:
            angle_diff -= ANGLE_NORMALIZATION
        while angle_diff < -180:
            angle_diff += ANGLE_NORMALIZATION
        return angle_diff

    def update_steering(self):
        angle_diff = self.normalize_angle_difference(self.steering)

        if abs(angle_diff) > MAX_TURN_RATE_DEGREES:
            self.angle += MAX_TURN_RATE_DEGREES if angle_diff > 0 else -MAX_TURN_RATE_DEGREES
        else:
            self.angle = self.steering

        self.angle %= ANGLE_NORMALIZATION

    def update_position(self):
        rad = math.radians(self.angle)
        self.x += self.velocity * math.cos(rad)
        self.y += self.velocity * math.sin(rad)

        self.x = max(0, min(MAP_WIDTH_PIXELS, self.x))
        self.y = max(0, min(MAP_HEIGHT_PIXELS, self.y))

    def update_temperature(self):
        if abs(self.velocity) > VELOCITY_HEATING_THRESHOLD:
            self.temperature += TEMPERATURE_INCREASE_RATE
        else:
            self.temperature -= TEMPERATURE_DECREASE_RATE

        self.temperature = max(TEMPERATURE_MIN, min(TEMPERATURE_MAX, self.temperature))

    def update_physics(self, dt=1.0):
        self.update_velocity()
        self.update_steering()
        self.update_position()
        self.update_temperature()

    def get_sensor_data_with_noise(self):
        return {
            "truck_id": self.id,
            "position_x": int(self.x + random.uniform(-SENSOR_NOISE_POSITION, SENSOR_NOISE_POSITION)),
            "position_y": int(self.y + random.uniform(-SENSOR_NOISE_POSITION, SENSOR_NOISE_POSITION)),
            "angle_x": int(self.angle + random.uniform(-SENSOR_NOISE_ANGLE, SENSOR_NOISE_ANGLE)) % ANGLE_NORMALIZATION,
            "temperature": int(self.temperature + random.uniform(-SENSOR_NOISE_TEMPERATURE, SENSOR_NOISE_TEMPERATURE)),
            "fault_electrical": self.fault_electrical,
            "fault_hydraulic": self.fault_hydraulic,
            "timestamp": int(datetime.now().timestamp() * 1000),
        }

    def calculate_rotated_corners(self):
        rad = math.radians(self.angle)
        cos_a = math.cos(rad)
        sin_a = math.sin(rad)

        corners = [
            (-self.width / 2, -self.height / 2),
            (self.width / 2, -self.height / 2),
            (self.width / 2, self.height / 2),
            (-self.width / 2, self.height / 2),
        ]

        rotated_corners = []
        for px, py in corners:
            rx = px * cos_a - py * sin_a
            ry = px * sin_a + py * cos_a
            rotated_corners.append((self.x + rx, self.y + ry))

        return rotated_corners

    def get_display_color(self):
        if self.fault_electrical or self.fault_hydraulic:
            return COLOR_ERROR
        if self.temperature > TEMPERATURE_CRITICAL_THRESHOLD:
            return COLOR_ERROR
        if self.temperature > TEMPERATURE_WARNING_THRESHOLD:
            return COLOR_WARNING
        return self.color

    def draw_body(self, screen):
        if self.image:
            rotated_image = pygame.transform.rotozoom(self.image, -self.angle, 1.0)
            rect = rotated_image.get_rect()
            rect.center = (self.x, self.y)
            screen.blit(rotated_image, rect)

            status_color = self.get_display_color()
            if status_color != self.color:
                pygame.draw.rect(screen, status_color, rect, 4)
        else:
            # Draw a futuristic arrow/triangle shape
            color = self.get_display_color()
            rad = math.radians(self.angle)
            cos_a = math.cos(rad)
            sin_a = math.sin(rad)
            
            # Local coordinates for a sharp arrowhead shape
            # Nose (front)
            p1 = (self.width * 0.6, 0)
            # Rear Right
            p2 = (-self.width * 0.5, self.height * 0.5)
            # Rear Center (indented)
            p3 = (-self.width * 0.3, 0)
            # Rear Left
            p4 = (-self.width * 0.5, -self.height * 0.5)
            
            points = [p1, p2, p3, p4]
            rotated_points = []
            
            for lx, ly in points:
                rx = lx * cos_a - ly * sin_a
                ry = lx * sin_a + ly * cos_a
                rotated_points.append((self.x + rx, self.y + ry))

            pygame.draw.polygon(screen, color, rotated_points)
            pygame.draw.polygon(screen, COLOR_BG, rotated_points, 2)  # Inner dark outline

            # Add a "core" glow or engine light
            engine_pos = (self.x - self.width * 0.3 * cos_a, self.y - self.width * 0.3 * sin_a)
            pygame.draw.circle(screen, COLOR_TEXT, engine_pos, 3)

            # Outer glow
            glow_surface = pygame.Surface((self.width * 2, self.height * 2), pygame.SRCALPHA)
            glow_rect = glow_surface.get_rect(center=(self.width, self.height))
            
            # Offset points for glow surface
            glow_points = [(x - self.x + self.width, y - self.y + self.height) for x, y in rotated_points]
            
            pygame.draw.polygon(glow_surface, (*color[:3], 40), glow_points)
            screen.blit(glow_surface, (self.x - self.width, self.y - self.height))

    def draw_direction_indicator(self, screen):
        rad = math.radians(self.angle)
        end_x = self.x + TRUCK_DIRECTION_LINE_LENGTH * math.cos(rad)
        end_y = self.y + TRUCK_DIRECTION_LINE_LENGTH * math.sin(rad)

        mid_x = self.x + (TRUCK_DIRECTION_LINE_LENGTH * 0.7) * math.cos(rad)
        mid_y = self.y + (TRUCK_DIRECTION_LINE_LENGTH * 0.7) * math.sin(rad)

        pygame.draw.line(screen, COLOR_TEXT, (self.x, self.y), (end_x, end_y), 4)

        arrow_size = 8
        arrow_angle_1 = rad + math.radians(150)
        arrow_angle_2 = rad - math.radians(150)
        arrow_p1 = (end_x + arrow_size * math.cos(arrow_angle_1), end_y + arrow_size * math.sin(arrow_angle_1))
        arrow_p2 = (end_x + arrow_size * math.cos(arrow_angle_2), end_y + arrow_size * math.sin(arrow_angle_2))
        pygame.draw.polygon(screen, COLOR_TEXT, [(end_x, end_y), arrow_p1, arrow_p2])

    def draw_label(self, screen):
        font = get_safe_font(18, bold=True)
        text = font.render(f"TRUCK {self.id}", True, COLOR_TEXT)
        bg_surface = pygame.Surface((text.get_width() + 10, text.get_height() + 4), pygame.SRCALPHA)
        pygame.draw.rect(bg_surface, (*COLOR_SURFACE, 180), bg_surface.get_rect(), border_radius=4)
        bg_surface.blit(text, (5, 2))
        screen.blit(bg_surface, (self.x - text.get_width() // 2 - 5, self.y - self.height / 2 - 28))

    def draw(self, screen):
        self.draw_body(screen)
        self.draw_direction_indicator(screen)
        self.draw_label(screen)


def get_safe_font(size, bold=False):
    font_names = ['segoeui', 'helveticaneue', 'arial', 'liberationsans', 'dejavusans']
    font_obj = None
    
    # Try to find a matching system font
    for name in font_names:
        font_path = pygame.font.match_font(name, bold=bold)
        if font_path:
            try:
                font_obj = pygame.font.Font(font_path, size)
                break
            except:
                continue
                
    # Fallback to default if no specific font found or error
    if font_obj is None:
        font_obj = pygame.font.SysFont(None, size, bold=bold)
        
    return font_obj

class MineSimulation:
    def __init__(self):
        pygame.init()
        self.screen = pygame.display.set_mode((WINDOW_WIDTH_PIXELS, WINDOW_HEIGHT_PIXELS))
        pygame.display.set_caption("Mine Simulation System")
        self.map_surface = pygame.Surface((MAP_WIDTH_PIXELS, MAP_HEIGHT_PIXELS))
        self.clock = pygame.time.Clock()
        
        self.font = get_safe_font(24, bold=True)
        self.small_font = get_safe_font(20)

        self.trucks = {
            1: Truck(1, 100, 200),
            2: Truck(2, 200, 300),
            3: Truck(3, 300, 400)
        }
        self.selected_truck_id = 1

        # Load truck image
        try:
            import os
            image_path = os.path.join("assets", "truck.png")
            if os.path.exists(image_path):
                truck_img = pygame.image.load(image_path).convert_alpha()
                # Scale to match defined dimensions if needed
                truck_img = pygame.transform.scale(truck_img, (TRUCK_WIDTH_PIXELS, TRUCK_HEIGHT_PIXELS))
                for truck in self.trucks.values():
                    truck.image = truck_img
                print(f"[Simulation] Loaded truck image from {image_path}")
            else:
                print(f"[Simulation] Truck image not found at {image_path}, using shapes.")
        except Exception as e:
            print(f"[Simulation] Error loading truck image: {e}")

        self.mqtt_client = None
        self.mqtt_connected = False
        if mqtt:
            self.setup_mqtt()

        self.running = True
        self.paused = False

    def setup_mqtt(self):
        try:
            self.mqtt_client = mqtt.Client(client_id="mine_simulation")
            self.mqtt_client.on_connect = self.on_mqtt_connect
            self.mqtt_client.on_message = self.on_mqtt_message

            self.mqtt_client.connect(MQTT_BROKER_HOST, MQTT_BROKER_PORT, 60)
            self.mqtt_client.loop_start()
            print(f"[MQTT] Connecting to broker at {MQTT_BROKER_HOST}:{MQTT_BROKER_PORT}")
        except Exception as e:
            print(f"[MQTT] Connection failed: {e}")
            print("[MQTT] Continuing without MQTT. Start broker with: mosquitto")

    def on_mqtt_connect(self, client, userdata, flags, rc):
        if rc == 0:
            self.mqtt_connected = True
            print("[MQTT] Connected successfully")

            for truck_id in self.trucks.keys():
                topic = MQTT_TOPIC_COMMANDS.format(truck_id)
                client.subscribe(topic)
                print(f"[MQTT] Subscribed to {topic}")
        else:
            print(f"[MQTT] Connection failed with code {rc}")

    def on_mqtt_message(self, client, userdata, msg):
        try:
            topic_parts = msg.topic.split("/")
            truck_id = int(topic_parts[1])

            if truck_id in self.trucks:
                command = json.loads(msg.payload.decode())
                truck = self.trucks[truck_id]

                if "acceleration" in command:
                    truck.acceleration = command["acceleration"]
                if "steering" in command:
                    truck.steering = command["steering"]

        except Exception as e:
            print(f"[MQTT] Error processing message: {e}")

    def publish_sensor_data(self):
        if not self.mqtt_connected or not self.mqtt_client:
            return

        for truck in self.trucks.values():
            sensor_data = truck.get_sensor_data_with_noise()
            topic = MQTT_TOPIC_SENSORS.format(truck.id)
            payload = json.dumps(sensor_data)
            self.mqtt_client.publish(topic, payload)

    def toggle_electrical_fault(self):
        if self.selected_truck_id in self.trucks:
            truck = self.trucks[self.selected_truck_id]
            truck.fault_electrical = not truck.fault_electrical
            print(f"[Simulation] Truck {truck.id} Electrical fault: {truck.fault_electrical}")

    def toggle_hydraulic_fault(self):
        if self.selected_truck_id in self.trucks:
            truck = self.trucks[self.selected_truck_id]
            truck.fault_hydraulic = not truck.fault_hydraulic
            print(f"[Simulation] Truck {truck.id} Hydraulic fault: {truck.fault_hydraulic}")

    def increase_temperature(self):
        if self.selected_truck_id in self.trucks:
            truck = self.trucks[self.selected_truck_id]
            truck.temperature += TEMPERATURE_TEST_INCREMENT
            print(f"[Simulation] Truck {truck.id} Temperature: {truck.temperature}°C")

    def handle_events(self):
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                self.running = False

            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    self.running = False
                elif event.key == pygame.K_SPACE:
                    self.paused = not self.paused
                elif event.key == pygame.K_1:
                    self.selected_truck_id = 1
                elif event.key == pygame.K_2:
                    self.selected_truck_id = 2
                elif event.key == pygame.K_3:
                    self.selected_truck_id = 3
                elif event.key == pygame.K_e:
                    self.toggle_electrical_fault()
                elif event.key == pygame.K_h:
                    self.toggle_hydraulic_fault()
                elif event.key == pygame.K_t:
                    self.increase_temperature()

    def update(self):
        if not self.paused:
            for truck in self.trucks.values():
                truck.update_physics()

    def draw_grid(self):
        for x in range(0, MAP_WIDTH_PIXELS, GRID_SPACING_PIXELS):
            pygame.draw.line(self.map_surface, COLOR_GRID, (x, 0), (x, MAP_HEIGHT_PIXELS), 1)
        for y in range(0, MAP_HEIGHT_PIXELS, GRID_SPACING_PIXELS):
            pygame.draw.line(self.map_surface, COLOR_GRID, (0, y), (MAP_WIDTH_PIXELS, y), 1)

    def draw_trucks(self):
        for truck in self.trucks.values():
            truck.draw(self.map_surface)

    def draw_title(self, y_offset):
        title_font = get_safe_font(28, bold=True)
        title = title_font.render("MINE SIMULATION", True, COLOR_ACCENT)
        bg_surface = pygame.Surface((title.get_width() + 20, title.get_height() + 10), pygame.SRCALPHA)
        pygame.draw.rect(bg_surface, (*COLOR_SURFACE, 200), bg_surface.get_rect(), border_radius=6)
        bg_surface.blit(title, (10, 5))
        self.screen.blit(bg_surface, (10, y_offset))
        return y_offset + title.get_height() + 20

    def draw_status(self, y_offset):
        status_text = "PAUSED" if self.paused else "RUNNING"
        status_color = COLOR_WARNING if self.paused else COLOR_SUCCESS
        status_font = get_safe_font(20, bold=True)
        status = status_font.render(f"● {status_text}", True, status_color)
        bg_surface = pygame.Surface((status.get_width() + 16, status.get_height() + 8), pygame.SRCALPHA)
        pygame.draw.rect(bg_surface, (*COLOR_SURFACE, 200), bg_surface.get_rect(), border_radius=5)
        bg_surface.blit(status, (8, 4))
        self.screen.blit(bg_surface, (10, y_offset))
        return y_offset + status.get_height() + 15

    def draw_mqtt_status(self, y_offset):
        mqtt_text = "Connected" if self.mqtt_connected else "Disconnected"
        mqtt_color = COLOR_SUCCESS if self.mqtt_connected else COLOR_ERROR
        mqtt_font = get_safe_font(16, bold=True)
        mqtt_status = mqtt_font.render(f"MQTT: {mqtt_text}", True, mqtt_color)
        bg_surface = pygame.Surface((mqtt_status.get_width() + 16, mqtt_status.get_height() + 8), pygame.SRCALPHA)
        pygame.draw.rect(bg_surface, (*COLOR_SURFACE, 200), bg_surface.get_rect(), border_radius=5)
        bg_surface.blit(mqtt_status, (8, 4))
        self.screen.blit(bg_surface, (10, y_offset))
        return y_offset + mqtt_status.get_height() + 15

    def draw_truck_info(self, truck, y_offset, is_selected=False):
        info_font = get_safe_font(14, bold=True)
        title_font = get_safe_font(16, bold=True)

        max_width = SIDEBAR_WIDTH - 20
        padding = 10
        
        # Determine content based on selection
        if is_selected:
            bg_color = (*COLOR_SURFACE, 240)
            border_color = COLOR_ACCENT
            border_width = 2
            
            info_data = [
                (f"Position:", f"({int(truck.x)}, {int(truck.y)})", COLOR_TEXT),
                (f"Heading:", f"{int(truck.angle)}°", COLOR_TEXT),
                (f"Velocity:", f"{truck.velocity:.1f}", COLOR_SUCCESS if abs(truck.velocity) > 0 else COLOR_TEXT),
                (f"Temperature:", f"{int(truck.temperature)}°C", COLOR_ERROR if truck.temperature > TEMPERATURE_CRITICAL_THRESHOLD else (COLOR_WARNING if truck.temperature > TEMPERATURE_WARNING_THRESHOLD else COLOR_TEXT)),
                (f"Acceleration:", f"{truck.acceleration}%", COLOR_TEXT),
                (f"Steering:", f"{truck.steering}°", COLOR_TEXT),
                (f"Faults:", f"{'YES' if (truck.fault_electrical or truck.fault_hydraulic) else 'NO'}", COLOR_ERROR if (truck.fault_electrical or truck.fault_hydraulic) else COLOR_SUCCESS)
            ]
        else:
            bg_color = (*COLOR_SURFACE, 180)
            border_color = (*COLOR_SURFACE_LIGHT, 100)
            border_width = 1
            
            # Compact view for unselected
            info_data = [
                (f"Vel: {truck.velocity:.1f}", f"Temp: {int(truck.temperature)}°C", COLOR_TEXT),
                (f"Faults:", f"{'YES' if (truck.fault_electrical or truck.fault_hydraulic) else 'NO'}", COLOR_ERROR if (truck.fault_electrical or truck.fault_hydraulic) else COLOR_SUCCESS)
            ]

        # Calculate height
        header_height = 25
        row_height = 22
        total_height = header_height + (len(info_data) * row_height) + padding
        
        # Draw Card Background
        bg_surface = pygame.Surface((max_width, total_height), pygame.SRCALPHA)
        pygame.draw.rect(bg_surface, bg_color, bg_surface.get_rect(), border_radius=5)
        
        # Draw Border
        if is_selected:
            pygame.draw.rect(bg_surface, border_color, bg_surface.get_rect(), width=border_width, border_radius=5)
        
        self.screen.blit(bg_surface, (10, y_offset))

        # Draw Title
        title_color = COLOR_ACCENT if is_selected else COLOR_TEXT
        title = title_font.render(f"TRUCK {truck.id} {'[SELECTED]' if is_selected else ''}", True, title_color)
        self.screen.blit(title, (20, y_offset + 5))

        # Draw Data
        local_y = header_height
        for label, value, color in info_data:
            if is_selected:
                # Key-Value pair
                label_text = info_font.render(label, True, COLOR_TEXT)
                value_text = info_font.render(value, True, color)
                self.screen.blit(label_text, (20, y_offset + local_y))
                self.screen.blit(value_text, (10 + max_width - value_text.get_width() - 10, y_offset + local_y))
            else:
                # Summary row (just text, simplified)
                # Actually, let's keep the split if possible, or just render the text
                # Logic above put 2 items in label/value tuple for compact view, let's render them left/right
                t1 = info_font.render(label, True, COLOR_TEXT) # actually holding "Vel: ..."
                t2 = info_font.render(value, True, color)      # actually holding "Temp: ..." or "YES/NO"
                
                self.screen.blit(t1, (20, y_offset + local_y))
                self.screen.blit(t2, (10 + max_width - t2.get_width() - 10, y_offset + local_y))

            local_y += row_height
        
        return y_offset + total_height + 10

    def draw_controls(self):
        y_offset = WINDOW_HEIGHT_PIXELS - 150
        title_font = get_safe_font(16, bold=True)
        control_font = get_safe_font(13)

        title = title_font.render("CONTROLS", True, COLOR_ACCENT)
        self.screen.blit(title, (15, y_offset))
        y_offset += title.get_height() + 8

        controls = [
            "1, 2, 3 - Select Truck",
            "E - Toggle Electrical Fault",
            "H - Toggle Hydraulic Fault",
            "T - Temperature +20°C",
            "SPACE - Pause/Resume",
            "ESC - Quit",
        ]

        max_width = SIDEBAR_WIDTH - 20
        for line in controls:
            text = control_font.render(line, True, COLOR_TEXT)
            # No background boxes for cleaner look in the new sidebar
            self.screen.blit(text, (20, y_offset))
            y_offset += text.get_height() + 5

    def draw_ui(self):
        # Draw Sidebar Background
        sidebar_surface = pygame.Surface((SIDEBAR_WIDTH, WINDOW_HEIGHT_PIXELS))
        sidebar_surface.fill(COLOR_BG) 
        self.screen.blit(sidebar_surface, (0, 0))
        
        # Draw sidebar border line
        pygame.draw.line(self.screen, COLOR_SURFACE_LIGHT, (SIDEBAR_WIDTH, 0), (SIDEBAR_WIDTH, WINDOW_HEIGHT_PIXELS), 2)

        y_offset = 10
        y_offset = self.draw_title(y_offset)
        y_offset = self.draw_status(y_offset)
        y_offset = self.draw_mqtt_status(y_offset)
        
        # Divider
        y_offset += 10
        pygame.draw.line(self.screen, COLOR_SURFACE_LIGHT, (20, y_offset), (SIDEBAR_WIDTH - 20, y_offset), 1)
        y_offset += 20

        for truck_id in sorted(self.trucks.keys()):
            truck = self.trucks[truck_id]
            is_selected = (truck_id == self.selected_truck_id)
            y_offset = self.draw_truck_info(truck, y_offset, is_selected)

        self.draw_controls()

    def draw(self):
        self.screen.fill(COLOR_BG)
        self.map_surface.fill(COLOR_BG)
        
        self.draw_grid()
        self.draw_trucks()
        
        self.screen.blit(self.map_surface, (SIDEBAR_WIDTH, 0))
        self.draw_ui()
        
        pygame.display.flip()

    def print_startup_message(self):
        print("=" * 50)
        print("Mine Simulation Started")
        print("=" * 50)
        print("Controls:")
        print("  SPACE - Pause/Resume")
        print("  E - Toggle Electrical Fault")
        print("  H - Toggle Hydraulic Fault")
        print("  T - Increase Temperature")
        print("  ESC - Quit")
        print("=" * 50)

    def run(self):
        self.print_startup_message()

        frame_count = 0

        while self.running:
            self.handle_events()
            self.update()
            self.draw()

            frame_count += 1
            if frame_count % SENSOR_PUBLISH_FRAME_INTERVAL == 0:
                self.publish_sensor_data()

            self.clock.tick(SIMULATION_FPS)

        if self.mqtt_client:
            self.mqtt_client.loop_stop()
            self.mqtt_client.disconnect()

        pygame.quit()
        print("\n[Simulation] Shutdown complete")


if __name__ == "__main__":
    sim = MineSimulation()
    sim.run()