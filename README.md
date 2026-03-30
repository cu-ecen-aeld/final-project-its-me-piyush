## 📌 Overview

Smart Rover is a **modular 4WD robotic system** built using:

* Raspberry Pi 4 Model B running a custom Yocto Project image
* ESP32 for real-time control
* SocketCAN for communication

The system follows a **distributed architecture**:

* **Raspberry Pi (High-Level Node)**
  Handles UI, control logic, camera processing, and autonomous decision-making

* **ESP32 (Low-Level Node)**
  Handles motor control, sensor timing, and safety-critical operations

* **CAN Bus**
  Provides reliable communication between nodes

---

## 🧠 System Architecture

* High-level Linux processing separated from real-time control
* CAN-based communication between nodes
* Modular and scalable design

> The ESP32 ensures deterministic real-time behavior, while the Raspberry Pi handles high-level logic.

---

## 🔗 Project Links

* 📘 **Wiki (Project Documentation):**
  👉 [https://github.com/cu-ecen-aeld/final-project-assignment-dwalkes/wiki](https://github.com/cu-ecen-aeld/final-project-its-me-piyush/wiki/Project-Overview)

* 📅 **Project Board / Schedule:**
  👉 https://github.com/users/its-me-piyush/projects/4

---

## 🧩 Features

* Custom Yocto-based Linux system
* CAN-based distributed control
* Real-time motor control using ESP32
* Ultrasonic obstacle detection
* Web-based control interface (Flask)
* Camera + LLM-based interaction
* Autonomous navigation

---

## 🛠️ Tech Stack

* **Embedded Linux:** Yocto
* **Backend:** Python + Flask
* **Communication:** SocketCAN
* **Firmware:** ESP32 (C/C++)
* **Vision:** libcamera + Gemini

---

## 👥 Team

**Piyush Nagpal**

* Controller HAT design
* ESP32 firmware
* CAN architecture
* Hardware integration

**Omkar Sangrulkar**

* Yocto system setup
* Camera + LLM integration
* Web interface
* High-level system logic

---

## 🚀 Getting Started (Planned)

1. Build Yocto image
2. Flash Raspberry Pi
3. Setup CAN interface
4. Upload ESP32 firmware
5. Run rover control services

---

## 📌 Status

🚧 In Development – Active project under structured sprint plan
