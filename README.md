# 🦾 ESP32 Robotic Arm with Custom Web Dashboard

A custom ESP32-powered robotic arm featuring a responsive, self-hosted web control panel. The system combines embedded C++, web technologies, and motion-smoothing algorithms to deliver smooth and precise control. Users can control the arm wirelessly, adjust movement speed, save multiple positions, and automate sequences directly from a browser.

---

## ✨ Features

* 🌐 **Self-Hosted Web Interface** – Control the robotic arm from any smartphone, tablet, or computer connected to the ESP32 Wi-Fi network.
* 🎛️ **Real-Time Joint Control** – Smooth slider-based control for all joints and the gripper.
* ⚡ **Motion Smoothing** – Custom acceleration and deceleration algorithms for fluid movement.
* 💾 **Position Recording** – Save multiple arm positions directly to the ESP32's memory.
* ▶️ **Sequence Playback** – Play, pause, stop, and loop saved motion sequences.
* ⚙️ **Speed Adjustment** – Control the movement speed of the robotic arm from the web dashboard.
* ✋ **Gripper Pressure Control** – Adjust the gripper opening and closing range.
* 🏠 **Default Position Button** – Instantly return the arm to its predefined home position.
* 🎯 **Custom Position Button** – Move the arm to a user-defined position with a single click.
* 🌙 **Dark & Light Mode** – Switch between dark and light themes.
* 🚨 **Emergency Stop** – Immediately stop all movements for safety.
* 📱 **Responsive Design** – Optimized for both desktop and mobile devices.

---

## 🛠️ Hardware Used

* ESP32 Development Board
* PCA9685 16-Channel PWM Servo Driver
* 2 × MG996R Servo Motors (Shoulder & Elbow)
* 3 × SG90 Servo Motors (Base Rotation & Dual-Servo Gripper)
* 2 × Push Buttons (Gripper Open / Close)
* 1 × Red LED
* 1 × Green LED
* External 5V Power Supply

---

## 🔌 Wiring

### PCA9685 Connections

| PCA9685 | ESP32              |
| ------- | ------------------ |
| SDA     | GPIO 21            |
| SCL     | GPIO 22            |
| VCC     | 3.3V               |
| GND     | GND                |
| V+      | External 5V Supply |

### Buttons

| Component         | ESP32 Pin |
| ----------------- | --------- |
| Grip Close Button | GPIO 25   |
| Grip Open Button  | GPIO 26   |

### LEDs

| Component | ESP32 Pin |
| --------- | --------- |
| Red LED   | GPIO 13   |
| Green LED | GPIO 14   |

### Servo Channels

| PCA9685 Channel | Servo                  |
| --------------- | ---------------------- |
| Channel 0       | Shoulder (MG996R)      |
| Channel 1       | Elbow (MG996R)         |
| Channel 2       | Base Rotation (SG90)   |
| Channel 3       | Gripper Servo 1 (SG90) |
| Channel 4       | Gripper Servo 2 (SG90) |

---

## 🚀 Installation

### Required Libraries

Install the following libraries through the Arduino IDE Library Manager:

* WiFi.h
* WebServer.h
* Wire.h
* EEPROM.h
* Adafruit PWM Servo Driver Library

### Upload the Code

1. Open the project in Arduino IDE.
2. Select your ESP32 board.
3. Install all required libraries.
4. Compile and upload the code to the ESP32.

---

## 📶 Connecting to the Robotic Arm

1. Power on the robotic arm.
2. Connect to the ESP32 Wi-Fi network:

**SSID:** `RoboticArm_AP`

**Password:** `12345678`

3. Open a web browser and visit:

`http://192.168.4.1`

4. The control dashboard will open automatically.

---

## 🎮 Usage

### Manual Control

Use the sliders on the dashboard to control:

* Base Rotation
* Shoulder
* Elbow
* Gripper

### Save Positions

Click **Save Current Position** to store the current robotic arm position.

### Playback Controls

* ▶️ Start
* ⏸ Pause
* ⏹ Stop
* 🔁 Loop Sequence

### Settings Menu

Adjust:

* Motion Speed
* Playback Delay
* Motion Smoothness
* Gripper Limits

### Theme Settings

Switch between:

* 🌙 Dark Mode
* ☀️ Light Mode

---

## 📸 Project Preview

---

## 📸 Prototype Development Gallery

### Final Prototype
| Front View | Side View |
|------------|-----------|
| ![](Prototype-Image's/Hardware/IMG_20260629_172201.jpg) | ![](IMG_20260629_172628.jpg) |

### Working Demonstration
| Prototype | Object Pick-Up |
|-----------|----------------|
| ![](Prototype-Image's/Hardware/IMG_20260629_172534.jpg) | ![](Prototype-Image's/Hardware/IMG_20260629_172628.jpg) |

### CAD Design
![](IMG_20260629_172912.png)

### Prototype Parts
| Components | Assembly Parts |
|------------|----------------|
| ![](IMG_20260629_172401.jpg) | ![](IMG_20260629_174436.jpg) |

### Gripper Mechanism
![](IMG_20260629_173109.jpg)

### Mechanical Drawing
![](IMG_20260629_174346.jpg)

### Build Process
| Initial Linkage | Wooden Parts |
|-----------------|--------------|
| ![](IMG_20260629_174259.jpg) | ![](IMG_20260629_174214.jpg) |

### Additional Views
| View 1 | View 2 |
|--------|--------|
| ![](IMG_20260629_173500.jpg) | ![](IMG_20260629_173552.jpg) |

| View 3 | View 4 |
|--------|--------|
| ![](IMG_20260629_173901.jpg) | ![](IMG_20260629_174025.jpg) |

---

## 🔮 Future Improvements

* Object detection using OpenCV
* Automatic pick-and-place operations
* Mobile application control
* Inverse kinematics implementation
* Voice control support

---

## 📄 License

This project is open-source and available under the MIT License.

---

### Designed and built by Lakshmi Pavan Kalyan Imandi

Electronics and Communication Engineering (ECE) Student | Robotics & Embedded Systems Enthusiast
