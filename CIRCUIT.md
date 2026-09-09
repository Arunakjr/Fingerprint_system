<div align="center">

# 🫆 FingerAttend

### Fingerprint-Based Smart Attendance System

![ESP8266](https://img.shields.io/badge/ESP8266-NodeMCU-blue?style=for-the-badge&logo=arduino)
![Platform](https://img.shields.io/badge/Platform-Arduino-teal?style=for-the-badge&logo=arduino)
![Cloud](https://img.shields.io/badge/Cloud-Google%20Sheets-green?style=for-the-badge&logo=googlesheets)
![License](https://img.shields.io/badge/License-MIT-purple?style=for-the-badge)

*Mark attendance with a fingerprint. Logs instantly to Google Sheets.*
*Manage everything from a web dashboard on your phone.*

</div>

---

##  Features

- 🫆 **Fingerprint scanning** — R307 optical sensor, stores up to 127 prints
- 📊 **Google Sheets logging** — real-time attendance via Apps Script webhook
- 🌐 **Local web dashboard** — enroll, remove and view logs from any browser
- 🖥️ **OLED display** — live status, animations and scan feedback
- 🔔 **Buzzer feedback** — distinct tones for success, denied and enroll
- 👻 **Ghost-read protection** — triple-read validation + confidence threshold
- 📡 **WiFi hosted** — ESP8266 runs its own web server on port 80

---

##  Components Required

| # | Component |
|---|-----------|
| 1 | ESP8266 NodeMCU v1.0 |
| 2 | SSD1306 OLED 128×64 I2C (0.96") |
| 3 | R307 / R503 Fingerprint Sensor |
| 4 | Passive Buzzer |
| 5 | Breadboard |
| 6 | Jumper wires |
| 7 | USB cable (Micro USB) |

---

##  Wiring

### SSD1306 OLED → NodeMCU

| OLED Pin | NodeMCU Pin | Notes |
|----------|-------------|-------|
| VCC | 3.3V | 3.3V only — not 5V |
| GND | GND | |
| SCL | D1 | I2C clock |
| SDA | D2 | I2C data |

> I2C address: `0x3C` · Initialized with `Wire.begin(D2, D1)`

---

### R307 Fingerprint Sensor → NodeMCU

| Sensor Pin | NodeMCU Pin | Wire | Notes |
|------------|-------------|------|-------|
| VCC | VIN | 🔴 Red | Must be 5V — use VIN not 3.3V |
| GND | GND | ⚫ Black | |
| TX | D5 | 🟡 Yellow | Sensor TX → MCU reads |
| RX | D6 | 🟢 Green | Sensor RX → MCU writes |

> ⚠️ Cross-connect: Sensor TX → D5 and Sensor RX → D6

---

### Passive Buzzer → NodeMCU

| Buzzer Pin | NodeMCU Pin | Notes |
|------------|-------------|-------|
| + | D3 | PWM signal via `tone()` |
| − | GND | |

> ⚠️ Must be a **passive** buzzer — active buzzers won't work with `tone()`

---

### Full Wiring at a Glance

| NodeMCU | Component |
|---------|-----------|
| 3.3V | OLED VCC |
| GND | OLED GND |
| D1 | OLED SCL |
| D2 | OLED SDA |
| D3 | Buzzer + |
| GND | Buzzer − |
| VIN | Fingerprint VCC |
| GND | Fingerprint GND |
| D5 | Fingerprint TX |
| D6 | Fingerprint RX |

---

### Schematic

```
                    ┌─────────────────────┐
                    │   NodeMCU ESP8266   │
                    │                     │
  ┌──────────┐      │  3.3V ──────────────┼──→ OLED VCC
  │SSD1306   │      │  GND  ──────────────┼──→ OLED GND
  │OLED      │◄─────┤  D1   ──────────────┼──→ OLED SCL
  │128×64    │      │  D2   ──────────────┼──→ OLED SDA
  └──────────┘      │                     │
                    │  D3   ──────────────┼──→ Buzzer +
  ┌──────────┐      │  GND  ──────────────┼──→ Buzzer −
  │Passive   │◄─────┤                     │
  │Buzzer    │      │  VIN  ──────────────┼──→ FP VCC (5V)
  └──────────┘      │  GND  ──────────────┼──→ FP GND
                    │  D5   ◄─────────────┼──  FP TX
  ┌──────────┐      │  D6   ──────────────┼──→ FP RX
  │R307      │◄─────┤                     │
  │Fingerprint│     └─────────────────────┘
  └──────────┘                 │
                             USB cable
                          (power + upload)
```

### Power Notes

```
USB 5V via cable
  └── NodeMCU onboard regulator
        ├── 3.3V  ──→  OLED
        └── VIN   ──→  Fingerprint sensor (needs full 5V)
```

---

##  Setup

### Step 1 — Arduino IDE Board Setup

Add this URL in **File → Preferences → Additional boards manager URLs:**

```
http://arduino.esp8266.com/stable/package_esp8266com_index.json
```

Then go to **Tools → Board → Boards Manager**, search `ESP8266` and install.

Board settings in **Tools:**

```
Board        : NodeMCU 1.0 (ESP-12E Module)
CPU Freq     : 80 MHz
Flash Size   : 4MB (FS:2MB OTA:~1019KB)
Upload Speed : 115200
```

---

### Step 2 — Install Libraries

Go to **Sketch → Include Library → Manage Libraries** and install:

```
Adafruit SSD1306
Adafruit GFX Library
Adafruit Fingerprint Sensor Library
```

> `ESP8266WiFi`, `ESP8266HTTPClient` and `ESP8266WebServer` come built-in with the board package.

---

### Step 3 — WiFi Hotspot

Enable a mobile hotspot on your phone:

```
SSID     : pwm
Password : 12345678
```

> To use a different network change these lines in the sketch:
> ```cpp
> const char* ssid     = "pwm";
> const char* password = "12345678";
> ```

---

### Step 4 — Google Sheets Setup

#### 4a — Create the spreadsheet

1. Go to [sheets.google.com](https://sheets.google.com)
2. Create a new spreadsheet and name it `FingerAttend`

#### 4b — Add the Apps Script

1. Inside the spreadsheet click **Extensions → Apps Script**
2. Delete any existing code and paste this:

```javascript
function doPost(e) {
  var sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();
  var data = JSON.parse(e.postData.contents);
  sheet.appendRow([new Date(), data.id, data.name, data.status]);
  return ContentService.createTextOutput("OK");
}

function doGet(e) {
  var sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();
  sheet.appendRow([
    new Date(),
    e.parameter.id,
    e.parameter.name,
    e.parameter.status
  ]);
  return ContentService.createTextOutput("OK");
}
```

3. Click **Save**

#### 4c — Deploy as Web App

1. Click **Deploy → New Deployment**
2. Click the gear icon next to Type → select **Web App**
3. Set:
   - Execute as: **Me**
   - Who has access: **Anyone**
4. Click **Deploy → Authorize access** and allow permissions
5. Copy the **Web App URL** — it looks like:

```
https://script.google.com/macros/s/AKfycb.........../exec
```

#### 4d — Paste URL into sketch

Find this line in the sketch and replace with your URL:

```cpp
const char* scriptURL = "https://script.google.com/macros/s/YOUR_URL_HERE/exec";
```

#### 4e — Test it

Paste this in your browser (replace with your URL):

```
https://script.google.com/macros/s/YOUR_URL_HERE/exec?id=1&name=Test&status=Present
```

A new row should appear in your spreadsheet instantly. If it does — you are ready.

---

### Step 5 — Add Names

Edit `getNameByID()` in the sketch to match your enrolled fingerprint IDs:

```cpp
String getNameByID(int id) {
  switch (id) {
    case 1: return "Alice";
    case 2: return "Bob";
    case 3: return "Charlie";
    // add more...
    default: return "User_" + String(id);
  }
}
```

---

### Step 6 — Upload and Run

1. Connect NodeMCU via USB cable
2. Select the correct COM port in **Tools → Port**
3. Click **Upload**
4. Open **Serial Monitor** at `115200` baud
5. Turn on hotspot `pwm`
6. The IP address will show on the OLED and in Serial Monitor
7. Open that IP in any browser connected to the hotspot

---

##  Web Dashboard

Connect any device to hotspot `pwm` and open the IP shown on the OLED in a browser.

| Feature | Description |
|---------|-------------|
| Overview | Total enrolled IDs and today's log count |
| Enroll | Enter ID + Name → click Enroll → place finger on sensor |
| Remove | Remove any fingerprint by ID or click Remove next to it in the list |
| Stored IDs | Live list of all enrolled fingerprints with names |
| Attendance log | Every scan with name, ID, time and status |
| Auto-refresh | Stats and mode update every 3 seconds automatically |

---

##  Serial Commands

Open Serial Monitor at **115200 baud:**

| Command | Action |
|---------|--------|
| `A` | Enter enroll mode |
| `#5` | Enroll fingerprint as ID 5 |
| `#12` | Enroll fingerprint as ID 12 |
| `R#5` | Delete fingerprint ID 5 |
| `R#12` | Delete fingerprint ID 12 |
| `L` | List all stored IDs |

---

##  Attendance Sheet Output

| Timestamp | ID | Name | Status |
|-----------|----|------|--------|
| 6/14/2026 09:00:01 | 1 | Alice | Present |
| 6/14/2026 09:02:45 | 2 | Bob | Present |
| 6/14/2026 09:05:12 | 3 | Charlie | Present |

---

##  Troubleshooting

| Problem | Fix |
|---------|-----|
| OLED blank | Try I2C address `0x3D` instead of `0x3C` in code |
| OLED garbled | Swap D1 and D2 jumpers |
| Fingerprint not found | Check VCC is on VIN not 3.3V |
| Ghost reads | Already fixed — triple-read + confidence filter built in |
| Buzzer silent | Make sure it is a passive buzzer not active |
| WiFi not connecting | Turn on hotspot SSID `pwm` · Password `12345678` |
| Sheets not logging | Re-deploy Apps Script and paste the new URL in code |
| Dashboard not loading | Make sure your device is connected to hotspot `pwm` |
| HTTP error on POST | `client.setInsecure()` already set — check script URL is correct |

---

##  Built With

- [Arduino ESP8266 Core](https://github.com/esp8266/Arduino)
- [Adafruit SSD1306](https://github.com/adafruit/Adafruit_SSD1306)
- [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library)
- [Adafruit Fingerprint Sensor Library](https://github.com/adafruit/Adafruit-Fingerprint-Sensor-Library)
- [Google Apps Script](https://developers.google.com/apps-script)

---

##  License

MIT License — free to use, modify and distribute.

---

<div align="center">

Made with — FingerAttend Open Source Project

*Hardware: ESP8266 · Software: Arduino C++ · Cloud: Google Sheets*

</div>
