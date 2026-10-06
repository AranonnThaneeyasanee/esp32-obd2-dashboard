#pragma once

#include <cmath>
#include <cctype>

// Forward Declarations
void drawGIFHome_T02();

// ============================================================
// Vehicle Calibration Profile
// ============================================================
// ค่า PID เฉพาะผู้ผลิต, ECU header และสัมประสิทธิ์แปลงหน่วยทั้งหมด
// ถูกแยกออกไปไว้ในไฟล์ calibration เพื่อให้เปลี่ยนรุ่นรถได้โดยไม่ต้องแก้ logic
//
// ถ้ามี vehicle_calibration.h (ค่าจริงของคุณเอง) จะถูกใช้ก่อน
// ถ้าไม่มี จะ fallback ไปใช้ค่าตัวอย่างเพื่อให้โปรเจคคอมไพล์ผ่าน
// ดูวิธีตั้งค่าได้ที่หัวไฟล์ vehicle_calibration.example.h
#if __has_include("vehicle_calibration.h")
  #include "vehicle_calibration.h"
  #define VP_CALIBRATION_SOURCE "user"
#else
  #include "vehicle_calibration.example.h"
  #define VP_CALIBRATION_SOURCE "example-placeholder"
#endif

extern int dolphin_test_mode;

#define DEFAULT_DOLPHIN_TEST 0

#define TEST1_PID          VP_REQ_SPEED
#define TEST1_DIVISOR      VP_SPEED_DIVISOR
#define TEST1_BUILD_NAME   ("SPEED_PROFILE_" VP_CALIBRATION_SOURCE)

// ============================================================
// Multi-Page and Touch Sensor (TTP223) Configuration
// ============================================================
#define TOUCH_PIN 7 // GPIO 7 on Left Side of S3 Zero (Connect to TTP223 SIG)
#define ENABLE_CHG_DEBUG_PAGE 1
#define ENABLE_CHG_EST_DEBUG_PAGE 1 // Temporary Hardware Debug Page 9 (Compile-time only)

#if ENABLE_CHG_EST_DEBUG_PAGE
#define TOTAL_PAGES 9
#elif ENABLE_CHG_DEBUG_PAGE
#define TOTAL_PAGES 8
#else
#define TOTAL_PAGES 7
#endif
#define GIF_Y_OFFSET 32

int currentPage = 1;
// 1 = Robo Eye / GIF Home
// 2 = 12V Battery
// 3 = Battery Pack Temp
// 4 = EV Motor RPM (via BYD MCU PID VP_PID_MOTOR_RPM)
// 5 = Motor Temp (via BYD MCU PID VP_PID_MOTOR_TEMP)
// 6 = Battery SOH (via BYD BMS PID VP_PID_SOH)
// 7 = Energy Use (via BYD BMS PID VP_PID_ENERGY)

// ============================================================
// Display Configurations (Toggleable via long press)
// ============================================================
bool showGifHome = false; // false = Robo Eyes, true = loop.gif
bool gifHomeNeedClear = true; // true = clear screen when launching home GIF

// ============================================================
// State definitions for S3 Dashboard Connection
// ============================================================
#define STATE_WIFI_CONNECTING 1
#define STATE_TCP_CONNECTING 2
#define STATE_ACTIVE 3

int currentState = STATE_WIFI_CONNECTING;

// ============================================================
// Wi-Fi Supervisor State Definitions & Enums
// ============================================================
enum ApState : uint8_t {
  AP_STATE_STOPPED = 0,
  AP_STATE_RUNNING = 1,
  AP_STATE_CLIENT_CONNECTED = 2,
  AP_STATE_RECOVERING = 3
};

enum StaState : uint8_t {
  STA_STATE_IDLE = 0,
  STA_STATE_CONNECTING = 1,
  STA_STATE_CONNECTED = 2,
  STA_STATE_SCANNING = 3,
  STA_STATE_BACKOFF = 4,
  STA_STATE_WAIT_FOR_ADAPTER = 5
};

enum TcpState : uint8_t {
  TCP_STATE_IDLE = 0,
  TCP_STATE_CONNECTING = 1,
  TCP_STATE_ACTIVE = 2,
  TCP_STATE_RECONNECT_WAIT = 3
};

enum ElmInitState : uint8_t {
  ELM_INIT_IDLE = 0,
  ELM_INIT_SEND_ATZ = 1,
  ELM_INIT_WAIT_ATZ = 2,
  ELM_INIT_SEND_ATD = 3,
  ELM_INIT_WAIT_ATD = 4,
  ELM_INIT_SEND_ATE0 = 5,
  ELM_INIT_WAIT_ATE0 = 6,
  ELM_INIT_SEND_ATH1 = 7,
  ELM_INIT_WAIT_ATH1 = 8,
  ELM_INIT_SEND_ATSP0 = 9,
  ELM_INIT_WAIT_ATSP0 = 10,
  ELM_INIT_SEND_ATAL = 11,
  ELM_INIT_WAIT_ATAL = 12,
  ELM_INIT_SEND_ATST64 = 13,
  ELM_INIT_WAIT_ATST64 = 14,
  ELM_INIT_CONFIRMED = 15,
  ELM_INIT_FAILED = 16
};

enum PortalState : uint8_t {
  PORTAL_STATE_INACTIVE = 0,
  PORTAL_STATE_ENTER = 1,
  PORTAL_STATE_ACTIVE = 2,
  PORTAL_STATE_UPLOAD = 3,
  PORTAL_STATE_EXIT = 4
};

enum ChannelSyncState : uint8_t {
  CHANNEL_SYNC_OK = 0,
  CHANNEL_SYNC_PENDING = 1,
  CHANNEL_SYNC_IN_PROGRESS = 2
};

// ── OBD Connection Health State ──────────────────────────────────────────────
// Single source of truth for all UI health indicators.
// Written ONLY by handleConnection() via getObdHealthState().
// Read by: ui_themes.h (status circle, top bar scanner), /api/status.
enum ObdHealthState : uint8_t {
  OBD_HEALTH_DISCONNECTED = 0,  // Wi-Fi lost or stack fully down
  OBD_HEALTH_CONNECTING   = 1,  // Initial bring-up phase, no link yet
  OBD_HEALTH_WARNING      = 2,  // Wi-Fi OK but TCP/ELM recovering, or data stale
  OBD_HEALTH_CONNECTED    = 3   // Wi-Fi + TCP + ELM + fresh OBD data = fully healthy
};
inline ObdHealthState obdHealthState = OBD_HEALTH_DISCONNECTED;

// ── Wi-Fi Drop Recovery Cycle Tracking ───────────────────────────────────────
// wifiEverConnected: strictly monotonic latch. Transitions false -> true on GOT_IP.
// Never reset during runtime; prevents boot connection retries from being miscounted.
inline bool     wifiEverConnected        = false;

// wifiDropRecoveryActive: SOLE lifecycle flag for active drop recovery cycle.
// Latches true on STA_DISCONNECTED (only if wifiEverConnected).
// Clears ONLY when full recovery (OBD_HEALTH_CONNECTED) is confirmed.
inline bool     wifiDropRecoveryActive   = false;

// wifiDropRecoveryStartMs: timestamp of the FIRST disconnect event of the cycle.
// Preserved across multiple re-drops within the same cycle to measure true total outage.
// NOTE: 0 after cycle completion is an intentional reset state; validity is governed by wifiDropRecoveryActive.
inline uint32_t wifiDropRecoveryStartMs  = 0;

// wifiLastDropMs: timestamp of the MOST RECENT disconnect event.
// Updated unconditionally on every disconnect; used by [RECONNECTED] for staDowntime.
inline uint32_t wifiLastDropMs           = 0;

// wifiDropCount: primary cumulative disconnect counter.
// Replaces ambiguous wifiReconnectCount semantics while maintaining full backward compatibility.
inline uint32_t wifiDropCount            = 0;
inline uint32_t& wifiReconnectCount      = wifiDropCount; // Backward compatibility alias

// ── OBD Health Diagnostic Counters (non-persistent, serial/API debug only) ───
inline uint32_t obdHealthWarningCount         = 0;
inline uint32_t obdHealthDisconnectCount      = 0;
inline uint32_t obdHealthRecoverySuccessCount = 0;


// Stale Event Record Structure
struct WiFiEventRecord {
  uint32_t sequence;
  uint32_t timestampMs;
  WiFiEvent_t event;
  uint8_t disconnectReason;
  bool pending;
};

// Scan Candidate Structure
struct ScanCandidate {
  String ssid;
  String bssid;
  int32_t rssi;
  uint8_t channel;
  int priority;
};

// Supervisor runtime state variables
inline ApState currentApState = AP_STATE_STOPPED;
inline StaState currentStaState = STA_STATE_IDLE;
inline TcpState currentTcpState = TCP_STATE_IDLE;
inline ElmInitState currentElmState = ELM_INIT_IDLE;
inline PortalState currentPortalState = PORTAL_STATE_INACTIVE;
inline ChannelSyncState channelSyncState = CHANNEL_SYNC_OK;

inline uint8_t currentApChannel = 1;      // Known AP channel
inline uint8_t targetApChannel = 1;       // Target AP channel for sync
inline uint8_t savedObdChannel = 1;       // Persisted channel hint
inline String savedObdBssid = "";         // Persisted BSSID hint
inline String savedObdSsid = "";          // Persisted SSID cache
inline String savedObdIp = "192.168.0.10";// Persisted IP cache
inline uint32_t savedObdGen = 0;          // Persisted generation number
inline uint8_t currentActiveSlot = 0;     // Active NVS Slot (0 or 1)
inline bool obdConfigValid = false;       // Persisted valid flag

// RFC 1982 Serial Number Arithmetic Helper
inline bool isGenerationNewer(uint32_t a, uint32_t b) {
  return (int32_t)(a - b) > 0;
}

inline bool isSequenceNewer(uint32_t a, uint32_t b) {
  return (int32_t)(a - b) > 0;
}

// MAC / BSSID Validation and Parser Helpers
inline bool parseMacAddress(const String& macStr, uint8_t mac[6]) {
  if (macStr.length() < 17) return false;
  unsigned int m[6];
  if (sscanf(macStr.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x",
             &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 6 &&
      sscanf(macStr.c_str(), "%02X:%02X:%02X:%02X:%02X:%02X",
             &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 6) {
    return false;
  }
  bool allZero = true;
  for (int i = 0; i < 6; i++) {
    mac[i] = (uint8_t)m[i];
    if (mac[i] != 0) allZero = false;
  }
  return !allZero;
}

inline bool isValidMac(const String& macStr) {
  uint8_t dummy[6];
  return parseMacAddress(macStr, dummy);
}

// Canonical CRC-8 Integrity Calculator
inline uint8_t updateCrc8(uint8_t crc, uint8_t data) {
  crc ^= data;
  for (uint8_t i = 0; i < 8; ++i) {
    if (crc & 0x80) crc = (crc << 1) ^ 0x07;
    else crc <<= 1;
  }
  return crc;
}

inline uint8_t calcObdConfigCrc8(const String& ssid, const String& ip, const uint8_t bssid[6], uint8_t ch, uint32_t gen) {
  uint8_t crc = 0x00;
  // 1. Length-prefixed SSID
  crc = updateCrc8(crc, (uint8_t)ssid.length());
  for (size_t i = 0; i < ssid.length(); ++i) crc = updateCrc8(crc, (uint8_t)ssid[i]);
  // 2. Length-prefixed IP
  crc = updateCrc8(crc, (uint8_t)ip.length());
  for (size_t i = 0; i < ip.length(); ++i) crc = updateCrc8(crc, (uint8_t)ip[i]);
  // 3. Raw 6-byte BSSID
  for (size_t i = 0; i < 6; ++i) crc = updateCrc8(crc, bssid[i]);
  // 4. Channel (1 byte)
  crc = updateCrc8(crc, ch);
  // 5. Generation (4 bytes little-endian)
  crc = updateCrc8(crc, (uint8_t)(gen & 0xFF));
  crc = updateCrc8(crc, (uint8_t)((gen >> 8) & 0xFF));
  crc = updateCrc8(crc, (uint8_t)((gen >> 16) & 0xFF));
  crc = updateCrc8(crc, (uint8_t)((gen >> 24) & 0xFF));
  return crc;
}

// Critical Section Mutex for Synchronized Event Snapshot Protocol
inline portMUX_TYPE wifiEventMux = portMUX_INITIALIZER_UNLOCKED;

// Event Sequence & Records captured by WiFi.onEvent() under Critical Section
inline uint32_t globalEventSequence = 0;
inline volatile WiFiEventRecord latestStaEvent = {0, 0, (WiFiEvent_t)0, 0, false};
inline volatile WiFiEventRecord latestApEvent = {0, 0, (WiFiEvent_t)0, 0, false};
inline uint32_t lastProcessedStaSequence = 0;
inline uint32_t lastProcessedApSequence = 0;

// Explicit State Timeouts
static constexpr uint32_t STA_CONNECT_TIMEOUT_MS = 12000;
// Recovery tolerance only.
// Does NOT suppress, delay, or discard ARDUINO_EVENT_WIFI_STA_DISCONNECTED events.
// Disconnect events are still logged immediately when they occur.
// This value only widens the association window for legitimate RF delays.
// It is NOT a root-cause fix for Wi-Fi dropout and does NOT affect health timeouts.

static constexpr uint32_t STA_SCAN_TIMEOUT_MS = 10000;
static constexpr uint32_t TCP_CONNECT_TIMEOUT_MS = 2000;
static constexpr uint32_t ELM_STEP_TIMEOUT_MS = 1500;
static constexpr uint32_t CHANNEL_SYNC_TIMEOUT_MS = 3000;
static constexpr uint32_t AP_WATCHDOG_COOLDOWN_MS = 10000;
static constexpr uint32_t CIRCUIT_BREAKER_TIMEOUT_MS = 60000;
static constexpr uint32_t WAIT_FOR_ADAPTER_SCAN_INTERVAL_MS = 30000;
static constexpr uint32_t INITIAL_OBD_GENERATION = 1;

static constexpr uint32_t OBD_HEALTH_FRESH_TIMEOUT_MS        = 3000;
// Telemetry freshness threshold: telemetry is considered fresh if (now - obdLastGoodFrameMs) < 3000ms.
// Bound directly to isObdDataFresh().

static constexpr uint32_t OBD_HEALTH_WARNING_DATA_GRACE_MS   = 5000;
// Independent health grace window limit: if telemetry is not fresh (age >= 3000ms) but Wi-Fi link is healthy,
// the system holds OBD_HEALTH_WARNING until age exceeds 5000ms, at which point it transitions to DISCONNECTED.
// Mathematical contract: 3000ms <= age <= 5000ms -> WARNING; age > 5000ms -> DISCONNECTED.
// Must NOT be changed or derived from display smoothing timeouts.

static constexpr uint32_t DISPLAY_ZERO_SPEED_TIMEOUT_MS      = 5000;
// Display smoothing timeout: zeros target display speed/RPM when telemetry is missing for > 5000ms.
// Dedicated to display layer; independent of OBD_HEALTH_WARNING_DATA_GRACE_MS.

inline uint32_t calculateBackoffMs(uint8_t attempt) {
  if (attempt >= 4) return 15000UL;
  return (1000UL << attempt);
}

// Non-blocking Supervisor Restart Variables
inline bool supervisorRestartPending = false;
inline uint32_t supervisorRestartRequestMs = 0;
inline uint32_t wifiStackFailureStartMs = 0;

// Quantitative Heap Soak Diagnostics
inline uint32_t bootFreeHeap = 0;
inline uint32_t minFreeHeap = 0;
inline uint32_t minLargestFreeBlock = 0;

// Supervisor Forward Declarations
void requestStaReconnect();
void requestSupervisorRestart();
void commitConfirmedObdConfig();
void checkChannelHarmonization();
void resetObdTransactionState();
bool staHealthyForChannelSync();

// ============================================================
// WiFi Network Config for Generic ELM327 WiFi OBD2 Adapter
// ============================================================
String obd_ssid_active = "";
const char *obd_password = "";
String obd_ip_active = "192.168.0.10"; // Generic ELM327 WiFi default IP
const uint16_t tcp_port = 35000;

// ============================================================
// GPIO pin definitions for ESP32-S3 Zero (Sleek Contiguous Mapping)
// ============================================================
#define TFT_SCLK 12
#define TFT_MOSI 11
#define TFT_CS 10
#define TFT_DC 9
#define TFT_RST 8

// Color definitions (Sci-Fi Cyberpunk Palette)
#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF
#define COLOR_RED 0xF800
#define COLOR_GREEN 0x07E0 // Neon Green
#define COLOR_BLUE 0x001F
#define COLOR_CYAN 0x07FF     // Cyber Cyan
#define COLOR_YELLOW 0xFFE0   // Warning Yellow
#define COLOR_ORANGE 0xFD20   // Caution Orange
#define COLOR_GREY 0xA596     // Soft Grey
#define COLOR_DARKGREY 0x18E3 // Tech dark grey for unlit elements
#define COLOR_NAVY 0x011A     // Navy Blue (Header / Dark Blue Container)

// ── OBD Health Color / Blink / Label Helpers ─────────────────────────────────
// Depend only on obdHealthState and COLOR_* macros (must be defined above this block).
// No WiFi API calls. Safe to use from ui_themes.h and main_s3_dashboard.cpp.

inline uint16_t getObdHealthColor() {
  switch (obdHealthState) {
    case OBD_HEALTH_CONNECTED:    return COLOR_GREEN;
    case OBD_HEALTH_WARNING:      return COLOR_ORANGE;
    case OBD_HEALTH_CONNECTING:   return COLOR_ORANGE;
    case OBD_HEALTH_DISCONNECTED:
    default:                      return COLOR_RED;
  }
}

inline const char* getObdHealthLabel() {
  switch (obdHealthState) {
    case OBD_HEALTH_CONNECTED:    return "CONNECTED";
    case OBD_HEALTH_WARNING:      return "WARNING";
    case OBD_HEALTH_CONNECTING:   return "CONNECTING";
    case OBD_HEALTH_DISCONNECTED:
    default:                      return "DISCONNECTED";
  }
}

// Non-blocking blink using millis(). delay() must never be used here.
inline bool getObdHealthBlinkVisible() {
  uint32_t now = millis();
  switch (obdHealthState) {
    case OBD_HEALTH_CONNECTED:    return true;                     // Solid — always visible
    case OBD_HEALTH_WARNING:      return ((now / 500) % 2) == 0;  // ~1 Hz: 500ms ON / 500ms OFF
    case OBD_HEALTH_CONNECTING:   return ((now / 500) % 2) == 0;  // ~1 Hz
    case OBD_HEALTH_DISCONNECTED:
    default:                      return ((now / 200) % 2) == 0;  // ~2.5 Hz: 200ms ON / 200ms OFF
  }
}

// Returns draw color: health color during blink-ON, COLOR_DARKGREY during blink-OFF.
inline uint16_t getObdHealthIndicatorColor() {
  if (!getObdHealthBlinkVisible()) return COLOR_DARKGREY;
  switch (obdHealthState) {
    case OBD_HEALTH_CONNECTED:    return COLOR_GREEN;
    case OBD_HEALTH_WARNING:
    case OBD_HEALTH_CONNECTING:   return COLOR_ORANGE;
    case OBD_HEALTH_DISCONNECTED:
    default:                      return COLOR_RED;
  }
}

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
WiFiClient obdClient;

// ============================================================
// Real-time Vehicle Data Variables
// ============================================================
float batt12v = 0.00;
float batt_temp = 0.0;
int motor_rpm = 0;
float motor_temp = 0.0;
float hv_soh = 100.0;

// Trip Energy & Cost tracking
float energy_kwh          = 0.0;   // net trip energy to display (kWh, always >= 0)
float discharge_kwh       = 0.0;   // accumulated energy used (discharge only)
float regen_kwh           = 0.0;   // accumulated energy recovered (regen only)
float last_energy_raw     = -1.0;  // last raw cumulative reading (-1 = not captured yet)

float tripKwhUsed         = 0.0f;  // ค่าที่โชว์บนจอ: kWh ที่ใช้สะสมทั้งทริป
float tripRegenKwh        = 0.0f;  // พลังงาน regen สะสม (kWh)
float instantPowerKw      = 0.0f;  // กำลังไฟ ณ วินาทีนั้น (kW)
float tripCostBaht        = 0.0f;  // ค่าไฟโดยประมาณ (บาท)
float electricityRateBahtPerKwh = 4.50f; // อัตราค่าไฟต่อหน่วย (บาท)
int energySourceMode      = 0;     // 0=AUTO, 1=BMS_ACCUMULATED, 2=HV_POWER_INTEGRATION, 3=22000E_FALLBACK, 4=OFF_DEBUG
String energySourceLabel  = "NO_DATA";
bool energyValid          = false;
String energyError        = "no_valid_energy_source";

// HV Battery
float hvBatteryVoltage    = 0.0f;
float hvBatteryCurrent    = 0.0f;
float batteryPowerKw      = 0.0f;
unsigned long lastEnergyUpdateMs = 0;

// Slow Query Scheduler & Candidate Validation
#define SLOW_QUERY_INTERVAL_MS 2000
unsigned long lastSlowQueryTime = 0;          // Time of last slow query pass
bool accumulatedDischargeValidated = false;    // Validation status of PID VP_PID_ACC_DISCHARGE
float lastAccumulatedDischargeVal = -1.0f;     // Monotonic value checker
int monotonicCount = 0;                       // Monotonic confirmation count

// HV Voltage & Current Validation Telemetry
bool hvVoltageValid = false;
bool hvCurrentValid = false;
unsigned long hvVoltageLastUpdateMs = 0;
unsigned long hvCurrentLastUpdateMs = 0;
float energy22000EAdjustKwh = 0.0f; // BMS adjustment debug tracking

// BMS Accumulated
float accumulatedDischargeNow   = 0.0f;
float accumulatedDischargeStart = -1.0f;

// VP_PID_ENERGY fallback
float energy22000Eraw     = 0.0f;
float energy22000Edelta   = 0.0f;

float speed_raw_smooth    = -1.0;  // smoothed VP_PID_AUX raw value for jitter filtering
int consecutiveCanFailures =
    0; // Track consecutive CAN failures to detect when car is off
float batt_soc = -1.0f;
float socCandidate = -1.0f;

// ============================================================
// SOC State — BYD Dolphin SOC V1.0
// ============================================================

// BMS Coarse SOC: from PID VP_PID_SOC (integer 0–100, stored as float)
// Updated ONLY on confirmed positive response (62 00 05 XX).
// Malformed / negative responses must NOT touch this state.
//
// [R7] socCoarseValid semantics:
//   true  = "the most recent VP_PID_SOC sample passed parser validation"
//   false = "no valid sample has ever been received this session"
//   socCoarseValid does NOT mean "the current sample is fresh".
//   Freshness is always evaluated separately:
//     (uint32_t)(now - socCoarseLastUpdateMs) <= SOC_DATA_TIMEOUT_MS
//   Do NOT set socCoarseValid = false just because sample becomes stale.
inline float    socBmsCoarse          = -1.0f;
inline bool     socCoarseValid        = false;
inline uint32_t socCoarseLastUpdateMs = 0;
// ^ [R11] All freshness checks use: (uint32_t)(now - socCoarseLastUpdateMs)
//         This is safe across millis() rollover (~49.7 day wrap).
// ^ [R16] Written ONLY by validated VP_PID_SOC parser. NEVER mutated by resolver.
// ^ = ESP32 uptime ms when the validated VP_PID_SOC response was processed/received by the ESP32 [R5]
// ^ NOT a Unix timestamp. NOT the time the car transmitted.

// BMS Detailed SOC: reserved for future VP_PID_SOH field validation
// [R12] HARD LOCK: socDetailedValid is initialized false and has NO V1.0
// write path that can set it true.
inline float    socBmsDetailed          = -1.0f;
inline bool     socDetailedValid        = false;
inline uint32_t socDetailedLastUpdateMs = 0;

// Final resolved SOC — [R14.1] written EXCLUSIVELY by resolveBydSoc()
inline bool     socFinalValid        = false;
inline uint32_t socFinalLastUpdateMs = 0;
// ^ = socCoarseLastUpdateMs when source is VP_PID_SOC (source timestamp rule)
//
// [R8] socFinalLastUpdateMs semantics:
//   Retains the timestamp of the most recent VALID resolved SOC,
//   even after that SOC becomes stale/invalid.
//   When socFinalValid == false, soc_last_update MUST NOT be
//   interpreted as proof that current SOC is fresh.
//   Current validity/freshness is determined by soc_valid ONLY.

// Cross-check diagnostic — written EXCLUSIVELY by resolveBydSoc() [reset to -1.0f in V1]
inline float socSourceDifference = -1.0f;

// SOC Source enum
enum SocSource : uint8_t {
    SOC_SOURCE_NONE         = 0,
    SOC_SOURCE_BMS_DETAILED = 1,   // Future: validated VP_PID_SOH field
    SOC_SOURCE_BMS_COARSE   = 2    // Active in V1.0: PID VP_PID_SOC
};
inline SocSource socSource = SOC_SOURCE_NONE;

// SOC timing constants
// [R11] SOC_DATA_TIMEOUT_MS used in: (uint32_t)(now - timestamp) <= SOC_DATA_TIMEOUT_MS
// Boundary: at exactly 15,000 ms = VALID/FRESH; at 15,001 ms = STALE/INVALID.
static constexpr uint32_t SOC_DATA_TIMEOUT_MS    = 15000;  // 15s freshness window
static constexpr float    SOC_CROSSCHECK_MAX_DIFF =  1.0f; // diagnostic diff threshold (%)
int vehicle_speed = 0;
float vehicleSpeedKmh = 0.0f;
unsigned long lastRpmUpdateTime = 0;
unsigned long lastRpmActualTime = 0; // Last time actual MMCU RPM was parsed

int config_slots[6] = {0, 1, 2, 3, 4, 5};
int car_profile = 0; // 0 = EV BYD Dolphin, 1 = Generic EV, 2 = Generic ICE
int dolphin_test_mode = DEFAULT_DOLPHIN_TEST; // 0=A, 1=B, 2=C, 3=D, 4=E, 5=F

// ============================================================
// Vehicle Profile Management Framework
// ============================================================
struct VehicleProfile {
  const char* id;
  const char* brand;
  const char* model;
  const char* market;
  const char* powertrain;
  const char* status;      // CONFIRMED / DISABLED / PLACEHOLDER_ONLY / WAITING_FOR_PROFILE_DATA
  const char* speedPid;
  const char* speedHeader;
  float speedDivisor;
  float rpmFactor;
  bool selectableInProduction;
  float usableBatteryKwh;  // Usable battery capacity in kWh
};

// ============================================================
// SOC Profile Configuration — BYD Dolphin SOC V1.0
//
// ARCHITECTURE LOCK (R6):
// This struct is metadata/forward-compatible ONLY.
// The existence of this struct does NOT authorize:
//   - Creating a generic multi-brand resolver
//   - Moving all PIDs into profile-driven architecture
//   - Refactoring the OBD polling engine for profiles
//   - Activating secondary/detailed sources without validation
//
// V1.0: BYD Dolphin source correctness FIRST.
// ============================================================
struct SocProfileConfig {
    const char* primarySocPid;     // Active V1 source: ดู VP_REQ_SOC ใน vehicle_calibration.h
    float       primarySocScale;   // 1.0 = integer direct (1% real resolution)
    const char* secondarySocPid;   // Future placeholder — NOT active
    float       secondarySocScale; // 0.0 = not active in V1
    float       crossCheckMaxDiff; // Diagnostic threshold (%)
};

// BYD Dolphin SOC profile constant
inline const SocProfileConfig BYD_DOLPHIN_SOC_PROFILE = {
    VP_REQ_SOC,  // primarySocPid   — active
    1.0f,      // primarySocScale — integer direct, 1% real resolution
    VP_REQ_SOH,  // secondarySocPid — placeholder, NOT active until byte mapping validated
    0.0f,      // secondarySocScale — 0.0 = not active
    1.0f       // crossCheckMaxDiff
};

inline const VehicleProfile VEHICLE_PROFILES[] = {
  {
    "BYD_DOLPHIN_TH_EV",
    "BYD",
    "Dolphin",
    "TH",
    "EV",
    "CONFIRMED_BY_PROJECT_LOG",
    VP_REQ_SPEED,
    VP_HDR_SPEED,
    VP_SPEED_DIVISOR,
    VP_RPM_FACTOR,
    true,
    44.9f // usableBatteryKwh
  },
  {
    "MG_ZS_EV_TH",
    "MG",
    "ZS EV",
    "TH",
    "EV",
    "WAITING_FOR_PROFILE_DATA",
    "",
    "",
    0.0f,
    0.0f,
    false,
    44.5f // generic fallback usableBatteryKwh
  },
  {
    "ORA_GOOD_CAT_TH",
    "GWM ORA",
    "Good Cat",
    "TH",
    "EV",
    "WAITING_FOR_PROFILE_DATA",
    "",
    "",
    0.0f,
    0.0f,
    false,
    47.8f // generic fallback usableBatteryKwh
  },
  {
    "TOYOTA_YARIS_TH",
    "Toyota",
    "Yaris",
    "TH",
    "ICE",
    "WAITING_FOR_PROFILE_DATA",
    "",
    "",
    0.0f,
    0.0f,
    false,
    0.0f
  }
};
inline const int NUM_VEHICLE_PROFILES = sizeof(VEHICLE_PROFILES) / sizeof(VEHICLE_PROFILES[0]);

inline String selected_vehicle_profile = "BYD_DOLPHIN_TH_EV";

// Helper declarations
const VehicleProfile* findVehicleProfile(const String& id);
bool isValidObdIp(const String& ipStr);

// ============================================================
// AutoBack Feature Configuration and State Variables
// ============================================================
inline bool autoBackEnabled = true;
inline int autoBackPageType = 0; // 0 = GIF Loop, 1 = Robo Eye, 2 = Stopwatch
inline bool autoBackActive = false;
inline uint32_t stoppedDurationMs = 0;
inline uint32_t movingDurationMs = 0;
inline int previousPageBeforeAutoBack = 1;
inline uint32_t autoBackStopwatchStartMs = 0;
inline bool chargeAutoBackActive = false;
inline bool chargeAutoBackSessionUsed = false;
inline uint32_t chargeEntryConfirmMs = 0;
inline uint32_t chargeExitConfirmMs = 0;

// ============================================================
// Background Driving Time Logger Variables (Temporarily Disabled)
// ============================================================
/*
inline uint32_t drivingSecondsToday = 0;
inline uint32_t drivingSecondsThisWeek = 0;
inline uint32_t drivingTimeAccumulatedMs = 0;
inline uint32_t lastDrivingPersistMs = 0;
inline String currentDate = "NoSync";
*/

// ============================================================
// Simplified Charging Mode Configuration and State Variables
// ============================================================
enum ChargeEstimateState {
  CHG_EST_NOT_CHARGING = 0,
  CHG_EST_CANDIDATE    = 1,
  CHG_EST_ACTIVE       = 2,
  CHG_EST_END_CANDIDATE = 3
};

#define CHARGE_EST_MIN_POWER_SAMPLES   5       // Minimum 5 valid 1-sec samples before ETA is VALID
#define CHARGE_EST_ENTRY_DEBOUNCE_MS   5000    // 5s continuous confirmation
#define CHARGE_EST_EXIT_DEBOUNCE_MS    10000   // 10s continuous exit confirmation
#define CHARGE_EST_MIN_POWER_KW        0.3f    // Minimum valid charging power

inline bool inChargingMode = false;
inline int chargingHomeType = 0; // 0 = GIF Loop, 1 = Robo Eye
inline int chargeTargetSoc = 80; // 80%, 90%, 100%
inline int chargingSubPage = 0;  // 0 = CHARGING_ESTIMATE_PAGE, 1 = CHARGING_COUNT_PAGE, 2 = CHARGING_HOME_PAGE
inline float avgChargePowerKw = 0.0f;
inline int previousPageBeforeCharging = 1;
inline float chargePowerHistory[120];
constexpr size_t CHARGE_EST_HISTORY_CAPACITY = sizeof(chargePowerHistory) / sizeof(chargePowerHistory[0]);
static_assert(CHARGE_EST_HISTORY_CAPACITY >= CHARGE_EST_MIN_POWER_SAMPLES,
              "Charge Estimate history buffer too small");
inline int chargeHistoryIdx = 0;
inline int chargeHistoryCount = 0;
inline unsigned long lastHistoryUpdateMs = 0;

inline ChargeEstimateState chargeEstimateState = CHG_EST_NOT_CHARGING;
inline bool chargeDetectedRaw                 = false;
inline bool chargeDirectionValid              = false;
inline float chargeInstantPowerKw             = 0.0f;
inline bool chargePowerValid                  = false;

inline bool chargeAutoBackConfigArmed         = false;
// Session authorization snapshot:
// Latched once when CHG_EST_CANDIDATE begins.
// TRUE means this charging session is authorized for Charge Estimate AutoBack.
// Must not be changed by live autoBackPageType changes during the session.
// May be cleared only by Candidate abort, explicit AutoBack OFF,
// or true charging-session termination.

inline bool chargeEstimateAutoBackOwnedScreen = false; // Session-scoped screen ownership latch

inline bool chargeHvReady                     = false; // Mirrored from single-capture snapshot
inline bool chargeObdFresh                    = false; // Mirrored from single-capture snapshot

inline uint32_t chargeStateTimerMs            = 0;
inline uint32_t lastChargeSampleMs            = 0;

inline float chargeSocDelta                   = 0.0f;
inline float chargeEnergyNeededKwh            = 0.0f;
inline float chargeEtaMinutes                 = -1.0f;
inline String chargeEtaStatusStr              = "NO_CHG";

#define CHARGING_ESTIMATE_PAGE 0
#define CHARGING_COUNT_PAGE    1
#define CHARGING_HOME_PAGE     2

// ============================================================
// Charge Telemetry & Session Variables (A/C Charging Fix)
// ============================================================
inline float accumulatedChargeNow = -1.0f;
inline float accumulatedChargeStart = -1.0f;
inline float lastAccumulatedChargeVal = -1.0f;
inline int monotonicChargeCount = 0;
inline bool accumulatedChargeValidated = false;

inline int basuChargeCount = -1;
inline int lastSavedChargeCount = -1;
inline bool chargeSessionDetected = false;

inline float lastChargeEnergySampleKwh = -1.0f;
inline uint32_t lastChargeEnergyIncreaseMs = 0;



// ============================================================
// Energy Comparison Note Variables (Debug mode)
// ============================================================
inline float carDisplayEnergyVal = 0.0f;
inline String carDisplayEnergyUnit = "kWh";
inline String carDisplayEnergyLabel = "";

// ============================================================
// WiFi/TCP Debug & Status Variables
// ============================================================
inline int wifiDisconnectReason = 0;
// Note: wifiReconnectCount is aliased to wifiDropCount in cycle tracking above
inline int tcpReconnectCount = 0;
inline unsigned long lastWiFiDisconnectMs = 0;
inline unsigned long lastTCPDisconnectMs = 0;
inline int wifiRssi = 0;
inline String connectionMode = "SCAN"; // "STATIC", "DHCP", "SCAN"

// OBD Connection Health Status Variables
inline bool obdConnected = false;
inline bool obdConnecting = false;
inline unsigned long obdLastGoodFrameMs = 0;
inline unsigned long obdLastResponseMs = 0;
inline uint32_t obdReconnectCount = 0;
inline uint32_t obdConsecutiveTimeouts = 0;
inline String obdLastErrorReason = "None";

// Display-Only Target Variables ( Grace Display )
inline float displayTargetSpeed = 0.0f;
inline float displayTargetRpm = 0.0f;

// Energy Integration Debug Variables
inline float rawPowerKw = 0.0f;
inline float dischargePowerKw = 0.0f;
inline float regenPowerKw = 0.0f;
inline uint32_t hvVoltageAgeMs = 0;
inline uint32_t hvCurrentAgeMs = 0;
inline bool integrationAllowed = false;
inline String integrationSkipReason = "None";
inline uint32_t lastEnergyIntegrationDtMs = 0;
inline bool energySessionStarted = false;

// Temporary Charging Debug Variables
inline float dbgInstantChargePowerKw = 0.0f;
inline bool dbgBatterySideCharging = false;
inline bool dbgChargingEnterCondition = false;
inline bool dbgHvPowerReady = false;
inline uint32_t dbgChargingCandidateMs = 0;
inline uint32_t dbgHvVoltageAgeMs = 0;
inline uint32_t dbgHvCurrentAgeMs = 0;


// ============================================================
// Display Smoothing Layer
// ============================================================
// These values are ONLY for TFT display rendering.
// They must NEVER be used for calibration, logging, or formula calculations.
// Raw/target values: vehicle_speed, motor_rpm (set by obd_handler)
// Smoothed display: displaySpeedKmh, displayRpm (set by UI loop)
float displaySpeedKmh = 0.0f;
float displayRpm      = 0.0f;
unsigned long lastSmoothTime = 0;

// ============================================================
// RPM Source Mode (selectable via Web Portal, saved in NVS)
// ============================================================
// 0 = AUTO: Use actual MMCU RPM if valid, fallback to estimated
// 1 = EST:  Force estimated RPM (speedKmh * VP_RPM_FACTOR) always
// 2 = MMCU: Force actual MMCU RPM only, NO fallback
int rpmSourceMode = 0;

// ============================================================
// MMCU RPM Debug Tracking (PID VP_PID_MOTOR_RPM)
// ============================================================
int   mmcuRpmRaw         = 0;      // Raw signed RPM value from VP_PID_MOTOR_RPM
int   mmcuRpmDisplayAbs  = 0;      // abs(mmcuRpmRaw) for display
bool  mmcuRpmValid       = false;  // Whether last MMCU reading was valid
int   mmcuResponseCount  = 0;      // Total successful VP_PID_MOTOR_RPM responses
unsigned long mmcuLastUpdateMs = 0; // Last successful VP_PID_MOTOR_RPM timestamp
String mmcuLastError     = "not_implemented"; // Last error state

// Active RPM source label for debug/display
// "EST" / "REAL_MMCU" / "AUTO_FALLBACK_EST" / "MMCU_NO_DATA"
String rpmSourceLabel    = "EST";

unsigned long lastQueryTime = 0;
bool rxBlinkState = false;
AnimatedGIF gif;
bool theme2GifOpen = false;
bool themeChanged = true;
bool wasStationConnected = false;
bool forceObdReconnect = false;
Preferences preferences;

WebServer server(80);
File uploadFile;
bool uploadError = false;
String uploadErrorMsg = "";
const size_t WRITE_BUFF_SIZE = 4096;
uint8_t writeBuffer[WRITE_BUFF_SIZE];
size_t writeBufferLen = 0;
bool pendingRestart = false;
unsigned long restartRequestTime = 0;

inline String uploadSuccessPath = "";
inline size_t uploadSuccessSize = 0;
inline String uploadSuccessHeader = "";
inline bool uploadInProgress = false;
inline unsigned long lastUploadActivityMs = 0;
inline bool loopGifReloadRequested = false;
inline bool bootGifReloadRequested = false;
inline bool showSetupScreen = true;
inline bool gifOpenFailed = false;
inline unsigned long lastGifOpenAttemptMs = 0;
inline String lastGifSource = "none";
inline bool lastGifOpenResult = false;
inline String lastGifOpenError = "";

// Enable or disable diagnostic and clearing endpoints to manage firmware space
#define ENABLE_GIF_DEBUG 1

// Diagnostic state for detailed error JSON
inline String uploadDiagTarget = "";
inline String uploadDiagTemp = "";
inline String uploadDiagBackup = "";
inline bool uploadDiagTempExists = false;
inline size_t uploadDiagTempSize = 0;
inline String uploadDiagTempHeader = "";
inline bool uploadDiagFinalExistsBefore = false;
inline size_t uploadDiagFinalSizeBefore = 0;
inline String uploadDiagFinalHeaderBefore = "";
inline bool uploadDiagBackupExistsBefore = false;
inline bool uploadDiagBackupRemoveOk = false;
inline bool uploadDiagBackupRenameOk = false;

File gifFile;
uint16_t gifCanvas[320 * 172];
