# 🧭 AlpiSafe – Real-Time Safety System for Alpine Environments

## 📌 Table of Contents
1. [About the Project](#-about-the-project)
2. [Features](#-features)
3. [Technologies Used](#-technologies-used)
4. [Hardware Components](#-hardware-components)
5. [Setup and Installation](#-setup-and-installation)
6. [How It Works](#-how-it-works)

---

## 🚀 About the Project

**AlpiSafe** is an embedded system designed to **detect and respond to critical conditions** encountered by alpinists or backcountry skiers. By leveraging real-time sensor data, it assesses the user's condition and environment, and automatically notifies rescue services via Firebase when needed.

---

## 🔧 Features

- Real-time monitoring of heart rate, body and ambient temperature, motion, and GPS.
- Severity scoring algorithm with threshold-based alert escalation.
- Automatic SOS triggering via Wi-Fi.
- Display of vital signs and system status through a serial-based interactive UI.
- Weight optimization via Grid Search to fine-tune alert accuracy.

---

## 🧪 Technologies Used

- **Languages**: C/C++ (Arduino)
- **Firmware Framework**: Arduino Core for ESP32
- **Backend**: Firebase Realtime Database
- **Data Analysis & Optimization**: Python (for Grid Search)

---

## 🔩 Hardware Components

- **ESP32** (main microcontroller with Wi-Fi)
- **MAX30102** (heart rate & SpO2 sensor)
- **MPU6050** (accelerometer & gyroscope)
- **DHT22** (ambient temperature and humidity)
- **GPS Module**
- **Push Button** (manual override)
- **LED & Buzzer** (visual/auditory alerts)

---

## 🛠️ Setup and Installation
```bash
git clone https://github.com/cherguihakim/AlpiSafe.git
cd AlpiSafe
```
- Open the project in **Arduino IDE** or **PlatformIO**.
- Install required libraries (`Adafruit_Sensor`, `FirebaseESP32`, etc.).
- Flash the firmware to the **ESP32**.
- Configure your **Wi-Fi credentials** and **Firebase project keys** in `config.h`.

---

## 🧠 How It Works

- Sensor data is acquired periodically.
- A severity score is computed using weighted sub-scores.
- If the score exceeds alert thresholds, a warning is issued.
- If the user does not acknowledge (via button press), an SOS is triggered.
- All data is sent to **Firebase** for remote monitoring.





