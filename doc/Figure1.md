---
## ✅ **Detailed Image Description for a Blind Person**

The image is a **block diagram** showing a **simplified control system for an autonomous truck operating in a mine**.  
Everything is organized into **modules** connected by arrows representing communication between the parts.

---

### ✅ **1. Left Side – Simulation**

On the left side, there is a green block with the text **“Mine Simulation”**.

* This block represents software that simulates the mine environment.
* It connects to a screen with a person using a computer.
* The caption reads **“Simulation Interface”**.
* In other words, an operator can observe and manipulate a simulation of the mine.

---

### ✅ **2. Center-Left – Trucks**

Next to the simulation, there is a large vertical gray block labeled **“pub/sub”** (publish and subscribe, a communication model).

Inside it, there are small stacked blocks representing multiple trucks:

* **Truck 1**

  * sensors
  * actuators
  * fault sensors
* Then, ellipses (...)
* **Truck N** (the last one)

This symbolizes:

* One or several autonomous trucks operating at the same time
* Each truck has sensors, actuators, and specific sensors to detect faults
* All communicate using the publish-subscribe model

---

### ✅ **3. Center of the Diagram – Main Processing**

In the center, there is a vertical column called **“Circular Buffer”**, a kind of “message queue”.

This buffer is surrounded by blue blocks with cyclic communication arrows (circular arrows), indicating that they run repeatedly:

* **Sensor Processing**

  Processes data from each truck's sensors.

* **Command Logic**

  Decides which commands to send to the truck based on the received data.

* **Route Planning**

  Defines trajectories.

* **Navigation Control**

  Makes the truck follow the planned route.

These parts constantly exchange data through the **circular buffer**.

---

### ✅ **4. Monitoring and Local Interface**

Below the command logic, there is a blue block called **“Fault Monitoring”**.

* It receives data about possible defects
* It alerts other modules when something goes wrong

From its lower part, an arrow leads to another blue block: **“Local Interface”**

* This module connects to a computer operated by a person
* Caption: **Local Operation**
* This means a human operator can monitor the truck or intervene if necessary

---

### ✅ **5. Inter-Process Communication**

A green line connects the Fault Monitoring with the **Data Collector**

It is marked with **IPC**, which means “Inter-Process Communication”.

---

### ✅ **6. Data Collection and Storage**

In the bottom-right corner, there is:

* a blue block called **“Data Collector”**
* connected to an orange cylinder symbolizing a **storage file**
* this storage represents writing data to disk (history and operation logs)

---

### ✅ **7. Interaction with Mine Management**

In the right corner, there is a gray area marked again as **“pub/sub”**.

* It connects to a green block: **Mine Management GUI**
* A person using a computer accesses this GUI (graphical interface)
* Caption: **Mine Management**
* In other words, staff can supervise and control operations remotely.

---

### ✅ **8. Visual Legend – Explaining the Symbols Used**

In the bottom-right section, there is a box called **Legend**. It explains the types of lines and symbols in the figure:

* **Blue Arrow** → Direct memory access
* **Green Arrow** → Inter-process communication (IPC)
* **Red Arrow** → Event-based communication
* **Orange Arrow** → Input and output operations (like disk, keyboard, video)
* **Circular Arrows** → Repetitive and cyclic tasks

---

### ✅ **Final Message of the Figure**

The image shows that:

* Autonomous trucks send sensors and receive commands
* The system collects data, plans routes, and navigates
* Operators can interact locally or remotely
* There is fault monitoring
* Everything communicates via buffers, events, and IPC

---
