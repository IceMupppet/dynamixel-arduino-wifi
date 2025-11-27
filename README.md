# Dynamixel AX-12A Web Control Panel  
### UNO R4 WiFi + Dynamixel Shield (Robotis) — Full Setup & Firmware Guide

This project provides a **full-featured browser-based control panel** for a Dynamixel **AX-12A** servo connected through an **Arduino UNO R4 WiFi** and the **Robotis Dynamixel Shield**.

The controller serves a local webpage that allows:

- Torque Enable / Disable (toggle switch)  
- LED On / Off (toggle switch)  
- Goal Position control via a **0–300° slider**  
- Center button (150° / raw ≈ 512)  
- Velocity (MOVING_SPEED) slider  
- Live telemetry pulled from the servo:  
  - Position (° + raw 0–1023)  
  - Speed  
  - Load  
  - Temperature  
  - Voltage  
- Servo picture + ID label  
- Clean OBSIDIA-style dark UI  

---

## Hardware Required

- Arduino **UNO R4 WiFi**  
- **Dynamixel Shield** (Robotis)  
- **Dynamixel AX-12A** (or AX-12+; any AX works)  
- **12 V Power Supply** (recommended 12 V / ≥ 2 A)  
- Dynamixel **3-pin TTL cable**  
- (Optional) Custom Molex ↔ JST cable if mixing connector styles  
- USB-C cable for programming the UNO R4  

---

## ⚡ Power Wiring & Safety Notes

### 1. Do NOT power the servo from the UNO R4 USB port

Dynamixel servos must **never** draw power through USB.

### 2. Power the servo using 12 V into the Dynamixel Shield terminal block

```text
Dynamixel Shield
+12V ----> + terminal
GND  ----> - terminal
```

### 3. REMOVE the VIN JUMPER from the Dynamixel Shield

This is critical.

The VIN jumper bridges the servo power back into the Arduino’s VIN pin.  
If you leave it installed:

- The shield will *backfeed power* into the UNO  
- The UNO will turn on even when the shield is “off”  
- USB enumeration can fail  
- Programming becomes unreliable  
- There is a higher risk of damaging the board

**Remove this jumper:**

- It’s labeled **VIN** on the shield  
- Pull it off completely or park it on *one pin only*

With the VIN jumper removed:

| Power Source        | Affects       |
|---------------------|---------------|
| 12 V on shield      | **Servos only** |
| USB-C on UNO        | **UNO only**    |

This is the correct and safe configuration.

### 4. Power Switch Behavior

The Dynamixel Shield has a small power switch. This only toggles **servo power**, not Arduino power.

- With VIN jumper **removed**, switching the shield *off* will stop servo power, but the UNO stays powered from USB.  
- With VIN jumper **installed** (not recommended), the UNO will be powered from shield 12 V regardless of switch position.

---

## Dynamixel AX-12A Wiring

- Connect the AX-12A to the **Dynamixel Shield TTL port (3-pin)**  
- Signal pin goes toward the **inside** of the board  
- The servo must be powered from the shield’s 12 V input  

If your servo has the other style connector (Molex), splice or use a hybrid cable:  
**Signal = Yellow**, **Power = Red**, **Ground = Black**.

---

## Firmware Behavior

When uploaded, the UNO R4:

1. Connects to Wi-Fi (SSID: **SASSY**, Password: **mrssneaks7**)  
2. Initializes the Dynamixel bus at **1,000,000 bps**  
3. Forces the servo into **Joint Mode** (0–1023 range) by restoring CW/CCW angle limits:  
   - `CW_ANGLE_LIMIT = 0`  
   - `CCW_ANGLE_LIMIT = 1023`  
4. Sets initial MOVING_SPEED from a stored variable  
5. Reads present position and uses it as the default  
6. Turns torque **ON** and LED **OFF**  
7. Starts a web server on port 80  
8. Logs the IP address to Serial Monitor  

---

## How to Use the Web Interface

1. Open the Arduino IDE Serial Monitor after upload.  
2. Find the printed IP address, e.g.:

```text
WiFi connected. IP = 10.0.0.232
```

3. Open a browser and go to:

```text
http://10.0.0.232
```

You will see an advanced control panel with:

- Servo image  
- Live telemetry  
- Toggles  
- Sliders  
- Buttons  

All changes update the servo registers immediately.

---

## Dynamixel Control Features

### Torque Toggle

Uses:

```cpp
dxl.torqueOn(DXL_ID);
dxl.torqueOff(DXL_ID);
```

### LED Toggle

Uses:

```cpp
dxl.ledOn(DXL_ID);
dxl.ledOff(DXL_ID);
```

### Goal Position (Degrees + Raw)

The interface uses a **0–300°** slider and converts to raw units:

```cpp
raw = deg * 1023 / 300;
dxl.writeControlTableItem(GOAL_POSITION, DXL_ID, raw);
```

- Current degree and raw values are shown together.  
- A **Center** button instantly moves the servo to 150° (raw ≈ 512).

### Velocity Slider

Controls traditional AX-12A `MOVING_SPEED`:

- Range: **0–1023** (higher = faster)

### Live Status Reads

On each page load, these values are read from the servo and displayed in a table:

- `PRESENT_POSITION` (° + raw)  
- `PRESENT_SPEED`  
- `PRESENT_LOAD`  
- `PRESENT_TEMPERATURE`  
- `PRESENT_VOLTAGE`  
- `MOVING_SPEED` (current velocity setting)

This gives a compact “mini Dynamixel Wizard” view in the browser.

---

## Changing the Servo ID

Default ID is **2**.  
To change the ID used by the firmware, edit:

```cpp
const uint8_t DXL_ID = 2;
```

You must also make sure your servo is configured to the same ID (e.g., using Dynamixel Wizard or a separate configuration sketch).

---

## Network Configuration

Edit these lines to match your Wi-Fi:

```cpp
char ssid[] = "SASSY";
char pass[] = "mrssneaks7";
```

The device IP is printed to Serial on boot and shown at the bottom of the web UI.

---

## Troubleshooting

### UNO doesn’t show up in Arduino IDE

- Make sure the **VIN jumper is removed** from the Dynamixel Shield.  
- Check that the UNO is powered by USB-C, not accidentally backfed from 12 V.  
- Ensure a proper data-capable USB-C cable is used.  
- Confirm the correct board is selected: **Arduino UNO R4 WiFi**.

### Servo does not move

- Confirm 12 V is connected to the shield terminal block.  
- Verify the shield power switch is ON.  
- Make sure torque is **enabled** via the web UI.  
- Check that the servo ID matches `DXL_ID`.  
- Inspect cabling and connectors.

### Servo spins continuously

Your servo was in **Wheel Mode**, not Joint Mode.  
The firmware forces Joint Mode by restoring the CW/CCW angle limits, but if behavior persists, re-check using Dynamixel Wizard or a dedicated configuration sketch.

---

## Project Purpose

This system functions as a **portable servo control panel**, similar to the official Dynamixel Wizard, but:

- Runs entirely on **UNO R4 WiFi + Web UI**  
- Is easy to embed into larger robotics or test setups  
- Matches the visual style used in OBSIDIA’s control interfaces
