<div align="center">

# FingerAttend

### Fingerprint-Based Attendance System using ESP8266 and Google Sheets

A compact IoT attendance system that identifies users with an R307/R503-compatible fingerprint sensor, gives feedback on an SSD1306 OLED and buzzer, and records successful attendance scans in Google Sheets through a Google Apps Script web app.

</div>

---

## Features

- Fingerprint-based user identification
- Supports fingerprint IDs `1` to `127`
- Google Sheets attendance logging over Wi-Fi
- OLED status messages and scan animations
- Buzzer feedback for startup, scans, success, failure, and enrollment
- Fingerprint enrollment and deletion through Serial Monitor commands
- Stored fingerprint ID listing through Serial Monitor
- Ghost-read protection using repeated image checks and a confidence threshold
- Automatic Wi-Fi reconnection before sending attendance
- Default names mapped to fingerprint IDs in the ESP8266 sketch

---

## System Flow

```text
User places finger
        |
        v
R307/R503 Fingerprint Sensor
        |
        v
ESP8266 NodeMCU
   |          |
   |          +----> OLED + Buzzer feedback
   |
   +---- Wi-Fi ----> Google Apps Script ----> Google Sheets
```

The included block diagram is available as:

`fingerattend_block_diagram_16x9.png`

---

## Hardware Required

| Component | Purpose |
|---|---|
| ESP8266 NodeMCU v1.0 | Main controller and Wi-Fi connectivity |
| R307 / R503 fingerprint sensor | Fingerprint enrollment and matching |
| SSD1306 128x64 I2C OLED | User interface and status display |
| Passive buzzer | Audio feedback |
| Breadboard | Prototyping |
| Jumper wires | Connections |
| Micro-USB cable | Power and programming |

---

## Wiring

### SSD1306 OLED to NodeMCU

| OLED | NodeMCU | Description |
|---|---|---|
| VCC | 3.3V | OLED power |
| GND | GND | Ground |
| SCL | D1 | I2C clock |
| SDA | D2 | I2C data |

The sketch initializes I2C with:

```cpp
Wire.begin(D2, D1);
```

The OLED address used by the project is `0x3C`.

### Fingerprint Sensor to NodeMCU

| Fingerprint Sensor | NodeMCU | Description |
|---|---|---|
| VCC | VIN | 5 V supply from USB/VIN |
| GND | GND | Ground |
| TX | D5 | Sensor TX -> ESP8266 RX |
| RX | D6 | Sensor RX <- ESP8266 TX |

The fingerprint sensor serial connection runs at `57600` baud.

### Passive Buzzer to NodeMCU

| Buzzer | NodeMCU |
|---|---|
| + | D3 |
| - | GND |

> Use a **passive buzzer** because the sketch generates tones with `tone()`.

---

## Software Requirements

### Arduino IDE

Install the ESP8266 board package in Arduino IDE.

Add the following Boards Manager URL under **File -> Preferences -> Additional Boards Manager URLs**:

```text
http://arduino.esp8266.com/stable/package_esp8266com_index.json
```

Then install **ESP8266 by ESP8266 Community** from Boards Manager.

Recommended board selection:

```text
NodeMCU 1.0 (ESP-12E Module)
```

### Arduino Libraries

Install these libraries from Arduino Library Manager:

- Adafruit GFX Library
- Adafruit SSD1306
- Adafruit Fingerprint Sensor Library

The ESP8266 board package supplies the Wi-Fi and HTTP libraries used by the sketch:

- `ESP8266WiFi`
- `ESP8266HTTPClient`
- `WiFiClientSecure`

`Wire` and `SoftwareSerial` are also used by the project.

---

## Project Files

```text
FingerPrint-attendance-main/
├── ESP8266 code
├── Google Script code
├── CIRCUIT.md
└── fingerattend_block_diagram_16x9.png
```

| File | Description |
|---|---|
| `ESP8266 code` | Main Arduino/ESP8266 fingerprint attendance sketch |
| `Google Script code` | Google Apps Script endpoint that appends attendance rows |
| `CIRCUIT.md` | Wiring and project documentation |
| `fingerattend_block_diagram_16x9.png` | System block diagram |

> For easier use in Arduino IDE, you may rename `ESP8266 code` to something such as `FingerAttend.ino`. The sketch folder should then also be named `FingerAttend`.

---

## Google Sheets Setup

### 1. Create a spreadsheet

Create a Google Sheet for attendance records. The Apps Script will append rows in this format:

| Timestamp | ID | Name | Status |
|---|---|---|---|
| Auto-generated | Fingerprint ID | User name | Present |

You can optionally add the following header row manually:

```text
Timestamp | ID | Name | Status
```

### 2. Add the Apps Script

Open the spreadsheet and go to:

**Extensions -> Apps Script**

Paste the contents of the included `Google Script code` file:

```javascript
function doPost(e) {
  var sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();
  var data = JSON.parse(e.postData.contents);
  sheet.appendRow([
    new Date(),
    data.id,
    data.name,
    data.status
  ]);
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
  return ContentService.createTextOutput("OK - Row Added");
}
```

Save the script.

### 3. Deploy as a Web App

In Apps Script:

1. Select **Deploy -> New deployment**.
2. Choose **Web app**.
3. Set **Execute as** to yourself.
4. Configure access so the ESP8266 can call the endpoint.
5. Authorize the requested permissions.
6. Copy the deployed Web App URL ending in `/exec`.

Example format:

```text
https://script.google.com/macros/s/YOUR_DEPLOYMENT_ID/exec
```

### 4. Configure the ESP8266 sketch

Replace the placeholder URL:

```cpp
const char* scriptURL = "SCRIPT_URL";
```

with your deployed Apps Script URL:

```cpp
const char* scriptURL = "https://script.google.com/macros/s/YOUR_DEPLOYMENT_ID/exec";
```

---

## Wi-Fi Configuration

The uploaded sketch currently contains:

```cpp
const char* ssid     = "pwm";
const char* password = "12345678";
```

Change these values to your Wi-Fi network or mobile hotspot credentials before uploading if required.

> Do not publish real Wi-Fi passwords in a public repository. For a public project, replace credentials with placeholders before committing the sketch.

---

## User Name Mapping

The project associates fingerprint IDs with names in `getNameByID()`:

```cpp
String getNameByID(int id) {
  switch (id) {
    case 2: return "Bob";
    case 3: return "Charlie";
    // Add your own IDs here.
    default: return "User_" + String(id);
  }
}
```

Edit this function to match your enrolled users.

For example:

```cpp
case 1: return "Alice";
case 2: return "Bob";
case 3: return "Charlie";
```

---

## Uploading the ESP8266 Code

1. Connect the NodeMCU to your computer over USB.
2. Open the ESP8266 sketch in Arduino IDE.
3. Select **NodeMCU 1.0 (ESP-12E Module)**.
4. Select the correct COM/serial port.
5. Verify that the Wi-Fi credentials and `scriptURL` are configured.
6. Compile the sketch.
7. Upload it to the board.
8. Open Serial Monitor at **115200 baud**.

At startup, the ESP8266 will:

1. Initialize the OLED and buzzer.
2. Connect to Wi-Fi.
3. Initialize the fingerprint sensor at `57600` baud.
4. Display the number of stored fingerprint templates.
5. Wait for a fingerprint scan or a Serial Monitor command.

---

## Serial Monitor Commands

Set Serial Monitor to **115200 baud** and send commands terminated by a newline.

| Command | Action |
|---|---|
| `A` | Enter enrollment mode and wait for an ID |
| `#5` | Enroll a fingerprint as ID `5` |
| `#12` | Enroll a fingerprint as ID `12` |
| `R#5` | Delete fingerprint ID `5` |
| `R#12` | Delete fingerprint ID `12` |
| `L` | List all stored fingerprint IDs |

Valid fingerprint IDs are from `1` to `127`.

### Enroll a Fingerprint

You can enroll directly with an ID:

```text
#5
```

Then:

1. Place the finger when prompted.
2. Remove it after the first scan.
3. Place the same finger again.
4. The fingerprint template is stored as ID `5` if both scans match.

The enrollment operation times out after approximately 30 seconds per scan stage.

### Remove a Fingerprint

To remove ID `5`:

```text
R#5
```

### List Stored Fingerprints

Send:

```text
L
```

The complete list is printed to Serial Monitor.

---

## Attendance Operation

During normal operation:

1. The OLED asks the user to place a finger.
2. The sketch performs repeated reads to confirm that a real finger is present.
3. The fingerprint image is converted to a template.
4. The sensor searches its stored templates.
5. Matches with confidence below `50` are ignored.
6. On a valid match, the ID is converted to a user name using `getNameByID()`.
7. The ESP8266 sends a JSON POST request to Google Apps Script.
8. Google Apps Script appends the attendance entry to the active Google Sheet.
9. The OLED and buzzer report success or failure.

The JSON payload has this format:

```json
{
  "id": 5,
  "name": "Alice",
  "status": "Present"
}
```

---

## Ghost-Read Protection

The uploaded sketch contains several checks intended to reduce false fingerprint detections:

- The sensor must report a valid fingerprint image three consecutive times.
- A fresh image is captured after the confirmation stage.
- Matches below a confidence value of `50` are ignored.
- The code checks for finger removal after failed matches.
- The code waits for the finger to be lifted before accepting another scan.

You can tune the confidence threshold in this section:

```cpp
if (finger.confidence < 50) {
  return -1;
}
```

A higher value is stricter but may reject more genuine scans.

---

## Google Sheets Test

You can test the Apps Script endpoint from a browser because the included script supports HTTP GET requests.

Use a URL in this form:

```text
https://script.google.com/macros/s/YOUR_DEPLOYMENT_ID/exec?id=1&name=Test&status=Present
```

A successful request should append a new row to the spreadsheet.

---

## Troubleshooting

| Problem | Possible Fix |
|---|---|
| OLED remains blank | Check SDA/SCL wiring and verify the OLED address is `0x3C` |
| OLED initialization fails | Test whether your module uses `0x3D` instead of `0x3C` |
| Fingerprint sensor is not detected | Check VCC, GND, TX/RX wiring and confirm the sensor uses `57600` baud |
| Fingerprint never matches | Re-enroll the finger and verify that the stored ID exists |
| Valid finger is rejected | The confidence threshold may be too strict for your sensor/environment |
| False/ghost reads occur | Check sensor wiring/power and adjust the confidence threshold if necessary |
| Buzzer does not produce tones | Confirm that it is a passive buzzer and is connected to `D3` |
| Wi-Fi fails | Verify SSID/password and ensure a 2.4 GHz network is available for ESP8266 |
| HTTP logging fails | Verify the Apps Script deployment URL and its access permissions |
| Sheet receives no data | Test the Apps Script URL with a browser GET request first |
| HTTP certificate problems | The sketch uses `client.setInsecure()`; verify the URL and connectivity |

---

## Important Notes

- The current uploaded ESP8266 sketch **does not implement a local web dashboard or `ESP8266WebServer`**. Device management is performed through Serial Monitor commands.
- The Apps Script writes to the **active sheet** of the spreadsheet in which the script is installed.
- The ESP8266 sends attendance only for recognized fingerprints.
- Attendance status is currently hard-coded as `"Present"`.
- User names are stored in the firmware's `getNameByID()` function rather than in the fingerprint sensor.
- `WiFiClientSecure::setInsecure()` disables TLS certificate verification. This simplifies HTTPS communication on the ESP8266 but is not ideal for security-sensitive deployments.

---

## Possible Improvements

- Add a local web dashboard for enrollment and attendance viewing
- Store names in EEPROM/LittleFS instead of hard-coding them
- Add duplicate-attendance prevention for the same user/day
- Add check-in and check-out modes
- Add offline buffering when Wi-Fi is unavailable
- Add an RTC module for local timestamps
- Add an administrator authentication layer
- Move Wi-Fi and deployment configuration into a separate secrets/config file
- Add sheet headers automatically during initial setup

---

## Security Recommendations

Before publishing or deploying this project:

- Replace hard-coded Wi-Fi credentials.
- Do not commit real passwords or private deployment URLs to a public repository.
- Restrict access to the Google Sheet and Apps Script deployment as much as your deployment allows.
- Treat fingerprint templates and attendance records as sensitive personal data.
- Use the system only with appropriate user notice/consent and according to applicable privacy requirements.

---

## Built With

- ESP8266 / NodeMCU
- Arduino C++
- Adafruit Fingerprint Sensor Library
- Adafruit SSD1306
- Adafruit GFX
- Google Apps Script
- Google Sheets

---

## License

No explicit license file was included in the uploaded project archive. Add a `LICENSE` file before describing the repository as MIT, GPL, Apache, or another licensed open-source project.

---

<div align="center">

**FingerAttend - Fingerprint Attendance with ESP8266 and Google Sheets**

</div>
