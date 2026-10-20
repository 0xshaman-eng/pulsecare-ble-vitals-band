# PulseCare - Medical Care & Vitality
**"Peace of Mind, Every Beat"**

PulseCare is a compact, wrist-worn device designed to continuously track vital signs (Heart Rate and SpO2) and bridge the gap between patients and guardians via BLE. Built for a university semester project, it captures precautionary data and features an emergency SOS system to ensure high-risk patients are never disconnected from medical oversight.

## Project Team
* E/20/418 - Wahalathantri TN 
* E/20/192 - Karunarathne AGSI 
* E/20/440 - Wickramsinghe RT 

## Hardware Architecture
The device is powered by a low-cost, efficient hardware stack:
* **Microcontroller:** ESP32-C3 Super Mini
* **Sensor:** MAX30100 Pulse Oximeter & Heart Rate Module
* **Display:** 0.96" OLED (128x64)
* **Power:** 3.7 V Li-Po ion battery with a TP4056 Charger IC 
* **Inputs:** Push button for screen toggling and SOS activation 

## Software & Technologies
* **Device Firmware:** C++ (Arduino IDE) utilizing `BLEDevice.h` and standard I2C communication.
* **Mobile Application:** Flutter/Dart (Android).
* **Backend:** Cloud Firestore for live data syncing and Guardian alerts.

## Repository Contents
* `/Code`: Contains the complete ESP32-C3 Arduino source code, including BLE setup, OLED rendering, and MAX30100 data processing (with a simulation mode fallback).
* `/Documentation`: Contains the complete project presentation and report (`PulseCare.pdf`), detailing the white-box analysis, schematics, and clinical thresholds (e.g., Hypoxia warnings for SpO2 below 90% ).

## Mobile Application (APK Download)
> **Note:** The source code for the Flutter application was unfortunately lost due to a local hardware failure. 

However, the final, fully compiled Android Application is available for testing! 
* You can download the **`app-release.apk`** directly from the **[Releases](../../releases)** tab on the right side of this repository. 
* Install this APK on any Android device to experience the Patient and Guardian UI, which connects to the smart gadget via Bluetooth Low Energy (BLE).

## Key Features
1. **Continuous Monitoring:** Uses Photoplethysmography (PPG) to track real-time BPM and Blood Oxygen.
2. **Emergency SOS:** A long-press (2000ms) on the device button instantly pushes an SOS flag to Firebase via the connected phone, immediately alerting the guardian.
3. **Smart Power Management:** Enters a simulated data phase if the sensor is disconnected to prevent system freezing. 
