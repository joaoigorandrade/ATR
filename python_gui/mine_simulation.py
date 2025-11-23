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

WINDOW_WIDTH_PIXELS = 1000
WINDOW_HEIGHT_PIXELS = 700
MAP_WIDTH_PIXELS = 1000
MAP_HEIGHT_PIXELS = 700
SIMULATION_FPS = 30

COLOR_BLACK = (0, 0, 0)
COLOR_WHITE = (255, 255, 255)
COLOR_GRAY = (128, 128, 128)
COLOR_DARK_GRAY = (64, 64, 64)
COLOR_RED = (255, 0, 0)
COLOR_GREEN = (0, 255, 0)
COLOR_BLUE = (0, 100, 255)
COLOR_YELLOW = (255, 255, 0)
COLOR_ORANGE = (255, 165, 0)

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
        self.color = COLOR_BLUE

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
            return COLOR_RED
        if self.temperature > TEMPERATURE_CRITICAL_THRESHOLD:
            return COLOR_RED
        if self.temperature > TEMPERATURE_WARNING_THRESHOLD:
            return COLOR_ORANGE
        return self.color

    def draw_body(self, screen):
        rotated_corners = self.calculate_rotated_corners()
        color = self.get_display_color()

        pygame.draw.polygon(screen, color, rotated_corners)
        pygame.draw.polygon(screen, COLOR_WHITE, rotated_corners, 2)

    def draw_direction_indicator(self, screen):
        rad = math.radians(self.angle)
        end_x = self.x + TRUCK_DIRECTION_LINE_LENGTH * math.cos(rad)
        end_y = self.y + TRUCK_DIRECTION_LINE_LENGTH * math.sin(rad)
        pygame.draw.line(screen, COLOR_YELLOW, (self.x, self.y), (end_x, end_y), 3)

    def draw_label(self, screen):
        font = pygame.font.Font(None, 24)
        text = font.render(f"T{self.id}", True, COLOR_WHITE)
        screen.blit(text, (self.x - 10, self.y - self.height / 2 - 20))

    def draw(self, screen):
        self.draw_body(screen)
        self.draw_direction_indicator(screen)
        self.draw_label(screen)


class MineSimulation:
    def __init__(self):
        pygame.init()
        self.screen = pygame.display.set_mode((WINDOW_WIDTH_PIXELS, WINDOW_HEIGHT_PIXELS))
        pygame.display.set_caption("Mine Simulation - Stage 2")
        self.clock = pygame.time.Clock()
        self.font = pygame.font.Font(None, 24)
        self.small_font = pygame.font.Font(None, 20)

        self.trucks = {
            1: Truck(1, 100, 200),
            2: Truck(2, 200, 300),
            3: Truck(3, 300, 400)
        }

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
        truck = self.trucks[DEFAULT_TRUCK_ID]
        truck.fault_electrical = not truck.fault_electrical
        print(f"[Simulation] Electrical fault: {truck.fault_electrical}")

    def toggle_hydraulic_fault(self):
        truck = self.trucks[DEFAULT_TRUCK_ID]
        truck.fault_hydraulic = not truck.fault_hydraulic
        print(f"[Simulation] Hydraulic fault: {truck.fault_hydraulic}")

    def increase_temperature(self):
        truck = self.trucks[DEFAULT_TRUCK_ID]
        truck.temperature += TEMPERATURE_TEST_INCREMENT
        print(f"[Simulation] Temperature: {truck.temperature}°C")

    def handle_events(self):
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                self.running = False

            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    self.running = False
                elif event.key == pygame.K_SPACE:
                    self.paused = not self.paused
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
            pygame.draw.line(self.screen, COLOR_GRAY, (x, 0), (x, MAP_HEIGHT_PIXELS), 1)
        for y in range(0, MAP_HEIGHT_PIXELS, GRID_SPACING_PIXELS):
            pygame.draw.line(self.screen, COLOR_GRAY, (0, y), (MAP_WIDTH_PIXELS, y), 1)

    def draw_trucks(self):
        for truck in self.trucks.values():
            truck.draw(self.screen)

    def draw_title(self, y_offset):
        title = self.font.render("MINE SIMULATION - Stage 2", True, COLOR_WHITE)
        self.screen.blit(title, (10, y_offset))
        return y_offset + 30

    def draw_status(self, y_offset):
        status_text = "PAUSED" if self.paused else "RUNNING"
        status_color = COLOR_YELLOW if self.paused else COLOR_GREEN
        status = self.font.render(f"Status: {status_text}", True, status_color)
        self.screen.blit(status, (10, y_offset))
        return y_offset + 25

    def draw_mqtt_status(self, y_offset):
        mqtt_text = "Connected" if self.mqtt_connected else "Disconnected"
        mqtt_color = COLOR_GREEN if self.mqtt_connected else COLOR_RED
        mqtt_status = self.small_font.render(f"MQTT: {mqtt_text}", True, mqtt_color)
        self.screen.blit(mqtt_status, (10, y_offset))
        return y_offset + 25

    def draw_truck_info(self, truck, y_offset):
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
            text = self.small_font.render(line, True, COLOR_WHITE)
            self.screen.blit(text, (10, y_offset))
            y_offset += 20

        return y_offset

    def draw_controls(self):
        y_offset = WINDOW_HEIGHT_PIXELS - 120
        controls = [
            "Controls:",
            "SPACE - Pause/Resume",
            "E - Toggle Electrical Fault",
            "H - Toggle Hydraulic Fault",
            "T - Increase Temperature",
            "ESC - Quit",
        ]

        for line in controls:
            text = self.small_font.render(line, True, COLOR_YELLOW)
            self.screen.blit(text, (10, y_offset))
            y_offset += 18

    def draw_ui(self):
        y_offset = 10
        y_offset = self.draw_title(y_offset)
        y_offset = self.draw_status(y_offset)
        y_offset = self.draw_mqtt_status(y_offset)

        for truck in self.trucks.values():
            y_offset = self.draw_truck_info(truck, y_offset)

        self.draw_controls()

    def draw(self):
        self.screen.fill(COLOR_DARK_GRAY)
        self.draw_grid()
        self.draw_trucks()
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
