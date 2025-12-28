# Hand Gesture-Controlled Robotic Car Using ESP32

![Project Status](https://img.shields.io/badge/Status-Completed-green)
![Platform](https://img.shields.io/badge/Platform-ESP32-blue)
![License](https://img.shields.io/badge/License-MIT-lightgrey)

## Project Report
**[Click here to read the full Project Report (PDF)](./Project_Report.pdf)**

## Overview
The primary aim of this project is to design and implement a real-time, intuitive control system for a robotic car using hand gestures. Traditional control methods, such as joysticks, are replaced with a wearable gesture-recognition transmitter enabled by the wireless capabilities of the ESP32 microcontroller.

This system utilizes an **MPU6050 accelerometer** to capture hand-tilt gestures, processed and transmitted via the low-latency **ESP-NOW protocol**. Additionally, the system features a **Hybrid Control Mode**, allowing a seamless switch to a web-based Wi-Fi interface for redundancy and safety.

## Key Features
* **Intuitive Gesture Control:** Control the car by tilting your hand (Forward, Backward, Left, Right, Stop).
* **Low Latency Communication:** Uses the connectionless ESP-NOW protocol for immediate response, bypassing the need for a router.
* **Hybrid Web Interface:** A backup Wi-Fi Access Point mode allows control via any smartphone browser if the controller is unavailable.
* **Safety Mechanisms:** Includes deadzone logic to prevent accidental movement and auto-stop functionality if the signal is lost.

## Hardware Requirements
Based on the experimental block diagram, the following components are required:

**Transmitter Unit (Controller):**
* ESP32 Development Board
* MPU6050 Accelerometer/Gyroscope Sensor
* 3.3V Power Source (Battery)
* Push Button (for Mode Locking) & LED

**Receiver Unit (Car):**
* ESP32 Development Board
* L298N Motor Driver Module
* DC Geared Motors (x2 or x4) & Wheels
* Robot Chassis
* 7-12V Power Supply (Li-ion batteries recommended)

## Circuit & Wiring
* **MPU6050 to ESP32:** VCC -> 3.3V, GND -> GND, SDA -> GPIO 21 (or 33), SCL -> GPIO 22 (or 32).
* **L298N to ESP32:** IN1, IN2, IN3, IN4 connected to defined digital pins. Enable pins connected to PWM-capable pins.
* **Mode Button:** Connected to GPIO 5 (Input Pullup).

## Software & Libraries
The project is built using the **Arduino IDE**. You must install the following libraries:
1.  `Adafruit MPU6050`
2.  `Adafruit Unified Sensor`
3.  `WebSockets` (by Markus Sattler)
4.  `WiFi` & `esp_now` (Built-in ESP32 libraries)

## Installation & Usage

### 1. Setup
1.  Clone this repository.
2.  Open `Car_Receiver.ino` and upload it to the **Car ESP32**.
3.  Open `Controller_Transmitter.ino` and upload it to the **Controller ESP32**.

### 2. Operating Modes
The system supports two modes of operation. Use the physical button on the Car to toggle between them.

**A. Controller Mode (Default)**
* Power on both the Controller and the Car.
* The system automatically pairs via **ESP-NOW**.
* **Control:**
    * Tilt Forward/Back -> Move Forward/Reverse.
    * Tilt Left/Right -> Turn Left/Right.
    * Hold Hand Flat -> Stop.

**B. Web Mode (Backup)**
1.  Ensure the Car is powered on.
2.  On your phone, turn off Mobile Data.
3.  Connect Wi-Fi to **SSID:** `RC_Car` with **Password:** `23571113`.
4.  Open a browser and navigate to `http://192.168.4.1`.
5.  Use the on-screen sliders and buttons to drive the car.

## Contributors
* **Course:** EEE4103 Microprocessor And Embedded System
* **Institution:** American International University-Bangladesh
* **Semester:** Fall 2025-2026
* **Group:** 08

---
*Disclaimer: This project was developed for educational purposes as a Capstone Project.*