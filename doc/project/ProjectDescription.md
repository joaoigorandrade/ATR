# 1. INTRODUCTION AND CONTEXT
The mining sector has undergone significant transformation with the adoption of Industry 4.0 technologies, with autonomous vehicles being one of the promising technologies in this evolution. In this context, the adoption of autonomous vehicles emerges as a strategic solution to increase efficiency, reduce accidents, and improve control over extraction and transportation processes. The successful implementation of these systems requires a solid foundation in real-time systems, where functional correctness must be ensured within strict temporal constraints.

The development of autonomous vehicles presents considerable challenges, requiring the integration of different information sources, handling failures in real time, route planning, and ensuring synchronization among multiple trucks operating in parallel. Additionally, the systems must offer distinct modes of operation (manual and automatic), secure interfaces for interaction with local operators, and efficient communication with the mine's central management system.

In this work, you and your group will design and implement a simplified version of the control system for an autonomous truck. The work will enable the development and implementation of concurrent programming concepts in a realistic case of real-time industrial automation.

# 2. OBJECTIVES
Apply the knowledge from the Real-Time Automation discipline in the implementation of a control and monitoring system for an autonomous mining vehicle, according to the proposed architecture, covering both the onboard truck control and integration with the mine's central management system.

Specific objectives:
- Implement concurrent tasks for navigation, sensors, failures, and interface.
- Use synchronization mechanisms (mutex, semaphores, condition variables).
- Implement operation modes (local manual and remote automatic).
- Develop inter-process communication via IPC and pub/sub (MQTT).
- Build simplified graphical interfaces for local operation and central management.

# 3. DOCUMENT ORGANIZATION
Chapter 4 presents the general system specifications, which must be developed in two stages. Chapter 5 details the deliverables for Stage 1 and Stage 2. Chapter 6 presents general development instructions.

Page 1 of 6  
Real-Time Automation  
Prof. Leandro Freitas

# 4. GENERAL SYSTEM SPECIFICATIONS
Your group has been hired to develop the onboard application for the control system of an autonomous truck in a mine. The development will be simplified, containing only some critical parts. The complete system is illustrated in Figure 1, where each task is identified with a rectangle, and the interactions between them are represented by arrows.

**Figure 1 - Simplified control system for an autonomous truck.**

This will be developed in two complementary stages:
- **Stage 1:** Definition of the complete system architecture and implementation of all tasks in blue in Figure 1. Definition and implementation of the circular buffer (e.g., 200 positions) and its interactions with the other tasks. The interactions defined by the red arrows (events), orange (I/O), and green (IPC) are not implemented in this stage. The publisher and subscriber server, as well as the Mine Management and Mine Simulation interfaces, are also not implemented in this stage.
- **Stage 2:** Implementation of the Mine Management and Mine Simulation interfaces. This implementation does not necessarily need to be in C++; you can use a language that simplifies the process (e.g., Python with pygame). Implementation of the MQTT server and clients (publisher and subscriber) for communication with the systems. Functionality and scalability tests of the system, measuring the time spent by each task in critical situations.

The following sections provide more details on each system module. This is a basic specification of the system's operation. There are several nuances and details that you and your group will define to ensure the system's functionality.

Page 2 of 6  
Real-Time Automation  
Prof. Leandro Freitas

Table 1 shows the available sensors and actuators on the truck.

| Name of the variable | Type | Description |
|----------------------|------|-------------|
| i_posicao_x | int | Position of the vehicle on the x-axis, relative to an absolute ground reference obtained by position sensors (e.g., GNSS). |
| i_posicao_y | int | Position of the vehicle on the y-axis, relative to an absolute ground reference obtained by position sensors (e.g., GNSS). |
| i_angulo_x | int | Angular direction of the vehicle's front relative to east (east = angle zero), obtained by inertial sensors. |
| i_temperatura | int | Engine temperature (varies between -100 and +200). This temperature has an alert level if 𝑇 > 95 °C and generates a fault if 𝑇 > 120 °C. |
| i_falha_eletrica | bool | If true, indicates the presence of an electrical fault in the vehicle system. |
| i_falha_hidraulica | bool | If true, indicates the presence of a hydraulic fault in the vehicle system. |
| o_aceleracao | int | Determines the vehicle's acceleration as a percentage (-100 to 100%). |
| o_direcao | int | Determines the vehicle's steering angle in degrees (-180 to 180). When accelerating, the vehicle moves forward in the direction of the angle. |

The reading of positioning sensors is performed by the Sensor Processing task, which applies noise filtering using a moving average filter of order M (average of the last M measured samples). This task will populate a circular buffer for reading by the other tasks.

The actuator values (output) will be determined by the Command Logic task, which, from reading the processed sensors in the buffer, will determine the vehicle's state (manual, automatic, functioning, faulty) and process the commands. Table 2 shows the possible states and their encoding in the program variable.

Page 3 of 6  
Real-Time Automation  
Prof. Leandro Freitas

**Table 2 - States and commands for the truck. States are generated by the “Command Logic” task, from reading the buffer (processed data). Commands are performed by the operator in the “Local Interface” task.**

| Name of the variable | Type | Description |
|----------------------|------|-------------|
| e_defeito | bool | State that identifies the presence of a fault or fault not recognized by the operator (1: fault, 0: no fault). |
| e_automatico | bool | State that identifies the vehicle's operation mode (0: manual, 1: automatic). |
| c_automatico | bool | Command to switch the truck to automatic mode (true). Reset of this command. |
| c_man | bool | Command to switch the truck to manual mode (true). |
| c_rearme | bool | Command to rearm any fault that has occurred on the truck. |
| c_acelera | - | Command to accelerate the truck. |
| c_direita | - | Command to change the truck's angle (subtraction). |
| c_esquerda | - | Command to change the truck's angle (addition). |

**Attention:** This organization of variables is suggestive; if you wish, you can organize this information differently in your code, for example, by combining multiple boolean values into an integer variable.

The system also includes a Fault Monitoring task, responsible for reading fault sensor information (i_temperatura, i_falha_eletrica, i_falha_hidraulica, as per Table 1) and triggering events to the Command Logic, Navigation Control, and Data Collector tasks.

The Navigation Control task must execute two control algorithms: speed and angular position. The algorithm must read the states from the shared buffer to know if the vehicle is in manual or automatic mode. If in manual mode, the control must be disabled. In general, when in manual mode, the controller sets the setpoint to the current values to avoid undesired behavior when switching from manual to automatic mode again (bumpless transfer). When in automatic mode, the controller must read the setpoint value set by the Route Planning task and execute the control logic.

The Data Collector task is responsible for obtaining system state data, assigning a timestamp, and storing it in log files on disk. Each event log must contain the timestamp, truck identification (number), truck state, position, and event description. Storage on disk must be done with a specific structure, detailed in Table 3. This task is also responsible for organizing the data and establishing communication with the Local Interface task, in order to send the current truck states and receive commands from the local operator and publish to the buffer.

The Local Interface task handles interaction with the operator inside the truck, who must see on the screen (e.g., terminal) the system states, main measurements, and also accept operator commands, as per Table 2. Each command must be activated by a keyboard key.

The Mine Simulation task produces sensor data from the actuators. This task must also allow the user, in the simulation interface, to generate a fault in some truck. The generated sensor data must add random noise with zero mean, so that the sensor processing algorithm can handle this noise. Note that in the simulation, you must generate position values from acceleration and steering actuators. A good simulation implements a differential equation (or difference equation) to simulate the truck's movement dynamics (inertia). For this task, you can use any programming language and API that facilitates implementation.

The Mine Management task must present a real-time map of all trucks' positions in the mine. This task must know the position of all trucks in the mine, provided by the route planning system, and provide a user-friendly interface for the system. In this interface, it must be possible to change the position setpoint for each truck, so that the truck can follow a new setpoint. For this task, you can use any programming language and API that facilitates implementation.

The Route Planning task will read the initial and final points, defined by the Mine Management system, and define the speed and angular position setpoints for the Navigation Control system.

Page 4 of 6  
Real-Time Automation  
Prof. Leandro Freitas