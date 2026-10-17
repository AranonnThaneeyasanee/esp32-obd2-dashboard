#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <WiFi.h>

// ============================================================
// State definitions for C3 Reader (Client)
// ============================================================
#define STATE_WIFI_CONNECTING 1
#define STATE_TCP_CONNECTING 2
#define STATE_ACTIVE 3

int currentState = STATE_WIFI_CONNECTING;

// ============================================================
// WiFi Network Config to connect to S3 Zero (Vgate Sim)
// ============================================================
const char *obd_ssid = "BYD_OBD2_Simulator";
const char *obd_password = "";
const char *obd_ip = "192.168.4.1"; // S3 Zero AP IP
const uint16_t tcp_port = 35000;

// ============================================================
// GPIO pin definitions for ESP32-C3 SuperMini
// ============================================================
#define TFT_SCLK 4
#define TFT_MOSI 5
#define TFT_RST 0
#define TFT_DC 1
#define TFT_CS 7

// Manual RGB565 color definitions (High-Contrast Colorful Minimalist Theme)
#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF
#define COLOR_RED 0xF800   // Weak Alert
#define COLOR_GREEN 0x07E0 // Neon Green (Charging / Healthy)
#define COLOR_BLUE 0x001F
#define COLOR_CYAN 0x07FF     // Soft Cyan (Normal Standby)
#define COLOR_YELLOW 0xFFE0   // Charging low threshold
#define COLOR_ORANGE 0xFD20   // Standby Alert
#define COLOR_GREY 0xA596     // Soft Grey (Labels)
#define COLOR_DARKGREY 0x18E3 // Tech dark grey for unlit segments
#define COLOR_NAVY 0x011A     // Navy Blue (Header)

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
WiFiClient obdClient;

// ============================================================
// Voltage and networking variables
// ============================================================
float batt12v = 0.00;
unsigned long lastQueryTime = 0;
bool txBlinkState = false;
bool rxBlinkState = false;

// ============================================================
// Non-blocking connection and socket data handler
// ============================================================
void handleConnection() {
  // 1. Manage WiFi connection with full radio reset retry
  if (WiFi.status() != WL_CONNECTED) {
    currentState = STATE_WIFI_CONNECTING;
    static unsigned long lastWiFiAttempt = 0;
    unsigned long now = millis();

    // Full radio reset every 15 seconds if not connected
    if (now - lastWiFiAttempt > 15000) {
      lastWiFiAttempt = now;
      Serial.println("WiFi not connected. Full radio reset...");
      WiFi.disconnect(true); // true = turn off WiFi radio completely
      delay(1000);           // Wait for radio to fully shut down
      WiFi.mode(WIFI_STA);
      WiFi.setAutoReconnect(true);
      delay(100);
      if (obd_password == NULL || strlen(obd_password) == 0) {
        WiFi.begin(obd_ssid);
      } else {
        WiFi.begin(obd_ssid, obd_password);
      }
    }
    return;
  }

  // 2. Connect TCP Socket to port 35000 of S3 Zero
  if (!obdClient.connected()) {
    currentState = STATE_TCP_CONNECTING;
    static unsigned long lastTCPAttempt = 0;
    if (millis() - lastTCPAttempt >
        2000) { // Timeout 2s to prevent port spamming
      lastTCPAttempt = millis();
      Serial.println("Connecting to S3 Zero OBD Port...");
      obdClient.stop();
      if (obdClient.connect(obd_ip, tcp_port)) {
        Serial.println("Connected to S3 Zero! Handshaking...");
        obdClient.print("ATE0\r"); // Disable Echo
        delay(50);
        obdClient.print("ATRV\r"); // Initial voltage query
        lastQueryTime = millis();
        currentState = STATE_ACTIVE;
      }
    }
    return;
  }

  // 3. Send ATRV query every 300ms when connected
  currentState = STATE_ACTIVE;
  unsigned long now = millis();
  if (now - lastQueryTime > 300) {
    lastQueryTime = now;
    obdClient.print("ATRV\r");
    txBlinkState = !txBlinkState;
  }

  // Read and parse responses
  while (obdClient.available()) {
    String resp = obdClient.readStringUntil('>');
    resp.trim();
    if (resp.length() > 0) {
      int idx = resp.indexOf('V');
      if (idx != -1) {
        String valStr = resp.substring(0, idx);
        valStr.trim();
        float val = valStr.toFloat();
        if (val > 5.0 && val < 20.0) {
          batt12v = val;
          rxBlinkState = !rxBlinkState;
        }
      }
    }
  }
}

// ============================================================
// Draw Clean Segmented LED Gauge UI (Reference Match + Colorful Glow)
// ============================================================
void drawGaugeUI() {
  // Prevent screen flicker by clearing only when state changes
  static int prevState = -1;
  if (currentState != prevState) {
    prevState = currentState;
    tft.fillRect(0, 30, 320, 142, COLOR_BLACK); // Clear lower part only
  }

  // 1. Header (gorgeous navy banner matched to original reference image)
  static bool headerDrawn = false;
  if (!headerDrawn) {
    tft.fillRect(0, 0, 320, 28, COLOR_NAVY);
    tft.setTextColor(COLOR_WHITE);
    tft.setTextSize(2);
    tft.setCursor(50, 6);
    tft.print("12V BATTERY (V)");
    headerDrawn = true;
  }

  // 2. Giant Centered Voltage Digits (High-contrast dynamic colors)
  tft.setTextSize(8); // Huge readability size

  if (currentState == STATE_ACTIVE) {
    // Dynamic color text for fast status checking
    uint16_t numColor;
    if (batt12v >= 13.00) {
      numColor = COLOR_GREEN; // Active Charging (Neon Green)
    } else if (batt12v >= 12.00) {
      numColor = COLOR_CYAN; // Normal Idle (Cyber Cyan)
    } else {
      numColor = COLOR_RED; // Battery Alert (Red)
    }

    tft.setTextColor(numColor, COLOR_BLACK);

    char voltStr[16];
    sprintf(voltStr, "%.2f", batt12v);

    // Width of 5 chars ("13.85") at Size 8 is 240px.
    // Center it on 320px screen: (320 - 240) / 2 = 40px.
    tft.setCursor(40, 36);
    tft.print(voltStr);
  } else {
    // Elegant connecting states
    tft.setTextSize(4);
    tft.setTextColor(COLOR_GREY, COLOR_BLACK);
    tft.setCursor(40, 52);
    if (currentState == STATE_WIFI_CONNECTING) {
      tft.print("SEARCH AP");
    } else {
      tft.print("CONNECTING");
    }
  }

  // 3. Premium Colorful Segmented LED Bar Graph (y: 53 to 59)
  // 20 segments, 6px wide, 1px spacing. Total width = 139px.
  // Center alignment: starts at x = 10.
  int barStartX = 20;
  int barStartY = 114;
  int segWidth = 14;
  int segHeight = 14;
  int segSpacing = 2;
  int numSegments = 20;

  static int animIdx = 0;
  static int animDir = 1;
  static unsigned long lastAnim = 0;

  if (currentState == STATE_ACTIVE) {
    // Map voltage range 11.0V to 15.0V (4.0V total range over 19 steps)
    for (int i = 0; i < numSegments; i++) {
      float segVolt = 11.0 + (i * (4.0 / 19.0));
      uint16_t segColor;

      // Beautiful multi-color gradient based on position
      if (i < 5) {
        segColor = COLOR_RED; // < 12.05V Red (Weak)
      } else if (i >= 5 && i < 9) {
        segColor = COLOR_ORANGE; // 12.05V - 12.89V Orange (Normal Standby)
      } else if (i >= 9 && i < 13) {
        segColor = COLOR_YELLOW; // 12.90V - 13.73V Yellow (Starting to charge)
      } else {
        segColor = COLOR_GREEN; // >= 13.74V Neon Green (Strong Charging)
      }

      int x = barStartX + (i * (segWidth + segSpacing));
      if (batt12v >= segVolt) {
        tft.fillRect(x, barStartY, segWidth, segHeight,
                     segColor); // Lit Segment
      } else {
        tft.fillRect(x, barStartY, segWidth, segHeight,
                     COLOR_DARKGREY); // Unlit Segment
      }
    }
  } else {
    // Smooth scanning pulse animation when connecting
    if (millis() - lastAnim > 50) {
      lastAnim = millis();
      animIdx += animDir;
      if (animIdx >= numSegments - 1 || animIdx <= 0) {
        animDir = -animDir;
      }
    }

    for (int i = 0; i < numSegments; i++) {
      int x = barStartX + (i * (segWidth + segSpacing));
      if (abs(i - animIdx) <= 1) {
        tft.fillRect(x, barStartY, segWidth, segHeight,
                     COLOR_CYAN); // Moving scan highlight
      } else {
        tft.fillRect(x, barStartY, segWidth, segHeight,
                     COLOR_DARKGREY); // Unlit background
      }
    }
  }

  // 4. Reference Labels & Dual Status LEDs (Exact match to original reference)
  tft.setTextSize(2);
  tft.setTextColor(COLOR_GREY, COLOR_BLACK);

  // Left scale marker: "11.0 L/H"
  tft.setCursor(20, 140);
  tft.print("11.0 L/H");

  // Right scale marker: "1/2"
  tft.setCursor(270, 140);
  tft.print("1/2");

  // Center status indicator circles (y = 71)
  // Left Circle: Green = WiFi online, Red = searching
  tft.fillCircle(146, 146, 4,
                 (WiFi.status() == WL_CONNECTED) ? COLOR_GREEN : COLOR_RED);

  // Right Circle: Pulsing Cyan on data exchange (TX/RX blink)
  tft.fillCircle(170, 146, 4,
                 (currentState == STATE_ACTIVE && rxBlinkState)
                     ? COLOR_CYAN
                     : COLOR_DARKGREY);
}

// ============================================================
// ARDUINO SETUP
// ============================================================
void setup(void) {
  Serial.begin(115200);
  delay(2000);

  Serial.println("=================================");
  Serial.println("Project LUNA: C3 Dashboard Active!");
  Serial.println("=================================");

  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  digitalWrite(TFT_CS, HIGH);

  // Hard reset the TFT display
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH);
  delay(5);
  digitalWrite(TFT_RST, LOW);
  delay(20);
  digitalWrite(TFT_RST, HIGH);
  delay(150);

  // Initialize SPI bus
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);
  tft.init(172, 320);
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);

  tft.setRotation(1);
  tft.fillScreen(COLOR_BLACK);

  // Connect to S3 Zero AP
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  Serial.print("Connecting to S3 Zero WiFi AP: ");
  Serial.println(obd_ssid);
  if (obd_password == NULL || strlen(obd_password) == 0) {
    WiFi.begin(obd_ssid);
  } else {
    WiFi.begin(obd_ssid, obd_password);
  }

  Serial.println("===== Init Done! =====");
}

// ============================================================
// ARDUINO LOOP
// ============================================================
void loop() {
  // Manage socket and connections
  handleConnection();

  // Update visual battery interface
  drawGaugeUI();

  // Yield CPU to FreeRTOS scheduler to prevent overheating and system lag
  delay(2);
}
