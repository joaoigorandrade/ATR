#!/usr/bin/env python3
"""
Mine Simulation GUI

Simulates the mine environment and truck physics:
- Visual representation of trucks on a 2D map
- Physics simulation: position from acceleration/steering
- Random sensor noise generation
- Fault injection capability
- MQTT publisher for sensor data

Real-Time Automation Concepts:
- Simulation of physical systems
- Sensor noise modeling
- MQTT pub/sub communication
"""

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

# Constants
WINDOW_WIDTH = 1000
WINDOW_HEIGHT = 700
MAP_WIDTH = 1000
MAP_HEIGHT = 700
FPS = 30

# Colors
BLACK = (0, 0, 0)
WHITE = (255, 255, 255)
GRAY = (128, 128, 128)
DARK_GRAY = (64, 64, 64)
RED = (255, 0, 0)
GREEN = (0, 255, 0)
BLUE = (0, 100, 255)
YELLOW = (255, 255, 0)
ORANGE = (255, 165, 0)

# MQTT Configuration
MQTT_BROKER = "localhost"
MQTT_PORT = 1883
MQTT_TOPIC_SENSORS = "truck/{}/sensors"
MQTT_TOPIC_COMMANDS = "truck/{}/commands"


class Truck:
    """Represents a simulated mining truck"""

    def __init__(self, truck_id, x, y):
        self.id = truck_id
        self.x = float(x)
        self.y = float(y)
        self.angle = 0.0
        self.velocity = 0.0
        self.acceleration = 0
        self.steering = 0

        self.temperature = 75.0
        self.fault_electrical = False
        self.fault_hydraulic = False

        self.max_speed = 5.0
        self.accel_rate = 0.3
        self.friction = 0.95

        self.width = 40
        self.height = 60
        self.color = BLUE

    def update_physics(self, dt=1.0):
        """Update truck physics based on acceleration and steering"""
        if self.acceleration > 0:
            self.velocity += self.accel_rate * (self.acceleration / 100.0)
        elif self.acceleration < 0:
            self.velocity += self.accel_rate * (self.acceleration / 100.0)
        else:
            self.velocity *= self.friction

        self.velocity = max(-self.max_speed, min(self.max_speed, self.velocity))

        if abs(self.velocity) > 0.1:
            turn_rate = 2.0 * (self.steering / 180.0)
            self.angle += turn_rate
            self.angle %= 360

        rad = math.radians(self.angle)
        self.x += self.velocity * math.cos(rad)
        self.y += self.velocity * math.sin(rad)

        self.x = max(0, min(MAP_WIDTH, self.x))
        self.y = max(0, min(MAP_HEIGHT, self.y))

        if abs(self.velocity) > 2.0:
            self.temperature += 0.1
        else:
            self.temperature -= 0.05

        self.temperature = max(20, min(150, self.temperature))

    def get_sensor_data_with_noise(self):
        """Get sensor data with random noise"""
        noise_pos = 2
        noise_angle = 1
        noise_temp = 2

        return {
            "truck_id": self.id,
            "position_x": int(self.x + random.uniform(-noise_pos, noise_pos)),
            "position_y": int(self.y + random.uniform(-noise_pos, noise_pos)),
            "angle_x": int(self.angle + random.uniform(-noise_angle, noise_angle))
            % 360,
            "temperature": int(
                self.temperature + random.uniform(-noise_temp, noise_temp)
            ),
            "fault_electrical": self.fault_electrical,
            "fault_hydraulic": self.fault_hydraulic,
            "timestamp": int(datetime.now().timestamp() * 1000),
        }

    def draw(self, screen):
        """Draw the truck on the screen"""
        rad = math.radians(self.angle)
        cos_a = math.cos(rad)
        sin_a = math.sin(rad)
        points = [
            (-self.width / 2, -self.height / 2),
            (self.width / 2, -self.height / 2),
            (self.width / 2, self.height / 2),
            (-self.width / 2, self.height / 2),
        ]

        rotated_points = []
        for px, py in points:
            rx = px * cos_a - py * sin_a
            ry = px * sin_a + py * cos_a
            rotated_points.append((self.x + rx, self.y + ry))

        color = self.color
        if self.fault_electrical or self.fault_hydraulic:
            color = RED
        elif self.temperature > 120:
            color = RED
        elif self.temperature > 95:
            color = ORANGE

        pygame.draw.polygon(screen, color, rotated_points)
        pygame.draw.polygon(screen, WHITE, rotated_points, 2)

        dir_len = 30
        end_x = self.x + dir_len * cos_a
        end_y = self.y + dir_len * sin_a
        pygame.draw.line(screen, YELLOW, (self.x, self.y), (end_x, end_y), 3)

        font = pygame.font.Font(None, 24)
        text = font.render(f"T{self.id}", True, WHITE)
        screen.blit(text, (self.x - 10, self.y - self.height / 2 - 20))


class MineSimulation:
    """Main simulation class"""

    def __init__(self):
        pygame.init()
        self.screen = pygame.display.set_mode((WINDOW_WIDTH, WINDOW_HEIGHT))
        pygame.display.set_caption("Mine Simulation - Stage 2")
        self.clock = pygame.time.Clock()
        self.font = pygame.font.Font(None, 24)
        self.small_font = pygame.font.Font(None, 20)

        self.trucks = {1: Truck(1, 100, 200)}

        self.mqtt_client = None
        self.mqtt_connected = False
        if mqtt:
            self.setup_mqtt()

        self.running = True
        self.paused = False

    def setup_mqtt(self):
        """Initialize MQTT connection"""
        try:
            self.mqtt_client = mqtt.Client(client_id="mine_simulation")
            self.mqtt_client.on_connect = self.on_mqtt_connect
            self.mqtt_client.on_message = self.on_mqtt_message

            self.mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
            self.mqtt_client.loop_start()
            print(f"[MQTT] Connecting to broker at {MQTT_BROKER}:{MQTT_PORT}")
        except Exception as e:
            print(f"[MQTT] Connection failed: {e}")
            print("[MQTT] Continuing without MQTT. Start broker with: mosquitto")

    def on_mqtt_connect(self, client, userdata, flags, rc):
        """MQTT connection callback"""
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
        """MQTT message callback for receiving commands"""
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
        """Publish sensor data for all trucks via MQTT"""
        if not self.mqtt_connected or not self.mqtt_client:
            return

        for truck in self.trucks.values():
            sensor_data = truck.get_sensor_data_with_noise()
            topic = MQTT_TOPIC_SENSORS.format(truck.id)
            payload = json.dumps(sensor_data)
            self.mqtt_client.publish(topic, payload)

    def handle_events(self):
        """Handle pygame events"""
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                self.running = False

            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    self.running = False
                elif event.key == pygame.K_SPACE:
                    self.paused = not self.paused
                elif event.key == pygame.K_e:
                    self.trucks[1].fault_electrical = not self.trucks[
                        1
                    ].fault_electrical
                    print(
                        f"[Simulation] Electrical fault: {self.trucks[1].fault_electrical}"
                    )
                elif event.key == pygame.K_h:
                    self.trucks[1].fault_hydraulic = not self.trucks[1].fault_hydraulic
                    print(
                        f"[Simulation] Hydraulic fault: {self.trucks[1].fault_hydraulic}"
                    )
                elif event.key == pygame.K_t:
                    self.trucks[1].temperature += 20
                    print(f"[Simulation] Temperature: {self.trucks[1].temperature}°C")

    def update(self):
        """Update simulation state"""
        if not self.paused:
            for truck in self.trucks.values():
                truck.update_physics()

    def draw(self):
        """Draw simulation"""
        self.screen.fill(DARK_GRAY)

        for x in range(0, MAP_WIDTH, 100):
            pygame.draw.line(self.screen, GRAY, (x, 0), (x, MAP_HEIGHT), 1)
        for y in range(0, MAP_HEIGHT, 100):
            pygame.draw.line(self.screen, GRAY, (0, y), (MAP_WIDTH, y), 1)

        for truck in self.trucks.values():
            truck.draw(self.screen)

        self.draw_ui()

        pygame.display.flip()

    def draw_ui(self):
        """Draw user interface"""
        y_offset = 10

        title = self.font.render("MINE SIMULATION - Stage 2", True, WHITE)
        self.screen.blit(title, (10, y_offset))
        y_offset += 30

        status_text = "PAUSED" if self.paused else "RUNNING"
        status_color = YELLOW if self.paused else GREEN
        status = self.font.render(f"Status: {status_text}", True, status_color)
        self.screen.blit(status, (10, y_offset))
        y_offset += 25

        mqtt_text = "Connected" if self.mqtt_connected else "Disconnected"
        mqtt_color = GREEN if self.mqtt_connected else RED
        mqtt_status = self.small_font.render(f"MQTT: {mqtt_text}", True, mqtt_color)
        self.screen.blit(mqtt_status, (10, y_offset))
        y_offset += 25

        for truck in self.trucks.values():
            y_offset += 10
            info_lines = [
                f"Truck {truck.id}:",
                f"  Pos: ({int(truck.x)}, {int(truck.y)})",
                f"  Angle: {int(truck.angle)}°",
                f"  Vel: {truck.velocity:.1f}",
                f"  Temp: {int(truck.temperature)}°C",
                f"  Acc: {truck.acceleration}%",
                f"  Steer: {truck.steering}°",
            ]

            for line in info_lines:
                text = self.small_font.render(line, True, WHITE)
                self.screen.blit(text, (10, y_offset))
                y_offset += 20

        y_offset = WINDOW_HEIGHT - 120
        controls = [
            "Controls:",
            "SPACE - Pause/Resume",
            "E - Toggle Electrical Fault",
            "H - Toggle Hydraulic Fault",
            "T - Increase Temperature",
            "ESC - Quit",
        ]

        for line in controls:
            text = self.small_font.render(line, True, YELLOW)
            self.screen.blit(text, (10, y_offset))
            y_offset += 18

    def run(self):
        """Main simulation loop"""
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

        frame_count = 0

        while self.running:
            self.handle_events()
            self.update()
            self.draw()

            frame_count += 1
            if frame_count % 10 == 0:
                self.publish_sensor_data()

            self.clock.tick(FPS)

        if self.mqtt_client:
            self.mqtt_client.loop_stop()
            self.mqtt_client.disconnect()

        pygame.quit()
        print("\n[Simulation] Shutdown complete")


if __name__ == "__main__":
    sim = MineSimulation()
    sim.run()
