#include <Arduino.h>       //core framework (pinMode, delay, Serial, tone/noTone, etc.)
#include <Wire.h>          //I2C driver
#include <WiFi.h>          //ESP32's station/AP Wi-Fi stac
#include <WiFiClientSecure.h>    //TLS-wrapped TCP client, needed because the Google Apps Script endpoint is https://.
#include <HTTPClient.h>          //a thin HTTP request/response layer built on top of a Client (here, WiFiClientSecure).
#include <Adafruit_GFX.h>         //the graphics primitive library and the SSD1306-specific driver that implements it.
#include <Adafruit_SSD1306.h>     //the graphics primitive library and the SSD1306-specific driver that implements it.
#include <Adafruit_Fingerprint.h> //driver for the optical fingerprint module (talks a binary packet protocol over UART).

// ================================================================
// ESP32-C3 SUPER MINI - FINGERPRINT ATTENDANCE SYSTEM
// Target: Arduino-ESP32 core 3.x (also uses standard tone()/noTone())
// ================================================================

// ---------------------------- Pins -------------------------------
// Avoid ESP32-C3 strapping pins GPIO2, GPIO8 and GPIO9.
// GPIO18/19 are normally used by native USB on C3 boards.
constexpr uint8_t OLED_SDA_PIN = 4; //compile-time constants with actual C++ types, so the compiler can type-check them (better than a raw macro).
constexpr uint8_t OLED_SCL_PIN = 5;  //chosen because the ESP32-C3 doesn't have fixed I2C pins; Wire.begin(sda, scl) later remaps them.
constexpr uint8_t FP_RX_PIN    = 0;   // ESP32 RX <- fingerprint TX
constexpr uint8_t FP_TX_PIN    = 1;   // ESP32 TX -> fingerprint RX
constexpr uint8_t BUZZER_PIN   = 10;   //passive buzzer or transducer driven with tone()/noTone().

// ---------------------------- OLED -------------------------------
constexpr uint8_t SCREEN_WIDTH  = 128;
constexpr uint8_t SCREEN_HEIGHT = 64;
constexpr uint8_t OLED_ADDRESS  = 0x3C;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------------------- Fingerprint UART -------------------------
constexpr uint32_t FP_BAUD = 57600;
HardwareSerial fpSerial(1);            // Use the ESP32-C3's UART1
Adafruit_Fingerprint finger(&fpSerial);

// ---------------------------- Wi-Fi ------------------------------
// For a real deployment, move these values to a secrets.h file.
const char *ssid     = "Texa";
const char *password = "12345678";

// IMPORTANT: use the raw URL only. Do not paste Markdown [text](url).
const char *scriptURL =
    "https://script.google.com/macros/s/AKfycbxubwjYiVg0JcdjFaWCcgruahPGND9iLwKXwFKAM7ocfdSMYZ0hzn6MQRCoY7KSAjHogw/exec";

// --------------------------- Settings ----------------------------
constexpr uint16_t MIN_MATCH_CONFIDENCE = 50;             //uint8_t does slower speed that's why we're using uint16_t
constexpr uint32_t ENROLL_TIMEOUT_MS     = 30000;
constexpr uint32_t WIFI_TIMEOUT_MS       = 15000;
constexpr uint32_t FINGER_RELEASE_MS     = 12000;

bool enrollMode = false;
int enrollID = -1;

// ================================================================
// Helpers
// ================================================================

String getNameByID(int id) {
  switch (id) {
    case 31: return "Arun";
    case 2:  return "keerthi";
    case 3:  return "Ganesh";
    case 4:  return "Logesh";
    case 5:  return "Suhail";
    case 6:  return "Afzal";
    case 7:  return "Aslam";
    case 8:  return "Nithi";
    case 9:  return "waseem";
    case 34: return "Aman";
    case 45: return "Rafic";
    default: return "User_" + String(id);
  }
}

uint16_t maxFingerprintID() {
  // getParameters() updates finger.capacity. Fall back if unavailable.
  if (finger.capacity >= 1 && finger.capacity <= 1000) {
    return finger.capacity;
  }
  return 127;
}

bool validFingerprintID(int id) {
  return id >= 1 && id <= static_cast<int>(maxFingerprintID());
}

String jsonEscape(const String &input) {
  String out;
  out.reserve(input.length() + 8);

  for (size_t i = 0; i < input.length(); ++i) {
    char c = input[i];
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"':  out += "\\\""; break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      case '\t': out += "\\t";  break;
      default:   out += c;       break;
    }
  }
  return out;
}

// ================================================================
// Buzzer
// ================================================================

void stopBuzzer() {
  noTone(BUZZER_PIN);
}

void beepSuccess() {
  tone(BUZZER_PIN, 1000, 150); delay(180);        //Pin, Hertz, Duration
  tone(BUZZER_PIN, 1400, 150); delay(180);        //Pin, Hertz, Duration
  tone(BUZZER_PIN, 1800, 200); delay(220);        //Pin, Hertz, Duration
  stopBuzzer();
}

void beepDenied() {
  tone(BUZZER_PIN, 400, 300); delay(350);         //Pin, Hertz, Duration
  tone(BUZZER_PIN, 300, 400); delay(450);         //Pin, Hertz, Duration
  stopBuzzer();
}

void beepStartup() {
  for (int f = 500; f <= 1500; f += 100) {
    tone(BUZZER_PIN, f, 60);
    delay(70);
  }
  stopBuzzer();
}

void beepScan() {
  tone(BUZZER_PIN, 800, 80);
  delay(100);
  stopBuzzer();
}

void beepEnroll() {
  tone(BUZZER_PIN, 1200, 100); delay(130);
  tone(BUZZER_PIN, 1200, 100); delay(130);
  stopBuzzer();
}

// ================================================================
// OLED UI
// ================================================================

void showMessage(const String &line1,
                 const String &line2 = "",
                 const String &line3 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 8);  display.println(line1);
  display.setCursor(0, 28); display.println(line2);
  display.setCursor(0, 48); display.println(line3);
  display.display();
}

void showWaiting() {
  display.clearDisplay();

  int bx = 50;
  int by = 8;
  display.drawRoundRect(bx, by, 28, 36, 5, SSD1306_WHITE);
  display.drawLine(bx + 5, by + 10, bx + 22, by + 10, SSD1306_WHITE);
  display.drawLine(bx + 4, by + 18, bx + 23, by + 18, SSD1306_WHITE);
  display.drawLine(bx + 5, by + 26, bx + 22, by + 26, SSD1306_WHITE);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 50);
  display.print("Place Finger...");
  display.display();
}

void showEnrollWaiting(int id, int step) {
  display.clearDisplay();
  display.drawRoundRect(0, 0, 128, 64, 4, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(10, 6);
  display.print("-- ENROLL MODE --");

  display.setCursor(6, 20);
  display.print("ID: ");
  display.print(id);
  display.print(" Step: ");
  display.print(step);
  display.print("/2");

  display.setCursor(6, 34);
  if (step == 1) display.print("Place finger...");
  else display.print("Place SAME finger");

  display.setCursor(6, 50);
  display.print("A to cancel");
  display.display();
}

void showScanAnimation() {
  constexpr int cx = 64;
  constexpr int cy = 30;

  for (int r = 5; r <= 27; r += 3) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 56);
    display.print("Scanning...");

    display.drawCircle(cx, cy, r, SSD1306_WHITE);
    if (r > 8)  display.drawCircle(cx, cy, r - 6, SSD1306_WHITE);
    if (r > 14) display.drawCircle(cx, cy, r - 12, SSD1306_WHITE);
    display.fillCircle(cx, cy, 3, SSD1306_WHITE);

    display.display();
    delay(45);
  }
}

void showGrantedAnimation(int id, String name) {
  constexpr int cx = 64;
  constexpr int cy = 32;

  for (int r = 2; r <= 72; r += 5) {
    display.clearDisplay();
    display.fillCircle(cx, cy, r, SSD1306_WHITE);
    display.display();
    delay(15);
  }

  display.clearDisplay();
  display.fillRect(0, 0, 128, 64, SSD1306_WHITE);
  for (int t = 0; t <= 2; ++t) {
    display.drawLine(38 + t, 30, 50 + t, 44, SSD1306_BLACK);
    display.drawLine(50 + t, 44, 74 + t, 18, SSD1306_BLACK);
  }
  display.display();
  delay(300);

  display.clearDisplay();
  display.drawRoundRect(2, 2, 124, 60, 6, SSD1306_WHITE);
  display.drawRoundRect(4, 4, 120, 56, 4, SSD1306_WHITE);

  for (int t = 0; t <= 1; ++t) {
    display.drawLine(22 + t, 22, 34 + t, 36, SSD1306_WHITE);
    display.drawLine(34 + t, 36, 54 + t, 10, SSD1306_WHITE);
  }

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(60, 10); display.print("MARKED!");
  display.setCursor(60, 26); display.print("ID: "); display.print(id);
  display.setCursor(60, 42);

  if (name.length() > 8) name = name.substring(0, 8);
  display.print(name);
  display.display();
}

void showDeniedAnimation() {
  for (int b = 0; b < 3; ++b) {
    display.clearDisplay();
    display.fillRect(0, 0, 128, 64, SSD1306_WHITE);
    display.display();
    delay(70);

    display.clearDisplay();
    display.display();
    delay(70);
  }

  display.clearDisplay();
  display.drawRoundRect(2, 2, 124, 60, 6, SSD1306_WHITE);

  for (int t = -1; t <= 1; ++t) {
    display.drawLine(30 + t, 12, 98 + t, 46, SSD1306_WHITE);
    display.drawLine(98 + t, 12, 30 + t, 46, SSD1306_WHITE);
  }

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(22, 51);
  display.print("ACCESS DENIED");
  display.display();
}

void showSendingAnimation() {
  for (int dot = 1; dot <= 3; ++dot) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 20);
    display.print("Sending data");

    for (int d = 0; d < dot; ++d) display.print(".");

    for (int b = 0; b < 4; ++b) {
      int bh = (b + 1) * 6;
      int bx = 44 + b * 12;
      int by = 50 - bh;

      if (b < dot) display.fillRect(bx, by, 8, bh, SSD1306_WHITE);
      else display.drawRect(bx, by, 8, bh, SSD1306_WHITE);
    }

    display.display();
    delay(250);
  }
}

// ================================================================
// Wi-Fi / HTTP
// ================================================================

bool connectWiFi(uint32_t timeoutMs = WIFI_TIMEOUT_MS) {
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.printf("Connecting to Wi-Fi: %s\n", ssid);
  WiFi.begin(ssid, password);

  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi connected. IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("Wi-Fi connection failed.");
  return false;
}

bool sendAttendance(int id, const String &name) {
  showSendingAnimation();

  if (!connectWiFi()) {
    showMessage("WiFi Failed!", "Attendance not", "uploaded");
    beepDenied();
    delay(1500);
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();  // Prototype choice. Use CA validation in production.

  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(10000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (!http.begin(client, scriptURL)) {
    Serial.println("HTTP begin failed.");
    showMessage("HTTP Error", "Could not start", "request");
    beepDenied();
    return false;
  }

  http.addHeader("Content-Type", "application/json");

  String payload;
  payload.reserve(100);
  payload = "{\"id\":" + String(id) +
            ",\"name\":\"" + jsonEscape(name) +
            "\",\"status\":\"Present\"}";

  Serial.print("Payload: ");
  Serial.println(payload);

  const int httpCode = http.POST(payload);
  Serial.print("HTTP code: ");
  Serial.println(httpCode);

  String response;
  if (httpCode > 0) {
    response = http.getString();
    Serial.print("Server response: ");
    Serial.println(response);
  }

  const bool success = (httpCode >= 200 && httpCode < 300);

  if (success) {
    display.clearDisplay();
    display.drawRoundRect(2, 2, 124, 60, 6, SSD1306_WHITE);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(18, 8); display.print("ATTENDANCE SAVED");
    display.drawLine(4, 20, 122, 20, SSD1306_WHITE);
    display.setCursor(10, 26); display.print("Name: "); display.print(name);
    display.setCursor(10, 38); display.print("ID  : "); display.print(id);
    display.setCursor(10, 50); display.print("Status: Present");
    display.display();
    beepSuccess();
  } else {
    showMessage("Log Failed!", "HTTP: " + String(httpCode), "Try again");
    beepDenied();
  }

  http.end();
  delay(1800);
  return success;
}

// ================================================================
// Fingerprint release/debounce
// ================================================================

bool waitForFingerRelease(uint32_t timeoutMs = FINGER_RELEASE_MS) {
  const unsigned long start = millis();
  uint8_t noFingerCount = 0;

  while (millis() - start < timeoutMs) {
    uint8_t p = finger.getImage();

    if (p == FINGERPRINT_NOFINGER) {
      ++noFingerCount;
      if (noFingerCount >= 2) {
        delay(150);
        return true;
      }
    } else {
      noFingerCount = 0;
    }

    delay(50);
  }

  Serial.println("Finger release timeout.");
  return false;
}

bool confirmRealFinger() {
  // Require stable presence across multiple captures.
  for (uint8_t i = 0; i < 3; ++i) {
    uint8_t p = finger.getImage();

    if (p == FINGERPRINT_NOFINGER) return false;
    if (p != FINGERPRINT_OK) {
      Serial.print("Fingerprint image error during debounce: 0x");
      Serial.println(p, HEX);
      return false;
    }

    delay(25);
  }
  return true;
}

// ================================================================
// Fingerprint scan
// ================================================================

int getFingerprintID() {
  uint8_t p = finger.getImage();

  if (p == FINGERPRINT_NOFINGER) return -1;

  if (p != FINGERPRINT_OK) {
    Serial.print("getImage error: 0x");
    Serial.println(p, HEX);
    delay(50);
    return -1;
  }

  // Debounce the optical sensor. This filters transient/ghost captures.
  if (!confirmRealFinger()) return -1;

  beepScan();
  showScanAnimation();

  // confirmRealFinger() left the latest good image in the sensor buffer.
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.print("image2Tz failed: 0x");
    Serial.println(p, HEX);
    showMessage("Bad Finger Image", "Place finger", "again");
    beepDenied();
    waitForFingerRelease();
    return -1;
  }

  p = finger.fingerFastSearch();

  if (p == FINGERPRINT_OK) {
    Serial.print("Matched ID: ");
    Serial.println(finger.fingerID);
    Serial.print("Confidence: ");
    Serial.println(finger.confidence);

    if (finger.confidence < MIN_MATCH_CONFIDENCE) {
      Serial.println("Rejected: confidence below threshold.");
      showMessage("Low Confidence", "Try finger again", "");
      beepDenied();
      waitForFingerRelease();
      return -1;
    }

    return finger.fingerID;
  }

  if (p == FINGERPRINT_NOTFOUND || p == FINGERPRINT_NOMATCH) {
    Serial.println("Fingerprint not recognized.");
    beepDenied();
    showDeniedAnimation();
    waitForFingerRelease();
    delay(250);
    return -1;
  }

  // Communication/image errors are not shown as "access denied" because
  // they do not mean that the biometric comparison actually failed.
  Serial.print("Fingerprint search error: 0x");
  Serial.println(p, HEX);
  showMessage("Sensor Read Error", "Try again", "");
  delay(500);
  return -1;
}

// ================================================================
// List / delete templates
// ================================================================

void listStoredIDs() {
  Serial.println("=================================");
  Serial.println("       STORED FINGERPRINT IDs");
  Serial.println("=================================");

  finger.getTemplateCount();
  Serial.print("Sensor reports total: ");
  Serial.println(finger.templateCount);
  Serial.print("Sensor capacity: ");
  Serial.println(maxFingerprintID());
  Serial.println("---------------------------------");

  int found = 0;
  for (uint16_t i = 1; i <= maxFingerprintID(); ++i) {
    if (finger.loadModel(i) == FINGERPRINT_OK) {
      Serial.print("ID #");
      Serial.print(i);
      Serial.print(" -> ");
      Serial.println(getNameByID(i));
      ++found;
      delay(5);
    }
  }

  if (found == 0) Serial.println("No fingerprints stored.");
  Serial.println("=================================");

  display.clearDisplay();
  display.drawRoundRect(0, 0, 128, 64, 4, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 6); display.print("Stored IDs: "); display.print(found);
  display.setCursor(6, 22); display.print("Check Serial");
  display.setCursor(6, 36); display.print("Monitor for");
  display.setCursor(6, 50); display.print("full list...");
  display.display();
  delay(2000);
}

void removeFingerprint(int id) {
  if (!validFingerprintID(id)) {
    Serial.println("Invalid fingerprint ID.");
    return;
  }

  Serial.print("Removing ID #");
  Serial.println(id);
  showMessage("Removing...", "ID: " + String(id), "Please wait");

  uint8_t p = finger.deleteModel(id);

  if (p == FINGERPRINT_OK) {
    Serial.println("Fingerprint deleted.");

    display.clearDisplay();
    display.drawRoundRect(2, 2, 124, 60, 6, SSD1306_WHITE);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(22, 14); display.print("ID REMOVED!");
    display.drawLine(4, 26, 122, 26, SSD1306_WHITE);
    display.setCursor(10, 34); display.print("ID #"); display.print(id); display.print(" deleted");
    display.setCursor(10, 48); display.print(getNameByID(id));
    display.display();

    beepDenied();
    delay(1800);
  } else {
    Serial.print("Delete failed: 0x");
    Serial.println(p, HEX);
    showMessage("Delete Failed!", "ID #" + String(id), "Not found?");
    beepDenied();
    delay(1500);
  }
}

// ================================================================
// Enrollment
// ================================================================

bool enrollmentCancelledFromSerial() {
  if (!Serial.available()) return false;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "A" || cmd.startsWith("R#")) {
    Serial.println("Enrollment cancelled.");
    showMessage("Enroll", "Cancelled", "");
    delay(1000);
    return true;
  }

  Serial.print("Ignored during enrollment: ");
  Serial.println(cmd);
  return false;
}

bool waitForEnrollmentImage(uint32_t timeoutMs) {
  const unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    if (enrollmentCancelledFromSerial()) return false;

    uint8_t p = finger.getImage();

    if (p == FINGERPRINT_OK) return true;
    if (p == FINGERPRINT_NOFINGER) {
      delay(30);
      continue;
    }

    // Image/packet errors are transient during placement, so keep trying.
    delay(50);
  }

  showMessage("Enroll Timeout", "Cancelled", "");
  Serial.println("Enrollment timed out.");
  beepDenied();
  delay(1200);
  return false;
}

bool enrollFingerprint(int id) {
  if (!validFingerprintID(id)) {
    Serial.print("Invalid ID. Valid range: 1-");
    Serial.println(maxFingerprintID());
    showMessage("Invalid ID", "Range: 1-" + String(maxFingerprintID()), "");
    delay(1500);
    return false;
  }

  Serial.println("---------------------------------");
  Serial.print("Enrolling ID #");
  Serial.println(id);

  // ------------------------- Scan 1 ------------------------------
  showEnrollWaiting(id, 1);
  Serial.println("Place finger (scan 1)...");

  if (!waitForEnrollmentImage(ENROLL_TIMEOUT_MS)) return false;

  beepEnroll();
  showMessage("Finger Detected", "Processing...", "");

  uint8_t p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.print("Scan 1 conversion failed: 0x");
    Serial.println(p, HEX);
    showMessage("Convert Failed", "Try again", "");
    beepDenied();
    delay(1500);
    return false;
  }

  showMessage("Scan 1 OK!", "Remove finger", "");
  beepSuccess();

  if (!waitForFingerRelease(10000)) {
    showMessage("Remove Finger", "Then retry", "");
    beepDenied();
    delay(1000);
    return false;
  }

  // ------------------------- Scan 2 ------------------------------
  showEnrollWaiting(id, 2);
  Serial.println("Place SAME finger (scan 2)...");

  if (!waitForEnrollmentImage(ENROLL_TIMEOUT_MS)) return false;

  beepEnroll();
  showMessage("Finger Detected", "Processing...", "");

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.print("Scan 2 conversion failed: 0x");
    Serial.println(p, HEX);
    showMessage("Convert Failed", "Try again", "");
    beepDenied();
    delay(1500);
    return false;
  }

  showMessage("Creating", "Model...", "");
  p = finger.createModel();

  if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Enrollment mismatch.");
    showMessage("MISMATCH!", "Use SAME finger", "Try again");
    beepDenied();
    delay(1800);
    waitForFingerRelease();
    return false;
  }

  if (p != FINGERPRINT_OK) {
    Serial.print("Model creation failed: 0x");
    Serial.println(p, HEX);
    showMessage("Model Failed", "Try again", "");
    beepDenied();
    delay(1500);
    waitForFingerRelease();
    return false;
  }

  showMessage("Storing...", "ID: " + String(id), "");
  p = finger.storeModel(id);

  if (p != FINGERPRINT_OK) {
    Serial.print("Store failed: 0x");
    Serial.println(p, HEX);
    showMessage("Store Failed!", "Try another ID", "");
    beepDenied();
    delay(1500);
    waitForFingerRelease();
    return false;
  }

  finger.getTemplateCount();

  display.clearDisplay();
  display.drawRoundRect(2, 2, 124, 60, 6, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(18, 6); display.print("ENROLLED!");
  display.drawLine(4, 18, 122, 18, SSD1306_WHITE);
  display.setCursor(6, 26); display.print("ID   : #"); display.print(id);
  display.setCursor(6, 38); display.print("Name : "); display.print(getNameByID(id));
  display.setCursor(6, 52); display.print("Total: "); display.print(finger.templateCount);
  display.display();

  Serial.println("Enrollment successful.");
  beepSuccess();
  waitForFingerRelease();
  delay(1500);
  return true;
}

// ================================================================
// Serial command interface
// ================================================================

void printCommands() {
  Serial.println("Commands:");
  Serial.println("  A       -> enter enrollment mode");
  Serial.println("  #<id>   -> enroll that ID");
  Serial.println("  R#<id>  -> remove that ID");
  Serial.println("  L       -> list stored IDs");
}

void handleSerial() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toUpperCase();

  if (cmd.length() == 0) return;

  Serial.print("CMD: ");
  Serial.println(cmd);

  if (cmd == "L") {
    listStoredIDs();
    return;
  }

  if (cmd == "A") {
    enrollMode = true;
    enrollID = -1;
    Serial.println("ENROLL MODE - send #<id>");
    showMessage("ENROLL MODE", "Send #<id>", "e.g. #5");
    return;
  }

  if (cmd.startsWith("R#")) {
    int id = cmd.substring(2).toInt();
    removeFingerprint(id);
    enrollMode = false;
    enrollID = -1;
    return;
  }

  if (cmd.startsWith("#")) {
    int id = cmd.substring(1).toInt();

    if (!validFingerprintID(id)) {
      Serial.print("Invalid ID. Valid range: 1-");
      Serial.println(maxFingerprintID());
      return;
    }

    enrollMode = true;
    enrollID = id;
    enrollFingerprint(id);

    // Always leave enrollment state cleanly, success or failure.
    enrollMode = false;
    enrollID = -1;
    return;
  }

  printCommands();
}

// ================================================================
// Setup
// ================================================================

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(150);
  delay(700);

  pinMode(BUZZER_PIN, OUTPUT);
  stopBuzzer();

  // -------------------------- OLED -------------------------------
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED initialization failed.");
    while (true) delay(1000);
  }

  for (int x = 0; x <= 128; x += 4) {
    display.clearDisplay();
    display.fillRect(0, 0, x, 64, SSD1306_WHITE);
    display.display();
    delay(8);
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 10);  display.print("ATTEND");
  display.setCursor(16, 38); display.print("SYSTEM");
  display.display();
  beepStartup();
  delay(700);

  // -------------------------- Wi-Fi ------------------------------
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);

  showMessage("Connecting WiFi", ssid, "Please wait...");

  if (connectWiFi()) {
    showMessage("WiFi Connected!", WiFi.localIP().toString(), "");
    tone(BUZZER_PIN, 1200, 250);
    delay(700);
    stopBuzzer();
  } else {
    // Do not halt. Fingerprint system can still operate locally and retry
    // Wi-Fi when an attendance record needs to be uploaded.
    showMessage("WiFi Offline", "Will retry when", "finger matches");
    beepDenied();
    delay(1200);
  }

  // ---------------------- Fingerprint UART -----------------------
  // Begin UART1 directly with explicit RX/TX GPIO routing.
  // We intentionally do not use SoftwareSerial on ESP32-C3.
  fpSerial.begin(FP_BAUD, SERIAL_8N1, FP_RX_PIN, FP_TX_PIN);
  delay(1000);  // Give sensor time to boot.

  if (!finger.verifyPassword()) {
    Serial.println("Fingerprint sensor not found / password verification failed.");
    showMessage("Sensor Error!", "Check RX/TX/VCC", "and baud rate");
    beepDenied();
    while (true) delay(1000);
  }

  Serial.println("Fingerprint sensor ready.");

  uint8_t paramResult = finger.getParameters();
  if (paramResult == FINGERPRINT_OK) {
    Serial.print("Sensor capacity: ");
    Serial.println(finger.capacity);
    Serial.print("Security level: ");
    Serial.println(finger.security_level);
    Serial.print("Sensor baud: ");
    Serial.println(finger.baud_rate);
  } else {
    Serial.print("Could not read sensor parameters: 0x");
    Serial.println(paramResult, HEX);
  }

  finger.getTemplateCount();

  Serial.println("=================================");
  Serial.println("   ESP32-C3 ATTENDANCE READY");
  Serial.println("=================================");
  Serial.print("Stored fingerprints: ");
  Serial.println(finger.templateCount);
  Serial.print("Maximum ID: ");
  Serial.println(maxFingerprintID());
  printCommands();
  Serial.println("=================================");

  showMessage("Stored Prints:", String(finger.templateCount), "System Ready!");
  delay(1500);
}

// ================================================================
// Main loop
// ================================================================

void loop() {
  handleSerial();

  if (enrollMode && enrollID == -1) {
    showMessage("ENROLL MODE", "Send #<id>", "via Serial");
    delay(150);
    return;
  }

  if (enrollMode) return;

  showWaiting();

  int id = getFingerprintID();
  if (id <= 0) {
    delay(30);
    return;
  }

  String name = getNameByID(id);

  Serial.println("---------------------------------");
  Serial.print("Matched ID: ");
  Serial.println(id);
  Serial.print("Name: ");
  Serial.println(name);
  Serial.print("Confidence: ");
  Serial.println(finger.confidence);

  showGrantedAnimation(id, name);
  delay(250);

  sendAttendance(id, name);

  // Critical duplicate-prevention latch: the same physical placement cannot
  // generate another attendance record until the finger is removed.
  showMessage("Remove Finger", "Ready for next", "scan after lift");
  waitForFingerRelease();
  delay(250);
}
