# KAVACH v2.0 — Intelligent Vehicle Safety System

> Detect. Alert. Respond. Automatically.

**Team COREsentinel** — Anwesha Mohanty · Ariana Mohanty · Ashutosh Mishra  
KIITFest 9.0 "Hack the IoT" Hackathon | OUTR, Bhubaneswar | 2026

---

## The Problem
India recorded 1.8 lakh road deaths in 2024. No Indian public bus has a real-time,
multi-parameter safety system. KAVACH addresses driver fatigue, gas leaks,
collisions, and crash alerts — all in one device under ₹1500.

---

## What it does
An ESP32-based device that scans 7 sensors every 500ms and responds automatically:

| Alert Level | Trigger | Response |
|---|---|---|
| Level 1 — Caution | Single sensor warning | Beep + Green LED |
| Level 2 — Warning | Two-sensor combo / CO / Microsleep | Double beep + Yellow LED |
| Level 3 — Critical | Fire risk / Collision / Crash | Continuous alarm + Red LED + Engine cutoff |
| Level 4 — Emergency | No human response for 5 seconds | Wi-Fi SOS sent with Bus ID |

---

## Sensors used
ESP32 · DHT11 (temp/humidity) · MQ-7/MQ-135 (gas) · PIR (driver presence) ·  
HC-SR04 ultrasonic (obstacle) · Vibration (crash) · IR (door/blind spot) · Sound (microsleep)

---

## Files
- `KAVACH_ESP32_HARDWARE.ino` — Full firmware
- `KAVACH_Presentation.pptx` — Project presentation

---

## Circuit (Key pin connections)
| GPIO | Connected to |
|---|---|
| 4 | DHT11 DATA |
| 5 / 18 | HC-SR04 TRIG / ECHO |
| 19 | PIR OUT |
| 34 | MQ-7/MQ-135 AO |
| 35 | Vibration DO |
| 32 | Sound DO |
| 33 | IR OUT |
| 13 | Buzzer |
| 25 / 12 / 27 | LED Green / Yellow / Red |
| 26 | Relay (ignition cutoff) |
| 21 / 22 | LCD I2C SDA / SCL |
