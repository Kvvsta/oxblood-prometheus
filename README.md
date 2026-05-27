# Motion Controlled Game System
**Short Title:** Oxblood-Prometheus  
**GitHub:** [https://github.com/Kvvsta/oxblood-prometheus](https://github.com/Kvvsta/oxblood-prometheus)

## Team Members
- Kostas
- Josh
- Sahani

---

## Project Overview

Oxblood-Prometheus is a motion-controlled 2D video game based on the Zephyr RTOS. After deciding on the natural satellite "Prometheus" for the short title of our project, we were inspired by the Greek myth of the titan with the same name.

In Greek mythology, Prometheus stole fire from Mount Olympus and gifted it to humanity. As punishment, he was condemned to the eternal torment of having his liver daily consumed by an eagle. He was eventually freed by the hero Hercules during the eleventh of his twelve infamous labours.

In our video game, the player takes the role of Hercules, wielding a physical wand (a Seeed XIAO nRF52840 microcontroller) to defeat waves of eagles before they reach Prometheus and consume his liver. The eagles spawn at the edges of the screen and slowly make their way towards the centre, where Prometheus lies. The player must defeat these entities by colliding their player icon with the eagle icons. If any eagle were to reach the centre, it triggers game over.

This is an endless game, akin to something like the "No-Internet Dinosaur game" by Google — there is no end goal or way to definitively beat the game. The longer the game runs, the more difficult it becomes: the rate of eagle spawns and their velocity increase proportionally with time.

The game system is built across four embedded nodes and a PC software stack:

- **Mobile Node 1 (PROMETHEUS-P1)** — held in the player's hand, continuously samples a LSM6DSL gyroscope sensor, extracting angular velocity to measure the direction and speed of the player's wrist motions. These wrist motions directly correspond to the movement of the player icon in the game, in the same manner that a mouse would affect the cursor.
- **Mobile Node 2 (PROMETHEUS-P2)** — a second identical mobile node, also sampling a LSM6DSL gyroscope and advertising over BLE NUS as a peripheral.
- **Base Node (M5Stack Core2)** — receives the gyroscope data from both mobile nodes, applies a Kalman filter to improve accuracy and reduce instability, then sends the filtered data to the PC via a USB serial connection using JSON encoding. It is received by a Python bridge and translated into player movement on an HTML canvas game rendered in the browser.
- **Camera Node (ESP32-S3-EYE)** — provides camera-based game controls using colour detection. The camera is mounted facing the player, who can hold up coloured cards to perform in-game actions (e.g. red = start, orange = pause, purple = restart). The camera communicates directly with the PC software via Wi-Fi (MQTT).

Our chosen actuator is the M5Stack Core2's built-in speaker on the base node, which plays sound effects such as hit sounds triggered by game events relayed from the PC over USB serial.

Overall, this project demonstrates sensor fusion, embedded real-time processing, multi-protocol wireless networking, and IoT principles, all developed within the Zephyr RTOS environment.

---

## Deliverables and KPIs

### Deliverable 1 — Wireless Handheld Device (IMU Transmission)
- 50–100 Hz packet transmission rate
- Less than 5% packet loss
- Reconnection time of approx. 3 seconds or less

### Deliverable 2 — Kalman Filtered Motion Cursor (IMU Sensor Fusion)
- < 10 px cursor jitter
- < 20 px drift per minute
- Stable filtered output at minimum 50 Hz

### Deliverable 3 — Camera-Based Gesture Detection
- Recognises at least 3 distinct gestures (e.g. start, pause, menu)
- Minimum 85% recognition accuracy
- < 500 ms gesture response time

### Deliverable 4 — Web Game Interface (HTML5)
- Minimum 30 FPS rendering
- Maximum 100 ms input response time

### Deliverable 5 — Telemetry and Debug Dashboard
- Minimum 10 Hz telemetry refresh
- Live gesture states
- Displays received and sent data packets with accurate timestamps

---

## System Architecture

### Message Protocol

| Step | From | To | Protocol | Payload |
|------|------|----|----------|---------|
| 1 | Mobile Node 1 | Base Node | BLE NUS | `{type: "imu", seq, gyro_y, gyro_z, t}` |
| 2 | Mobile Node 2 | Base Node | BLE NUS | `{type: "imu", seq, gyro_y, gyro_z, t}` |
| 3 | Base Node | PC Backend | Serial JSON | `{type: "imu", seq, gyro_y, gyro_z, t}` |
| 4 | PC Backend | Web Game | WebSocket | `{type: "state", player, eagles, score}` |
| 5 | Camera Node | PC Backend | MQTT (Mosquitto) | `{type: "gesture", gesture: "pause", confidence: 0.92}` |
| 6 | PC Backend | Base Node | Serial JSON | `{type: "audio", event: "eagle_killed"}` |
| 7 | Base Node | Speaker | I2S | Audio stream |
| 1.2 | Mobile Node | Base Node | BLE NUS | Re-advertise if connection lost |

---

## Sensor Integration

### LSM6DSL Accelerometer & Gyrometer (Mobile Nodes)
The gyroscope detects player movement by sampling angular velocity on the Y and Z axes (radians/s). The X axis is not used as the game is 2D. When facing the PC monitor, the user moves their wrist vertically and horizontally to control the player icon. Accelerometer data is not required. Data is sent to the base node via BLE NUS. Both mobile nodes run identical firmware, distinguished by their BLE device names (`PROMETHEUS-P1` and `PROMETHEUS-P2`).

### ESP32-S3-EYE Camera (Camera Node)
The camera performs on-device colour detection to trigger in-game events. Since only simple colour thresholds are needed (not full gesture recognition), the ESP32 processes frames locally without OpenCV, and transmits compact JSON messages to the PC via MQTT (e.g. `{"gesture": "restart"}`).

---

## Wireless Network Communication & IoT Protocols

1. **Mobile Nodes → Base Node via BLE NUS**
   - Provides stable connection with fast reconnection (~3s)
   - Mobile nodes advertise as peripherals; base node acts as central, maintaining up to 2 simultaneous connections
   - Avoids broadcasting movement data unnecessarily

2. **Base Node → PC via USB Serial (JSON)**
   - Python backend handles serial comms, MQTT subscription, and WebSocket to game
   - Wired connection minimises latency for continuous movement streaming
   - More stable with lower packet drop rate
   - PC also sends sound trigger commands back to base node

3. **Base Node → M5Stack Speaker via I²S**
   - Wired connection for in-game audio using the M5Stack Core2's built-in speaker
   - Sound effects stored as PCM arrays, played when triggered by PC

4. **Camera Node → PC via MQTT (Wi-Fi)**
   - Low data rate; only sends event labels, not video feed
   - Camera remains portable/untethered
   - Broker: Eclipse Mosquitto (lightweight, open-source)
   - ESP32 publishes; Python backend subscribes to `gesture` topics

---

## DIKW Pyramid

| Layer | Oxblood-Prometheus |
|-------|-------------------|
| **Data** | Raw gyroscope Y/Z angular velocity samples; BLE packet sequence numbers and timestamps; raw pixel values/colour counts from camera; game engine timestamps and state updates; audio event codes |
| **Information** | Filtered player movement vector; detected card colour (Blue, Red, Yellow, Green); player position, velocity, and score; eagle positions and count; packet loss %, latency, frame rate |
| **Knowledge** | Determine player movement intention from filtered IMU; recognise colour gestures as game commands; collision = eagle destroyed; increasing eagle speed/spawn rate = higher difficulty; eagles reaching Prometheus = game over; high packet loss = degraded responsiveness |
| **Wisdom** | Update score after eagle collisions; trigger audio feedback on kills; pause/resume on colour card; use telemetry metrics to evaluate KPIs |

---

## Example Scenario

1. The player tilts their wrist. The mobile node reads raw gyroscope data.
2. The mobile node filters the data and sends it via BLE. The base node forwards it to the PC, where it is interpreted into a movement vector and updates the game state.
3. The camera detects an orange card and publishes a `pause` command via MQTT. The PC receives it and pauses the game.
4. Player holds up an orange card again, and the PC resumes the game.
5. If the player collides with an eagle, the PC increases the score and sends an audio event to the base node to play a `hit` sound through the M5Stack speaker.
6. Telemetry (packet loss, latency, FPS, score) is shown on a dashboard for evaluation.

---

## Project Timeline (Gantt)

| Task | Target Date |
|------|-------------|
| Mobile Nodes (BLE NUS + IMU) | May 17 |
| Base Node (BLE central, I2S) | May 17 |
| Camera Node (Colour detection) | May 17 |
| PC Backend (MQTT, WebSocket) | May 19 |
| Web Game (HTML5 Canvas) | May 19 |
| Telemetry Dashboard | May 22 |
| System Integration and Testing | May 22 |
| Final Demo Preparation | May 25 |

---

## Zephyr RTOS Libraries Used

| Library | Header | Purpose |
|---------|--------|---------|
| Bluetooth | `<zephyr/bluetooth/bluetooth.h>` | Manages BLE connections; mobile nodes = peripheral, base = central (up to 2 connections) |
| Bluetooth NUS | `<zephyr/bluetooth/services/nus.h>` | Streams JSON-encoded gyroscope readings via BLE notifications |
| Zephyr Kernel API | `<zephyr/kernel.h>` | Threads and message queues |
| Zephyr Device API | `<zephyr/device.h>` | Obtain and verify hardware peripheral handles |
| Zephyr Sensor API | `<zephyr/drivers/sensor.h>` | Interfaces with LSM6DSL over SPI; samples gyroscope via `sensor_sample_fetch` / `sensor_channel_get` |
| Zephyr Video API | `<zephyr/drivers/video.h>` | Captures frames on ESP32-S3-EYE for on-device colour detection |
| I2S | `<zephyr/drivers/i2s.h>` | Streams PCM audio to M5Stack built-in speaker on the base node |
| MQTT | — | Used on ESP32-S3-EYE to send colour detection results to PC |

---

## Equipment

- 2× Seeed XIAO nRF52840 (Mobile Nodes)
- 1× M5Stack Core2 ESP32 (Base Node)
- 1× ESP32-S3-EYE Camera
- 1× PC

## User Guide
Ensure Mosquitto broker is running on your device. 

Connect base node (M5 COre2) to the USB serial port of your PC. 

Run liver_backend.py, and follow the http link printed on the terminal to a web browser. C

onnect the two mobile nodes to a power source, and connect camera to power source. 

Hold up red card to camera, ensuring it takes up majority of the screen. Enjoy the game.