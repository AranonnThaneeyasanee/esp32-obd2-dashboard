#include "emergency_boot_gif.h"
#include "emergency_loop_gif.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <AnimatedGIF.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <vector>
#include <algorithm>

// Global Configurations and Variables
#include "dashboard_config.h"

DNSServer dnsServer;

// SoftAP Client connection state for Setup Portal
volatile bool setupApClientConnected = false;
volatile bool setupApClientDisconnectPending = false;

static uint32_t setupApDisconnectCandidateMs = 0;
static constexpr uint32_t SETUP_AP_DISCONNECT_DEBOUNCE_MS = 400;

static constexpr uint8_t SETUP_AP_MAX_CONNECTIONS = 4;
static const IPAddress SETUP_AP_IP(192, 168, 4, 1);
static const IPAddress SETUP_AP_GATEWAY(192, 168, 4, 1);
static const IPAddress SETUP_AP_SUBNET(255, 255, 255, 0);

// Wi-Fi Power-Save Enforcer
inline void enforcePowerSavePolicy() {
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
}

// Heap Guard Logger
inline void logHeapGuard(const char* tag) {
  Serial.printf("[MEM][%s] free=%u min=%u\n", tag, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap());
}

// Wi-Fi Supervisor: Single-Writer Request for STA Reconnect
inline void requestStaReconnect() {
  currentStaState = STA_STATE_IDLE;
  currentTcpState = TCP_STATE_IDLE;
  currentElmState = ELM_INIT_IDLE;
  Serial.println("[WIFI][SUPERVISOR] STA Reconnect requested.");
}

// Wi-Fi Supervisor: Non-Blocking Restart Request (Zero delay, Idempotent)
inline void requestSupervisorRestart() {
  if (!supervisorRestartPending) {
    supervisorRestartPending = true;
    supervisorRestartRequestMs = millis();
    Serial.println("[WIFI][SUPERVISOR] Level-4 restart requested non-blockingly.");
  }
}

// Wi-Fi Supervisor: Dual-Slot NVS Staging with RFC 1982 Serial-Number Arithmetic & Canonical CRC-8
void commitConfirmedObdConfig() {
  String activeBssid = WiFi.BSSIDstr();
  uint8_t activeChannel = WiFi.channel();

  uint8_t bssidBytes[6] = {0};
  bool bssidOk = parseMacAddress(activeBssid, bssidBytes);
  bool channelOk = (activeChannel >= 1 && activeChannel <= 14);
  bool ssidOk = (obd_ssid_active.length() > 0);
  bool ipOk = isValidObdIp(obd_ip_active);

  // 1. Mandatory 4-Field Semantic Pre-Validation
  if (!ssidOk || !ipOk || !bssidOk || !channelOk) {
    Serial.printf("[WIFI][SUPERVISOR] Incomplete 4-field config (ssid=%d, ip=%d, bssid=%d, ch=%u) — skipping commit.\n",
                  ssidOk, ipOk, bssidOk, activeChannel);
    return;
  }

  // 2. Same-Config No-Op Optimization
  if (obdConfigValid &&
      obd_ssid_active == savedObdSsid &&
      obd_ip_active == savedObdIp &&
      activeBssid == savedObdBssid &&
      activeChannel == savedObdChannel) {
    Serial.println("[WIFI][SUPERVISOR] Confirmed config matches current trusted slot — clean no-op.");
    return;
  }

  // 3. Staging Slot Resolution & Monotonic Generation Increment
  uint8_t targetSlot = (currentActiveSlot == 0) ? 1 : 0;
  uint32_t newGen = (savedObdGen == 0) ? INITIAL_OBD_GENERATION : (savedObdGen + 1);

  uint8_t crc = calcObdConfigCrc8(obd_ssid_active, obd_ip_active, bssidBytes, activeChannel, newGen);

  String validKey = (targetSlot == 0) ? "obd_valid_0" : "obd_valid_1";
  String ssidKey = (targetSlot == 0) ? "obd_ssid_0" : "obd_ssid_1";
  String ipKey = (targetSlot == 0) ? "obd_ip_0" : "obd_ip_1";
  String bssidKey = (targetSlot == 0) ? "obd_bssid_0" : "obd_bssid_1";
  String chKey = (targetSlot == 0) ? "obd_ch_0" : "obd_ch_1";
  String genKey = (targetSlot == 0) ? "obd_gen_0" : "obd_gen_1";
  String chkKey = (targetSlot == 0) ? "obd_chk_0" : "obd_chk_1";

  if (!preferences.begin("obd2_dash", false)) {
    Serial.println("[WIFI][SUPERVISOR] [ERROR] Failed to open NVS write mode — retaining last known-good config.");
    return;
  }

  bool ok = true;
  ok &= (preferences.putBool(validKey.c_str(), false) > 0);
  ok &= (preferences.putString(ssidKey.c_str(), obd_ssid_active) > 0);
  ok &= (preferences.putString(ipKey.c_str(), obd_ip_active) > 0);
  ok &= (preferences.putString(bssidKey.c_str(), activeBssid) > 0);
  ok &= (preferences.putUChar(chKey.c_str(), activeChannel) > 0);
  ok &= (preferences.putUInt(genKey.c_str(), newGen) > 0);
  ok &= (preferences.putUChar(chkKey.c_str(), crc) > 0);
  ok &= (preferences.putBool(validKey.c_str(), true) > 0);
  preferences.end();

  if (!ok || !preferences.begin("obd2_dash", true)) {
    Serial.println("[WIFI][SUPERVISOR] [ERROR] NVS write verification open failed — retaining last known-good config.");
    return;
  }

  bool readValid = preferences.getBool(validKey.c_str(), false);
  String readSsid = preferences.getString(ssidKey.c_str(), "");
  String readIp = preferences.getString(ipKey.c_str(), "");
  String readBssid = preferences.getString(bssidKey.c_str(), "");
  uint8_t readChannel = preferences.getUChar(chKey.c_str(), 0);
  uint32_t readGen = preferences.getUInt(genKey.c_str(), 0);
  uint8_t readCrc = preferences.getUChar(chkKey.c_str(), 0);
  preferences.end();

  uint8_t readBssidBytes[6] = {0};
  bool readBssidOk = parseMacAddress(readBssid, readBssidBytes);
  uint8_t expectedCrc = calcObdConfigCrc8(readSsid, readIp, readBssidBytes, readChannel, readGen);

  bool verified = readValid &&
                  readBssidOk &&
                  (readSsid == obd_ssid_active) &&
                  (readIp == obd_ip_active) &&
                  (readBssid == activeBssid) &&
                  (readChannel == activeChannel) &&
                  (readGen == newGen) &&
                  (readCrc == expectedCrc);

  if (verified) {
    currentActiveSlot = targetSlot;
    savedObdSsid = readSsid;
    savedObdIp = readIp;
    savedObdBssid = readBssid;
    savedObdChannel = readChannel;
    savedObdGen = readGen;
    obdConfigValid = true;

    Serial.printf("[WIFI][SUPERVISOR] Staged NVS Commit Verified! Slot=%u, Gen=%u, SSID=%s, BSSID=%s, Ch=%u, IP=%s\n",
                  currentActiveSlot, savedObdGen, savedObdSsid.c_str(), savedObdBssid.c_str(), savedObdChannel, savedObdIp.c_str());
  } else {
    Serial.printf("[WIFI][SUPERVISOR] [ERROR] Staged NVS verification failed on slot %u — retaining last known-good config.\n", targetSlot);
  }
}

// Wi-Fi Supervisor: Verified Actual Channel Harmonization (Exclusive Owner)
void checkChannelHarmonization() {
  static uint32_t lastChannelSyncAttemptMs = 0;
  static uint32_t lastDeferLogMs = 0;

  if (channelSyncState == CHANNEL_SYNC_PENDING) {
    uint32_t now = millis();

    // ── 1. SoftAP Availability Gate: Retain PENDING if SoftAP is not enabled / active ──
    // MUST precede Target Validation and Target Guard to prevent declaring OK when SoftAP is unavailable.
    // Returns immediately without forcing CHANNEL_SYNC_OK (which would be false success).
    // Harmonization will retry automatically when SoftAP becomes available.
    bool apAvailable = ((WiFi.getMode() & WIFI_MODE_AP) != 0) && (WiFi.softAPIP() != IPAddress(0, 0, 0, 0));
    if (!apAvailable) {
      return; // Retain PENDING
    }

    // ── 2. Target Validation Guard: Do not force OK if target channel is invalid ──
    if (targetApChannel < 1 || targetApChannel > 14) {
      if (now - lastDeferLogMs >= 5000) {
        lastDeferLogMs = now;
        Serial.printf("[WIFI][CHANNEL][DEFER] reason=INVALID_TARGET targetApChan=%u\n", (unsigned)targetApChannel);
      }
      channelSyncState = CHANNEL_SYNC_PENDING; // Retain PENDING, NEVER set false OK
      return;
    }

    // ── 3. Target Guard: Do not reconfigure AP if target channel matches current channel ──
    if (targetApChannel == currentApChannel) {
      channelSyncState = CHANNEL_SYNC_OK;
      return;
    }

    // ── 4. P0: Web/AP Protection Gate (Absolute Priority) ─────────────────
    uint8_t apClientCount = WiFi.softAPgetStationNum();
    bool webApProtected = setupApClientConnected ||
                          (apClientCount > 0) ||
                          uploadInProgress ||
                          (currentPortalState != PORTAL_STATE_INACTIVE);
    if (webApProtected) {
      if (now - lastDeferLogMs >= 5000) {
        lastDeferLogMs = now;
        Serial.printf("[WIFI][CHANNEL][DEFER] reason=WEB_AP_PROTECTED apClients=%u setupClient=%d upload=%d portalState=%d staChan=%u targetApChan=%u\n",
                      (unsigned)apClientCount,
                      (int)setupApClientConnected,
                      (int)uploadInProgress, (int)currentPortalState,
                      (unsigned)(WiFi.status() == WL_CONNECTED ? WiFi.channel() : 0),
                      (unsigned)targetApChannel);
      }
      return; // Defer AP channel sync — active Web/AP takes absolute priority
    }

    // ── 5. P1: Active OBD Telemetry Protection Gate ───────────────────────
    if (staHealthyForChannelSync()) {
      if (now - lastDeferLogMs >= 5000) {
        lastDeferLogMs = now;
        Serial.printf("[WIFI][CHANNEL][DEFER] reason=OBD_HEALTHY staChan=%u targetApChan=%u\n",
                      (unsigned)WiFi.channel(), (unsigned)targetApChannel);
      }
      return; // Defer AP channel sync — active OBD link takes priority
    }

    // ── 6. P2: Channel Harmonization (Only when P0 & P1 gates are clear) ──
    if (now - lastChannelSyncAttemptMs >= 3000) {
      lastChannelSyncAttemptMs = now;

      // Last-moment race condition check immediately before mutation:
      // 1. Re-verify P0 and P1
      // 2. Re-verify SoftAP is still active with valid IP
      // 3. Verify STA channel hasn't drifted or disconnected
      bool apAvailableNow = ((WiFi.getMode() & WIFI_MODE_AP) != 0) && (WiFi.softAPIP() != IPAddress(0, 0, 0, 0));
      uint8_t liveStaChan = (WiFi.status() == WL_CONNECTED) ? WiFi.channel() : 0;
      if (setupApClientConnected || (WiFi.softAPgetStationNum() > 0) ||
          uploadInProgress || (currentPortalState != PORTAL_STATE_INACTIVE) ||
          staHealthyForChannelSync() || !apAvailableNow ||
          (liveStaChan < 1 || liveStaChan > 14 || liveStaChan != targetApChannel)) {
        channelSyncState = CHANNEL_SYNC_PENDING;
        return; // Abort sync — condition changed in intervening time
      }

      channelSyncState = CHANNEL_SYNC_IN_PROGRESS;
      Serial.printf("[WIFI][SUPERVISOR] Harmonizing AP channel to STA channel %u\n", targetApChannel);
      bool apOk = WiFi.softAP("OBD2-Dashboard", "12345678", targetApChannel, false, SETUP_AP_MAX_CONNECTIONS);
      enforcePowerSavePolicy();
      
      // Strict verification of actual hardware SoftAP channel via ESP-IDF driver
      wifi_config_t ap_conf;
      esp_err_t err = esp_wifi_get_config(WIFI_IF_AP, &ap_conf);
      uint8_t actualApChan  = (err == ESP_OK) ? ap_conf.ap.channel : 0;
      uint8_t actualStaChan = (WiFi.status() == WL_CONNECTED) ? WiFi.channel() : targetApChannel;

      // Resolution Invariant: CHANNEL_SYNC_IN_PROGRESS always resolves to OK or PENDING
      if (apOk && (actualApChan == targetApChannel) && (actualApChan == actualStaChan) && (WiFi.softAPIP() == SETUP_AP_IP)) {
        currentApChannel = targetApChannel;
        channelSyncState = CHANNEL_SYNC_OK;
        Serial.printf("[WIFI][SUPERVISOR] Channel harmonization verified! Actual Hardware SoftAP Channel=%u (Matches STA Channel=%u)\n",
                      currentApChannel, actualStaChan);
      } else {
        Serial.printf("[WIFI][SUPERVISOR] Channel harmonization verification failed (actualAp=%u, actualSta=%u, target=%u) — will retry.\n",
                      actualApChan, actualStaChan, targetApChannel);
        channelSyncState = CHANNEL_SYNC_PENDING;
      }
    }
  }
}

// HTML page template for Setup Portal
#include "web_portal_html.h"

// Filesystem and HTTP Upload Handlers
#include "web_handlers.h"

// OBD2 query parser and connection handlers
#include "obd_handler.h"

// Touch sensor checking logic
#include "touch_handler.h"

// GIF drawing routines
#include "gif_player.h"

// UI Gauges and Theme drawing routines
#include "ui_themes.h"

// Helper implementation to search for a vehicle profile by ID
const VehicleProfile *findVehicleProfile(const String &id) {
  for (int i = 0; i < NUM_VEHICLE_PROFILES; i++) {
    if (String(VEHICLE_PROFILES[i].id) == id) {
      return &VEHICLE_PROFILES[i];
    }
  }
  return nullptr;
}

// IP address format validation helper
bool isValidObdIp(const String &ipStr) {
  if (ipStr.length() < 7 || ipStr.length() > 15)
    return false;
  int dots = 0;
  for (int i = 0; i < ipStr.length(); i++) {
    char c = ipStr.charAt(i);
    if (c == '.')
      dots++;
    else if (c < '0' || c > '9')
      return false;
  }
  if (dots != 3)
    return false;

  // Parse parts
  int parts[4] = {-1, -1, -1, -1};
  int partIdx = 0;
  String currentPart = "";
  for (int i = 0; i < ipStr.length(); i++) {
    char c = ipStr.charAt(i);
    if (c == '.') {
      parts[partIdx++] = currentPart.toInt();
      currentPart = "";
    } else {
      currentPart += c;
    }
  }
  parts[partIdx] = currentPart.toInt();

  for (int i = 0; i < 4; i++) {
    if (parts[i] < 0 || parts[i] > 255)
      return false;
  }

  // Subnet must be reasonable: first three octets shouldn't be 0 or 255
  if (parts[0] == 0 || parts[0] == 255)
    return false;

  return true;
}

// ============================================================
// File Handlers for Background Driving Time Logger (JSON)
// ============================================================

#define ENABLE_DEBUG_API 1

// Helper: Escape string for clean JSON output
String escapeJsonString(const String& input) {
  String output = "";
  for (size_t i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    if (c == '\\') {
      output += "\\\\";
    } else if (c == '"') {
      output += "\\\"";
    } else if (c == '\n') {
      output += "\\n";
    } else if (c == '\r') {
      output += "\\r";
    } else if (c == '\t') {
      output += "\\t";
    } else if (c < 32) {
      char buf[8];
      snprintf(buf, sizeof(buf), "\\u%04x", c);
      output += buf;
    } else {
      output += c;
    }
  }
  return output;
}

#if 0
// Helper: validate YYYY-MM-DD format and convert to absolute day count
// Returns -1 if the date string is invalid (including impossible calendar dates)
int32_t dateToDays(const String& dateStr) {
  if (dateStr.length() != 10) return -1;
  if (dateStr.charAt(4) != '-' || dateStr.charAt(7) != '-') return -1;
  for (int i = 0; i < 10; i++) {
    if (i == 4 || i == 7) continue;
    char c = dateStr.charAt(i);
    if (c < '0' || c > '9') return -1;
  }
  int y = dateStr.substring(0, 4).toInt();
  int m = dateStr.substring(5, 7).toInt();
  int d = dateStr.substring(8, 10).toInt();
  if (y < 2020 || y > 2099 || m < 1 || m > 12 || d < 1) return -1;
  // Validate actual days in month
  bool isLeap = ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0));
  const int maxDays[] = {31, (isLeap ? 29 : 28), 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (d > maxDays[m - 1]) return -1;
  // Compute absolute day count using a simple formula
  // (sufficient for date difference within a few years)
  int32_t days = y * 365 + d;
  // Add days for completed months
  const int mdays[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  days += mdays[m - 1];
  // Leap year adjustment
  if (m > 2 && isLeap) days++;
  days += (y / 4) - (y / 100) + (y / 400);
  return days;
}

enum MergeResult {
  MERGE_FAILED = 0,
  MERGE_SUCCESS_NO_NOSYNC = 1,
  MERGE_SUCCESS_MERGED = 2
};

MergeResult mergeNoSyncDate(String realDate) {
  realDate.trim();
  Serial.printf("[DrivingTime] mergeNoSyncDate() started. target date: '%s'\n", realDate.c_str());
  if (realDate == "" || realDate == "NoSync" || realDate == "undefined" || realDate == "null") {
    Serial.println("[DrivingTime] mergeNoSyncDate: Target date is invalid/empty/NoSync!");
    return MERGE_FAILED;
  }

  if (!LittleFS.exists("/driving_time.json")) {
    Serial.println("[DrivingTime] mergeNoSyncDate: /driving_time.json does not exist. No need to merge.");
    return MERGE_SUCCESS_NO_NOSYNC;
  }

  File f = LittleFS.open("/driving_time.json", "r");
  if (!f) {
    Serial.println("[DrivingTime] mergeNoSyncDate: Failed to open /driving_time.json!");
    return MERGE_FAILED;
  }
  String json = f.readString();
  f.close();

  bool hasNoSync = (json.indexOf("\"date\":\"NoSync\"") != -1);
  Serial.printf("[DrivingTime] mergeNoSyncDate: NoSync exists in file: %s\n", hasNoSync ? "YES" : "NO");
  if (!hasNoSync) {
    return MERGE_SUCCESS_NO_NOSYNC; // No un-synced data to merge
  }

  uint32_t noSyncSeconds = 0;
  uint32_t realDateSeconds = 0;
  bool hasRealDate = false;
  String daysJson = "";

  int startIdx = json.indexOf("[");
  int endIdx = json.lastIndexOf("]");
  if (startIdx != -1 && endIdx != -1) {
    String arrayContent = json.substring(startIdx + 1, endIdx);
    int itemStart = 0;
    while (itemStart < arrayContent.length()) {
      int itemEnd = arrayContent.indexOf("}", itemStart);
      if (itemEnd == -1)
        itemEnd = arrayContent.length();
      String item = arrayContent.substring(itemStart, itemEnd + 1);
      item.trim();
      if (item.length() > 0) {
        if (item.indexOf("\"date\":\"NoSync\"") != -1) {
          int secIdx = item.indexOf("\"seconds\":");
          if (secIdx != -1) {
            int valStart = secIdx + 10;
            int valEnd = valStart;
            while (valEnd < item.length() && item.charAt(valEnd) >= '0' &&
                   item.charAt(valEnd) <= '9') {
              valEnd++;
            }
            noSyncSeconds = item.substring(valStart, valEnd).toInt();
          }
        } else if (item.indexOf("\"date\":\"" + realDate + "\"") != -1) {
          hasRealDate = true;
          int secIdx = item.indexOf("\"seconds\":");
          if (secIdx != -1) {
            int valStart = secIdx + 10;
            int valEnd = valStart;
            while (valEnd < item.length() && item.charAt(valEnd) >= '0' &&
                   item.charAt(valEnd) <= '9') {
              valEnd++;
            }
            realDateSeconds = item.substring(valStart, valEnd).toInt();
          }
        } else {
          if (daysJson.length() > 0)
            daysJson += ",";
          daysJson += item;
        }
      }
      itemStart = arrayContent.indexOf("{", itemEnd);
      if (itemStart == -1)
        break;
    }
  }

  Serial.printf("[DrivingTime] mergeNoSyncDate: NoSync seconds = %d, existing target date seconds = %d\n",
                noSyncSeconds, realDateSeconds);

  uint32_t mergedSeconds = realDateSeconds + noSyncSeconds;
  if (daysJson.length() > 0)
    daysJson += ",";
  daysJson += "{\"date\":\"" + realDate + "\",\"seconds\":" + String(mergedSeconds) + "}";

  String newJson = "{\"days\":[" + daysJson + "]}";
  Serial.printf("[DrivingTime] mergeNoSyncDate: Resulting JSON = '%s'\n", newJson.c_str());

  // Validate structural integrity in RAM
  if (newJson.indexOf("\"days\":[") == -1 || newJson.endsWith("]}") == false) {
    Serial.println("[DrivingTime] Merge aborted: malformed JSON generated in RAM!");
    return MERGE_FAILED;
  }

  // Write to temporary file
  File tmpF = LittleFS.open("/driving_time.tmp", "w");
  if (tmpF) {
    tmpF.print(newJson);
    tmpF.close();

    // Verify temp file size
    File verifyF = LittleFS.open("/driving_time.tmp", "r");
    if (verifyF) {
      size_t sz = verifyF.size();
      verifyF.close();
      if (sz > 0) {
        if (LittleFS.exists("/driving_time.json")) {
          LittleFS.remove("/driving_time.json");
        }
        if (LittleFS.rename("/driving_time.tmp", "/driving_time.json")) {
          Serial.printf("[DrivingTime] Merged NoSync (%d s) into %s (%d s -> %d s)\n",
                        noSyncSeconds, realDate.c_str(), realDateSeconds, mergedSeconds);
          return MERGE_SUCCESS_MERGED;
        }
      }
    }
  }

  Serial.println("[DrivingTime] Failed to write temporary file during merge! Keeping old file.");
  if (LittleFS.exists("/driving_time.tmp")) {
    LittleFS.remove("/driving_time.tmp");
  }
  return MERGE_FAILED;
}

bool pruneDrivingTimeHistory(String realDate) {
  realDate.trim();
  if (realDate == "" || realDate == "NoSync" || !LittleFS.exists("/driving_time.json")) {
    return false;
  }

  File f = LittleFS.open("/driving_time.json", "r");
  if (!f) return false;
  String json = f.readString();
  f.close();

  struct HistoryEntry {
    String date;
    uint32_t seconds;
  };
  std::vector<HistoryEntry> validEntries;

  int startIdx = json.indexOf("[");
  int endIdx = json.lastIndexOf("]");
  if (startIdx != -1 && endIdx != -1) {
    String arrayContent = json.substring(startIdx + 1, endIdx);
    int itemStart = 0;
    while (itemStart < arrayContent.length()) {
      int itemEnd = arrayContent.indexOf("}", itemStart);
      if (itemEnd == -1)
        itemEnd = arrayContent.length();
      String item = arrayContent.substring(itemStart, itemEnd + 1);
      item.trim();
      if (item.length() > 0) {
        String d = "";
        int dateIdx = item.indexOf("\"date\":\"");
        if (dateIdx != -1) {
          int dStart = dateIdx + 8;
          int dEnd = item.indexOf("\"", dStart);
          if (dEnd != -1) {
            d = item.substring(dStart, dEnd);
            d.trim();
          }
        }

        auto isValidDate = [](const String& str) -> bool {
          if (str.length() != 10) return false;
          if (str.charAt(4) != '-' || str.charAt(7) != '-') return false;
          for (int i = 0; i < 10; i++) {
            if (i == 4 || i == 7) continue;
            char c = str.charAt(i);
            if (c < '0' || c > '9') return false;
          }
          return true;
        };

        if (isValidDate(d)) {
          uint32_t secs = 0;
          int secIdx = item.indexOf("\"seconds\":");
          if (secIdx != -1) {
            int valStart = secIdx + 10;
            int valEnd = valStart;
            while (valEnd < item.length() && item.charAt(valEnd) >= '0' &&
                   item.charAt(valEnd) <= '9') {
              valEnd++;
            }
            secs = item.substring(valStart, valEnd).toInt();
          }

          bool dup = false;
          for (auto& entry : validEntries) {
            if (entry.date == d) {
              entry.seconds += secs;
              dup = true;
              break;
            }
          }
          if (!dup) {
            validEntries.push_back({d, secs});
          }
        }
      }
      itemStart = arrayContent.indexOf("{", itemEnd);
      if (itemStart == -1)
        break;
    }
  }

  bool hasRealDate = false;
  for (const auto& entry : validEntries) {
    if (entry.date == realDate) {
      hasRealDate = true;
      break;
    }
  }
  if (!hasRealDate) {
    validEntries.push_back({realDate, drivingSecondsToday});
  }

  // Filter out entries older than 7 days from realDate or with invalid dates
  int32_t refDays = dateToDays(realDate);
  std::vector<HistoryEntry> timeFilteredEntries;
  for (const auto& entry : validEntries) {
    int32_t entryDays = dateToDays(entry.date);
    if (entryDays < 0) continue;              // invalid date format
    if (refDays >= 0 && entryDays > refDays) continue;  // future date
    if (refDays >= 0 && (refDays - entryDays) >= 7) continue; // older than 7 days
    timeFilteredEntries.push_back(entry);
  }

  // Sort valid entries in descending order (newest date first)
  std::sort(timeFilteredEntries.begin(), timeFilteredEntries.end(), [](const HistoryEntry& a, const HistoryEntry& b) {
    return a.date > b.date;
  });

  std::vector<HistoryEntry> prunedEntries;

  // Always keep realDate
  for (const auto& entry : timeFilteredEntries) {
    if (entry.date == realDate) {
      prunedEntries.push_back(entry);
      break;
    }
  }

  // Add the rest until we have 7 entries
  for (const auto& entry : timeFilteredEntries) {
    if (prunedEntries.size() >= 7) break;
    if (entry.date == realDate) continue;
    prunedEntries.push_back(entry);
  }

  // Sort in ascending order (oldest date first) so they write chronologically
  std::sort(prunedEntries.begin(), prunedEntries.end(), [](const HistoryEntry& a, const HistoryEntry& b) {
    return a.date < b.date;
  });

  String daysJson = "";
  for (const auto& entry : prunedEntries) {
    if (daysJson.length() > 0)
      daysJson += ",";
    daysJson += "{\"date\":\"" + entry.date + "\",\"seconds\":" + String(entry.seconds) + "}";
  }

  String newJson = "{\"days\":[" + daysJson + "]}";

  // Validate structural integrity in RAM
  if (newJson.indexOf("\"days\":[") == -1 || newJson.endsWith("]}") == false) {
    Serial.println("[DrivingTime] Pruning aborted: malformed JSON generated in RAM!");
    return false;
  }

  // Write to temporary file
  File tmpF = LittleFS.open("/driving_time.tmp", "w");
  if (tmpF) {
    tmpF.print(newJson);
    tmpF.close();

    // Verify temp file size
    File verifyF = LittleFS.open("/driving_time.tmp", "r");
    if (verifyF) {
      size_t sz = verifyF.size();
      verifyF.close();
      if (sz > 0) {
        if (LittleFS.exists("/driving_time.json")) {
          LittleFS.remove("/driving_time.json");
        }
        if (LittleFS.rename("/driving_time.tmp", "/driving_time.json")) {
          Serial.printf("[DrivingTime] Pruned history. Kept %d entries.\n", prunedEntries.size());
          return true;
        }
      }
    }
  }

  Serial.println("[DrivingTime] Failed to write temporary file during prune! Keeping old file.");
  if (LittleFS.exists("/driving_time.tmp")) {
    LittleFS.remove("/driving_time.tmp");
  }
  return false;
}

void saveDrivingTime() {
  String daysJson = "";
  bool updated = false;

  Serial.printf("[DrivingTime] saveDrivingTime() started. currentDate='%s' today=%d week=%d\n",
                currentDate.c_str(), drivingSecondsToday, drivingSecondsThisWeek);

  if (LittleFS.exists("/driving_time.json")) {
    File f = LittleFS.open("/driving_time.json", "r");
    if (f) {
      String json = f.readString();
      f.close();

      int startIdx = json.indexOf("[");
      int endIdx = json.lastIndexOf("]");
      if (startIdx != -1 && endIdx != -1) {
        String arrayContent = json.substring(startIdx + 1, endIdx);
        int itemStart = 0;
        while (itemStart < arrayContent.length()) {
          int itemEnd = arrayContent.indexOf("}", itemStart);
          if (itemEnd == -1)
            itemEnd = arrayContent.length();
          String item = arrayContent.substring(itemStart, itemEnd + 1);
          item.trim();
          if (item.length() > 0) {
            if (item.indexOf("\"date\":\"" + currentDate + "\"") != -1) {
              if (daysJson.length() > 0)
                daysJson += ",";
              daysJson += "{\"date\":\"" + currentDate +
                          "\",\"seconds\":" + String(drivingSecondsToday) + "}";
              updated = true;
            } else {
              if (daysJson.length() > 0)
                daysJson += ",";
              daysJson += item;
            }
          }
          itemStart = arrayContent.indexOf("{", itemEnd);
          if (itemStart == -1)
            break;
        }
      }
    }
  }

  if (!updated) {
    if (daysJson.length() > 0)
      daysJson += ",";
    daysJson += "{\"date\":\"" + currentDate +
                "\",\"seconds\":" + String(drivingSecondsToday) + "}";
  }

  String newJson = "{\"days\":[" + daysJson + "]}";

  // Validate structural integrity in RAM
  if (newJson.indexOf("\"days\":[") == -1 || newJson.endsWith("]}") == false) {
    Serial.println("[DrivingTime] Save aborted: malformed JSON generated in RAM!");
    return;
  }

  Serial.printf("[DrivingTime] saveDrivingTime: Writing /driving_time.json. Content: '%s'\n", newJson.c_str());

  // Write to temporary file
  File tmpF = LittleFS.open("/driving_time.tmp", "w");
  if (tmpF) {
    tmpF.print(newJson);
    tmpF.close();

    // Verify temp file size
    File verifyF = LittleFS.open("/driving_time.tmp", "r");
    if (verifyF) {
      size_t sz = verifyF.size();
      verifyF.close();
      if (sz > 0) {
        if (LittleFS.exists("/driving_time.json")) {
          LittleFS.remove("/driving_time.json");
        }
        if (LittleFS.rename("/driving_time.tmp", "/driving_time.json")) {
          Serial.printf("[DrivingTime] Saved JSON: %s\n", newJson.c_str());
          return;
        }
      }
    }
  }

  Serial.println("[DrivingTime] Failed to write temporary file during save! Keeping old file.");
  if (LittleFS.exists("/driving_time.tmp")) {
    LittleFS.remove("/driving_time.tmp");
  }
}

void loadDrivingTime() {
  Serial.printf("[DrivingTime] loadDrivingTime() started. currentDate='%s'\n", currentDate.c_str());
  if (!LittleFS.exists("/driving_time.json")) {
    Serial.println("[DrivingTime] No history file found, using defaults.");
    drivingSecondsToday = 0;
    drivingSecondsThisWeek = 0;
    return;
  }
  File f = LittleFS.open("/driving_time.json", "r");
  if (!f) {
    Serial.println("[DrivingTime] Failed to open history file!");
    return;
  }
  String json = f.readString();
  f.close();

  Serial.printf("[DrivingTime] Loaded JSON: %s\n", json.c_str());

  drivingSecondsToday = 0;
  drivingSecondsThisWeek = 0;

  int32_t refDays = dateToDays(currentDate); // -1 if NoSync

  // Parse each entry and validate dates
  int startIdx = json.indexOf("[");
  int endIdx = json.lastIndexOf("]");
  if (startIdx == -1 || endIdx == -1) return;

  String arrayContent = json.substring(startIdx + 1, endIdx);
  int itemStart = 0;
  while (itemStart < arrayContent.length()) {
    int itemEnd = arrayContent.indexOf("}", itemStart);
    if (itemEnd == -1) break;
    String item = arrayContent.substring(itemStart, itemEnd + 1);
    item.trim();
    if (item.length() > 0) {
      // Extract date
      String d = "";
      int dateIdx = item.indexOf("\"date\":\"");
      if (dateIdx != -1) {
        int dStart = dateIdx + 8;
        int dEnd = item.indexOf("\"", dStart);
        if (dEnd != -1) {
          d = item.substring(dStart, dEnd);
          d.trim();
        }
      }
      // Extract seconds
      uint32_t secs = 0;
      int secIdx = item.indexOf("\"seconds\":");
      if (secIdx != -1) {
        int valStart = secIdx + 10;
        int valEnd = valStart;
        while (valEnd < item.length() && item.charAt(valEnd) >= '0' &&
               item.charAt(valEnd) <= '9') {
          valEnd++;
        }
        secs = item.substring(valStart, valEnd).toInt();
      }

      int32_t entryDays = dateToDays(d);
      if (entryDays < 0) {
        // Skip invalid date entries (NoSync, empty, malformed)
        Serial.printf("[DrivingTime] loadDrivingTime: Key '%s' is REJECTED (invalid date)\n", d.c_str());
      } else {
        // Load today's seconds
        if (d == currentDate) {
          drivingSecondsToday = secs;
          Serial.printf("[DrivingTime] loadDrivingTime: Key '%s' is ACCEPTED for today (%d s)\n", d.c_str(), secs);
        } else {
          Serial.printf("[DrivingTime] loadDrivingTime: Key '%s' is loaded with %d s\n", d.c_str(), secs);
        }
        // Accumulate weekly sum only for entries within last 7 days
        if (refDays >= 0) {
          int32_t age = refDays - entryDays;
          if (age >= 0 && age < 7) {
            drivingSecondsThisWeek += secs;
            Serial.printf("[DrivingTime] loadDrivingTime: Key '%s' ACCEPTED for Last 7 Days (age=%d days)\n", d.c_str(), age);
          } else {
            Serial.printf("[DrivingTime] loadDrivingTime: Key '%s' REJECTED for Last 7 Days (age=%d days, outside 7-day range)\n", d.c_str(), age);
          }
        } else {
          Serial.printf("[DrivingTime] loadDrivingTime: currentDate is NoSync, skipping weekly accumulation for key '%s'\n", d.c_str());
        }
      }
    }
    itemStart = arrayContent.indexOf("{", itemEnd);
    if (itemStart == -1) break;
  }

  Serial.printf("[DrivingTime] Loaded today=%d, last7days=%d\n",
                drivingSecondsToday, drivingSecondsThisWeek);
}
#endif

// ============================================================
// Logic Engines for Charging Mode, AutoBack, and Driving Logger
// ============================================================
static uint32_t chargingCandidateMs = 0;
static uint32_t chargingExitCandidateMs = 0;
static uint32_t vehicleSpeedExitMs = 0;

void updateChargingModeLogic(float speedKmh, uint32_t dtMs) {
  // Calculate instant charge power for entry detection (based on negative current)
  float instantChargePowerKw = 0.0f;
  if (hvBatteryVoltage > 100.0f && hvBatteryCurrent < -0.5f) {
    instantChargePowerKw = -(hvBatteryVoltage * hvBatteryCurrent) / 1000.0f;
  }

  // 2.5. Calculate fresh energy signal
  bool accumulatedChargeFresh =
      accumulatedChargeValidated &&
      lastChargeEnergyIncreaseMs > 0 &&
      (millis() - lastChargeEnergyIncreaseMs <= 15000);

  // Use instant power instead of rolling average for faster battery-side charging detection
  bool batterySideCharging = instantChargePowerKw >= 0.3f;

  // Active charging signal check (Battery charging or energy increasing)
  bool isActivelyCharging = batterySideCharging || accumulatedChargeFresh;
  static uint32_t activeChargeSignalTimerMs = 0;
  if (isActivelyCharging) {
    activeChargeSignalTimerMs = 0;
  } else {
    activeChargeSignalTimerMs += dtMs;
  }

  // Entry condition
  bool enterCondition = isHvPowerReady() && (speedKmh <= 0.5f) &&
                        (batterySideCharging || accumulatedChargeFresh);

  // Update temporary charging debug globals for screen display
  dbgInstantChargePowerKw = instantChargePowerKw;
  dbgBatterySideCharging = batterySideCharging;
  dbgChargingEnterCondition = enterCondition;
  dbgHvPowerReady = isHvPowerReady();
  dbgChargingCandidateMs = 0;
  dbgHvVoltageAgeMs = (hvVoltageLastUpdateMs > 0) ? (millis() - hvVoltageLastUpdateMs) : 999999;
  dbgHvCurrentAgeMs = (hvCurrentLastUpdateMs > 0) ? (millis() - hvCurrentLastUpdateMs) : 999999;

  inChargingMode = false; // Always false, Charging Mode UI removed
}

// ── Charge Estimate Candidate Dropout Recovery (v1.6) ──
static constexpr uint32_t CHARGE_EST_TELEMETRY_HOLD_MS = 7000; // 7s continuous candidate hold per dropout incident
static uint32_t chargeCandidateFreshAccumulatedMs = 0;          // Accumulated genuine fresh charging duration (ms)
static uint32_t chargeCandidateLastFreshMs        = 0;          // Timestamp of most recent fresh charging sample
static uint32_t chargeCandidateDropoutStartMs     = 0;          // Timestamp when current continuous dropout began (0 = fresh)

void updateAutoBack(float speedKmh, uint32_t dtMs) {
  // Single-Capture Telemetry Invariant (FINAL LOCK v1.3 Section 2.2 / 8)
  const uint32_t now = millis();
  const bool obdFreshNow = isObdDataFresh();
  const bool hvReadyNow = isHvPowerReady();

  // Export to single-capture diagnostic mirrors (consumed by Debug Page 9 & API)
  chargeHvReady = hvReadyNow;
  chargeObdFresh = obdFreshNow;

  chargeDirectionValid = (hvBatteryVoltage > 100.0f) && (hvBatteryCurrent < -0.5f);
  chargePowerValid = obdFreshNow && hvReadyNow && chargeDirectionValid &&
                     std::isfinite(hvBatteryVoltage) && std::isfinite(hvBatteryCurrent);
  if (chargePowerValid) {
    chargeInstantPowerKw = -(hvBatteryVoltage * hvBatteryCurrent) / 1000.0f;
  } else {
    chargeInstantPowerKw = 0.0f;
  }
  chargeDetectedRaw = chargePowerValid && std::isfinite(chargeInstantPowerKw) &&
                      (chargeInstantPowerKw >= CHARGE_EST_MIN_POWER_KW);

  // 1. Charge Estimate 4-State Machine
  switch (chargeEstimateState) {
    case CHG_EST_NOT_CHARGING:
      if (chargeDetectedRaw) {
        chargeEstimateState = CHG_EST_CANDIDATE;
        chargeStateTimerMs = now;
        chargeCandidateFreshAccumulatedMs = 0;
        chargeCandidateLastFreshMs = now;
        chargeCandidateDropoutStartMs = 0;
        chargeAutoBackConfigArmed = autoBackEnabled && (autoBackPageType == 3);
      }
      break;

    case CHG_EST_CANDIDATE:
      if (chargeDetectedRaw) {
        // Case A: Fresh telemetry + confirmed charging (DIR = CHARGE, Power >= 0.3 kW)
        if (chargeCandidateDropoutStartMs != 0) {
          // LOCK 2 & 6: Recovered from temporary dropout within 7s continuous window
          // Reset continuous dropout timer to 0 immediately (do NOT accumulate dropout time)
          chargeCandidateDropoutStartMs = 0;
          chargeCandidateLastFreshMs = now;
        } else {
          // Continuous fresh charging: accumulate elapsed fresh time
          uint32_t elapsedFresh = now - chargeCandidateLastFreshMs;
          if (elapsedFresh > 1000) {
            elapsedFresh = (dtMs > 0 && dtMs < 1000) ? dtMs : 100;
          }
          chargeCandidateFreshAccumulatedMs += elapsedFresh;
          chargeCandidateLastFreshMs = now;
        }

        // LOCK 1: Driven EXCLUSIVELY by chargeCandidateFreshAccumulatedMs (chargeStateTimerMs is NOT used)
        if (chargeCandidateFreshAccumulatedMs >= CHARGE_EST_ENTRY_DEBOUNCE_MS) {
          chargeEstimateState = CHG_EST_ACTIVE;
          chargeStateTimerMs = 0;
          chargeCandidateFreshAccumulatedMs = 0;
          chargeCandidateLastFreshMs = 0;
          chargeCandidateDropoutStartMs = 0;
          chargeHistoryCount = 0;
          chargeHistoryIdx = 0;
          avgChargePowerKw = 0.0f;
          lastChargeSampleMs = now;

          // True Configuration Snapshot execution & screen ownership
          if (chargeAutoBackConfigArmed && !chargeAutoBackSessionUsed) {
            if (!chargeAutoBackActive) {
              previousPageBeforeAutoBack = currentPage;
              chargeAutoBackActive = true;
              chargeEstimateAutoBackOwnedScreen = true;
              tft.fillScreen(COLOR_BLACK);
              themeChanged = true;
              gifHomeNeedClear = true;
            } else {
              chargeEstimateAutoBackOwnedScreen = false;
            }
            chargeAutoBackSessionUsed = true;
          }
        }
      } else {
        // chargeDetectedRaw == false
        if (obdFreshNow && hvReadyNow) {
          // Case B: Telemetry IS fresh, but confirms NOT charging -> abort candidate immediately
          chargeEstimateState = CHG_EST_NOT_CHARGING;
          chargeStateTimerMs = 0;
          chargeCandidateFreshAccumulatedMs = 0;
          chargeCandidateLastFreshMs = 0;
          chargeCandidateDropoutStartMs = 0;
          chargeAutoBackConfigArmed = false;
        } else {
          // Case C: Telemetry is STALE (temporary dropout) -> maintain Candidate Hold
          if (chargeCandidateDropoutStartMs == 0) {
            chargeCandidateDropoutStartMs = now; // Start tracking current continuous dropout
          } else if ((now - chargeCandidateDropoutStartMs) > CHARGE_EST_TELEMETRY_HOLD_MS) {
            // Continuous dropout exceeded 7,000 ms -> terminate candidate session
            chargeEstimateState = CHG_EST_NOT_CHARGING;
            chargeStateTimerMs = 0;
            chargeCandidateFreshAccumulatedMs = 0;
            chargeCandidateLastFreshMs = 0;
            chargeCandidateDropoutStartMs = 0;
            chargeAutoBackConfigArmed = false;
          }
          // Within 7,000 ms: session remains ALIVE in CHG_EST_CANDIDATE, timer paused, AutoBack armed preserved
        }
      }
      break;

    case CHG_EST_ACTIVE:
      if (chargeDetectedRaw) {
        // Anti-Burst 1 sample/sec
        if ((now - lastChargeSampleMs) >= 1000) {
          if (CHARGE_EST_HISTORY_CAPACITY > 0) {
            chargePowerHistory[chargeHistoryIdx] = chargeInstantPowerKw;
            chargeHistoryIdx = (chargeHistoryIdx + 1) % CHARGE_EST_HISTORY_CAPACITY;
            if (chargeHistoryCount < (int)CHARGE_EST_HISTORY_CAPACITY) {
              chargeHistoryCount++;
            }
          }
          lastChargeSampleMs = now;
        }

        // Rolling average with finite and threshold checks
        if (chargeHistoryCount >= CHARGE_EST_MIN_POWER_SAMPLES) {
          float sum = 0.0f;
          int validSamples = 0;
          for (size_t i = 0; i < (size_t)chargeHistoryCount; ++i) {
            const float sample = chargePowerHistory[i];
            if (std::isfinite(sample) && sample >= CHARGE_EST_MIN_POWER_KW) {
              sum += sample;
              validSamples++;
            }
          }
          if (validSamples >= CHARGE_EST_MIN_POWER_SAMPLES && std::isfinite(sum)) {
            const float candidateAverage = sum / (float)validSamples;
            if (std::isfinite(candidateAverage) && candidateAverage >= CHARGE_EST_MIN_POWER_KW) {
              avgChargePowerKw = candidateAverage;
            } else {
              avgChargePowerKw = 0.0f;
            }
          } else {
            avgChargePowerKw = 0.0f;
          }
        } else {
          avgChargePowerKw = 0.0f;
        }
      } else {
        // Charging signal lost -> transition to END_CANDIDATE
        chargeEstimateState = CHG_EST_END_CANDIDATE;
        chargeStateTimerMs = now;
      }
      break;

    case CHG_EST_END_CANDIDATE:
      if (chargeDetectedRaw) {
        // Resumed within 10s -> back to ACTIVE
        chargeEstimateState = CHG_EST_ACTIVE;
        chargeStateTimerMs = 0;
        lastChargeSampleMs = now;
        // History, average, sessionUsed, and screen ownership preserved
      } else {
        if ((now - chargeStateTimerMs) >= CHARGE_EST_EXIT_DEBOUNCE_MS) {
          // True session termination
          chargeEstimateState = CHG_EST_NOT_CHARGING;
          chargeStateTimerMs = 0;
          lastChargeSampleMs = 0;

          chargeCandidateFreshAccumulatedMs = 0;
          chargeCandidateLastFreshMs = 0;
          chargeCandidateDropoutStartMs = 0;

          chargeHistoryCount = 0;
          chargeHistoryIdx = 0;
          avgChargePowerKw = 0.0f;
          chargeAutoBackConfigArmed = false;
          chargeAutoBackSessionUsed = false;

          if (chargeEstimateAutoBackOwnedScreen) {
            currentPage = previousPageBeforeAutoBack;
            tft.fillScreen(COLOR_BLACK);
            themeChanged = true;
            gifHomeNeedClear = true;
          }
          chargeAutoBackActive = false;
          chargeEstimateAutoBackOwnedScreen = false;
        }
      }
      break;
  }

  // v1.4: Touch Exit != Session End (Stale Screen Ownership Guard)
  // Placed after State Machine so the state machine owns lifecycle transitions first.
  // If chargeAutoBackActive was dismissed externally (e.g. user touch input in touch_handler.h),
  // immediately relinquish screen ownership so session exit will not overwrite user's manual page choice.
  // Note: The charging session itself remains ACTIVE, power sampling continues, and ETA continues.
  if (!chargeAutoBackActive && chargeEstimateAutoBackOwnedScreen) {
    chargeEstimateAutoBackOwnedScreen = false;
  }

  // 2. Authoritative Prioritized ETA Calculation
  // Priority 1 — Invalid SOC
  if (!socFinalValid || !std::isfinite(batt_soc) || batt_soc < 0.0f) {
    chargeSocDelta = 0.0f;
    chargeEnergyNeededKwh = 0.0f;
    chargeEtaMinutes = -1.0f;
    chargeEtaStatusStr = "WAIT_SOC";
  }
  // Priority 2 — Not Charging / End Candidate
  else if (chargeEstimateState == CHG_EST_NOT_CHARGING || chargeEstimateState == CHG_EST_END_CANDIDATE) {
    chargeSocDelta = max(0.0f, (float)chargeTargetSoc - batt_soc);
    chargeEnergyNeededKwh = 0.0f;
    chargeEtaMinutes = -1.0f;
    chargeEtaStatusStr = "NO_CHG";
  }
  // Priority 3 — Target Reached (handles == and >)
  else if (batt_soc >= (float)chargeTargetSoc) {
    chargeSocDelta = 0.0f;
    chargeEnergyNeededKwh = 0.0f;
    chargeEtaMinutes = 0.0f;
    chargeEtaStatusStr = "DONE";
  }
  // Priority 4 — Insufficient Samples / Invalid Power
  else if (chargeHistoryCount < CHARGE_EST_MIN_POWER_SAMPLES || !std::isfinite(avgChargePowerKw) || avgChargePowerKw < CHARGE_EST_MIN_POWER_KW) {
    chargeSocDelta = (float)chargeTargetSoc - batt_soc;
    chargeEnergyNeededKwh = 44.9f * (chargeSocDelta / 100.0f);
    chargeEtaMinutes = -1.0f;
    chargeEtaStatusStr = "CALC";
  }
  // Priority 5 — Valid ETA
  else {
    chargeSocDelta = (float)chargeTargetSoc - batt_soc;
    chargeEnergyNeededKwh = 44.9f * (chargeSocDelta / 100.0f);
    chargeEtaMinutes = (chargeEnergyNeededKwh / avgChargePowerKw) * 60.0f;
    if (!std::isfinite(chargeEtaMinutes) || chargeEtaMinutes < 0.0f) {
      chargeEtaMinutes = -1.0f;
      chargeEtaStatusStr = "CALC";
    } else {
      chargeEtaStatusStr = "VALID";
    }
  }

  // 3. AutoBack Management & Screen Synchronization
  if (!autoBackEnabled) {
    if (autoBackActive) {
      autoBackActive = false;
      currentPage = previousPageBeforeAutoBack;
      tft.fillScreen(COLOR_BLACK);
      themeChanged = true;
      gifHomeNeedClear = true;
    }
    if (chargeAutoBackActive && chargeEstimateAutoBackOwnedScreen) {
      chargeAutoBackActive = false;
      currentPage = previousPageBeforeAutoBack;
      tft.fillScreen(COLOR_BLACK);
      themeChanged = true;
      gifHomeNeedClear = true;
      chargeEstimateAutoBackOwnedScreen = false;
    }
    stoppedDurationMs = 0;
    movingDurationMs = 0;
    autoBackStopwatchStartMs = 0;
    chargeAutoBackConfigArmed = false;
    return;
  }

  // Session Identity Lock (v1.4 Invariant):
  // If this charging session was armed or is active as Charge Estimate, session identity is FROZEN.
  // External changes to live autoBackPageType mid-session CANNOT dismiss, hijack, or alter the screen.
  if (chargeAutoBackConfigArmed || chargeAutoBackActive) {
    // Suppress normal speed-based AutoBack while charging session is armed/active
    if (autoBackActive) {
      autoBackActive = false;
      stoppedDurationMs = 0;
      movingDurationMs = 0;
    }
    return;
  }

  // If live config is currently Type 3 (Charge Estimate) but not in an active charging session,
  // normal speed-based AutoBack (Types 0, 1, 2) must not run
  if (autoBackPageType == 3) {
    if (autoBackActive) {
      autoBackActive = false;
      stoppedDurationMs = 0;
      movingDurationMs = 0;
    }
    return;
  }

  // 4. Original Normal AutoBack logic (Types 0, 1, 2)
  if (!obdFreshNow) {
    // Freeze AutoBack state on stale OBD data
    return;
  }

  if (chargeAutoBackActive || currentState != STATE_ACTIVE) {
    autoBackActive = false;
    stoppedDurationMs = 0;
    movingDurationMs = 0;
    return;
  }

  bool stopped = (speedKmh <= 0.5f);
  bool moving = (speedKmh > 0.5f);

  if (!autoBackActive) {
    if (stopped) {
      stoppedDurationMs += dtMs;
    } else {
      stoppedDurationMs = 0;
    }

    if (stoppedDurationMs >= 3000) {
      previousPageBeforeAutoBack = currentPage;
      autoBackActive = true;
      autoBackStopwatchStartMs = millis();
      tft.fillScreen(COLOR_BLACK);
      themeChanged = true;
      gifHomeNeedClear = true;
      stoppedDurationMs = 0;
    }
  } else {
    if (moving) {
      movingDurationMs += dtMs;
    } else {
      movingDurationMs = 0;
    }

    if (movingDurationMs >= 3000) {
      currentPage = previousPageBeforeAutoBack;
      autoBackActive = false;
      tft.fillScreen(COLOR_BLACK);
      themeChanged = true;
      gifHomeNeedClear = true;
      movingDurationMs = 0;
    }
  }
}

static bool lastDrivingState = false;

#if 0
void updateDrivingTime(float speedKmh, uint32_t dtMs) {
  bool isFresh = isObdDataFresh();
  bool driving = isFresh && (speedKmh > 0.5f);

  if (driving) {
    drivingTimeAccumulatedMs += dtMs;
    while (drivingTimeAccumulatedMs >= 1000) {
      drivingSecondsToday++;
      drivingSecondsThisWeek++;
      drivingTimeAccumulatedMs -= 1000;
    }
    lastDrivingState = true;
  } else {
    // Save immediately on state change (from driving to stopped)
    if (lastDrivingState) {
      lastDrivingState = false;
      Serial.println("[DrivingTime] Stopped driving. Saving immediately.");
      saveDrivingTime();
    }
  }

  // Periodic persistence check (every 60 seconds)
  if (driving && (millis() - lastDrivingPersistMs >= 60000)) {
    lastDrivingPersistMs = millis();
    Serial.println("[DrivingTime] Periodic save trigger.");
    saveDrivingTime();
  }
}
#endif

// ============================================================
// ARDUINO SETUP
// ============================================================
void setup(void) {

  Serial.begin(115200);
  delay(100);

  // Load saved configurations from NVS
  preferences.begin("obd2_dash", false);
  showGifHome = preferences.getBool("gif_home", false);

  // Load slot layout configuration
  config_slots[0] = preferences.getInt("slot0", 0);
  config_slots[1] = preferences.getInt("slot1", 1);
  config_slots[2] = preferences.getInt("slot2", 2);
  config_slots[3] = preferences.getInt("slot3", 3);
  config_slots[4] = preferences.getInt("slot4", 4);
  config_slots[5] = preferences.getInt("slot5", 5);

  // Load car profile selection
  car_profile = preferences.getInt("car_profile", 0);
  dolphin_test_mode = preferences.getInt("dolphin_test", DEFAULT_DOLPHIN_TEST);
  rpmSourceMode = 1; // Force EST mode internally (ignore NVS value)
  electricityRateBahtPerKwh = preferences.getFloat("elec_rate", 4.50f);
  energySourceMode = preferences.getInt("energy_mode", 0);

  // 1. Dual-Slot NVS Boot Evaluation with RFC 1982 Serial-Number Arithmetic
  bool slot0Valid = false;
  String slot0Ssid = "", slot0Ip = "", slot0Bssid = "";
  uint8_t slot0Chan = 1;
  uint32_t slot0Gen = 0;

  bool slot1Valid = false;
  String slot1Ssid = "", slot1Ip = "", slot1Bssid = "";
  uint8_t slot1Chan = 1;
  uint32_t slot1Gen = 0;

  // Validate Slot 0
  if (preferences.getBool("obd_valid_0", false)) {
    slot0Ssid = preferences.getString("obd_ssid_0", "");
    slot0Ip = preferences.getString("obd_ip_0", "192.168.0.10");
    slot0Bssid = preferences.getString("obd_bssid_0", "");
    slot0Chan = preferences.getUChar("obd_ch_0", 1);
    slot0Gen = preferences.getUInt("obd_gen_0", 0);
    uint8_t slot0Chk = preferences.getUChar("obd_chk_0", 0);

    uint8_t b0[6] = {0};
    if (slot0Ssid.length() > 0 && isValidObdIp(slot0Ip) && parseMacAddress(slot0Bssid, b0) &&
        slot0Chan >= 1 && slot0Chan <= 14 &&
        calcObdConfigCrc8(slot0Ssid, slot0Ip, b0, slot0Chan, slot0Gen) == slot0Chk) {
      slot0Valid = true;
    }
  }

  // Validate Slot 1
  if (preferences.getBool("obd_valid_1", false)) {
    slot1Ssid = preferences.getString("obd_ssid_1", "");
    slot1Ip = preferences.getString("obd_ip_1", "192.168.0.10");
    slot1Bssid = preferences.getString("obd_bssid_1", "");
    slot1Chan = preferences.getUChar("obd_ch_1", 1);
    slot1Gen = preferences.getUInt("obd_gen_1", 0);
    uint8_t slot1Chk = preferences.getUChar("obd_chk_1", 0);

    uint8_t b1[6] = {0};
    if (slot1Ssid.length() > 0 && isValidObdIp(slot1Ip) && parseMacAddress(slot1Bssid, b1) &&
        slot1Chan >= 1 && slot1Chan <= 14 &&
        calcObdConfigCrc8(slot1Ssid, slot1Ip, b1, slot1Chan, slot1Gen) == slot1Chk) {
      slot1Valid = true;
    }
  }

  // Deterministic Slot Selection Algorithm
  if (slot0Valid && slot1Valid) {
    if (slot0Gen == slot1Gen) {
      // Tie-Break: if identical config -> Slot 0, else ambiguous -> invalidate
      if (slot0Ssid == slot1Ssid && slot0Ip == slot1Ip && slot0Bssid == slot1Bssid && slot0Chan == slot1Chan) {
        currentActiveSlot = 0;
        savedObdSsid = slot0Ssid;
        savedObdIp = slot0Ip;
        savedObdBssid = slot0Bssid;
        savedObdChannel = slot0Chan;
        savedObdGen = slot0Gen;
        obdConfigValid = true;
      } else {
        Serial.println("[NVS] Ambiguous equal generations with differing configs — entering scan mode.");
        obdConfigValid = false;
      }
    } else if (isGenerationNewer(slot1Gen, slot0Gen)) {
      currentActiveSlot = 1;
      savedObdSsid = slot1Ssid;
      savedObdIp = slot1Ip;
      savedObdBssid = slot1Bssid;
      savedObdChannel = slot1Chan;
      savedObdGen = slot1Gen;
      obdConfigValid = true;
    } else {
      currentActiveSlot = 0;
      savedObdSsid = slot0Ssid;
      savedObdIp = slot0Ip;
      savedObdBssid = slot0Bssid;
      savedObdChannel = slot0Chan;
      savedObdGen = slot0Gen;
      obdConfigValid = true;
    }
  } else if (slot0Valid) {
    currentActiveSlot = 0;
    savedObdSsid = slot0Ssid;
    savedObdIp = slot0Ip;
    savedObdBssid = slot0Bssid;
    savedObdChannel = slot0Chan;
    savedObdGen = slot0Gen;
    obdConfigValid = true;
  } else if (slot1Valid) {
    currentActiveSlot = 1;
    savedObdSsid = slot1Ssid;
    savedObdIp = slot1Ip;
    savedObdBssid = slot1Bssid;
    savedObdChannel = slot1Chan;
    savedObdGen = slot1Gen;
    obdConfigValid = true;
  } else {
    obdConfigValid = false;
    currentActiveSlot = 0;
    savedObdSsid = "";
    savedObdIp = "192.168.0.10";
    savedObdBssid = "";
    savedObdChannel = 1;
    savedObdGen = 0;
    Serial.println("[NVS] No valid OBD configuration in either slot — clean scan fallback.");
  }

  if (obdConfigValid) {
    obd_ssid_active = savedObdSsid;
    obd_ip_active = savedObdIp;
    currentApChannel = savedObdChannel;
    targetApChannel = savedObdChannel;
    Serial.printf("[NVS] Selected Trusted Slot %u (Gen %u): SSID=%s, BSSID=%s, Ch=%u, IP=%s\n",
                  currentActiveSlot, savedObdGen, savedObdSsid.c_str(), savedObdBssid.c_str(), savedObdChannel, savedObdIp.c_str());
  } else {
    obd_ssid_active = "";
    obd_ip_active = "192.168.0.10";
    currentApChannel = 1;
    targetApChannel = 1;
  }

  // Record initial quantitative heap metrics
  bootFreeHeap = ESP.getFreeHeap();
  minFreeHeap = ESP.getMinFreeHeap();
  minLargestFreeBlock = ESP.getMaxAllocHeap();

  // Load energy comparison notes
  carDisplayEnergyVal = preferences.getFloat("car_eng_val", 0.0f);
  carDisplayEnergyUnit = preferences.getString("car_eng_unit", "kWh");
  carDisplayEnergyLabel = preferences.getString("car_eng_lbl", "");

  // Load and validate vehicle profiles (fallback to BYD_DOLPHIN_TH_EV if
  // invalid/non-selectable)
  selected_vehicle_profile =
      preferences.getString("selected_profile", "BYD_DOLPHIN_TH_EV");
  const VehicleProfile *sp = findVehicleProfile(selected_vehicle_profile);
  if (sp == nullptr || !sp->selectableInProduction) {
    selected_vehicle_profile = "BYD_DOLPHIN_TH_EV";
    preferences.putString("selected_profile", selected_vehicle_profile);
    car_profile = 0;
  } else {
    if (selected_vehicle_profile == "BYD_DOLPHIN_TH_EV") {
      car_profile = 0;
    }
  }

  String default_profile =
      preferences.getString("default_profile", "BYD_DOLPHIN_TH_EV");
  const VehicleProfile *dp = findVehicleProfile(default_profile);
  if (dp == nullptr || !dp->selectableInProduction) {
    default_profile = "BYD_DOLPHIN_TH_EV";
    preferences.putString("default_profile", default_profile);
  }

  // Load new feature configurations
  autoBackEnabled = preferences.getBool("ab_enabled", true);
  autoBackPageType = preferences.getInt("ab_type", 0);
  chargeTargetSoc = preferences.getInt("chg_target", 80);
  if (chargeTargetSoc < 0 || chargeTargetSoc > 100) {
    chargeTargetSoc = 80;
  }
  chargeTargetSoc = ((chargeTargetSoc + 5) / 10) * 10;
  chargeTargetSoc = constrain(chargeTargetSoc, 0, 100);
  chargingHomeType = preferences.getInt("chg_home", 0);
  lastSavedChargeCount = preferences.getInt("chg_count", -1);

  preferences.end();

  // Load history data from LittleFS
  // loadDrivingTime();
  Serial.printf("Loaded saved settings: showGifHome=%d, "
                "car_profile=%d, dolphin_test_mode=%d, rpmSourceMode=%d\n",
                showGifHome, car_profile, dolphin_test_mode,
                rpmSourceMode);

  Serial.println("=================================");
  Serial.println("Project OBD2: S3 Dashboard Active!");
  Serial.println("=================================");

  pinMode(TOUCH_PIN, INPUT);
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  digitalWrite(TFT_CS, HIGH);

  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH);
  delay(5);
  digitalWrite(TFT_RST, LOW);
  delay(20);
  digitalWrite(TFT_RST, HIGH);
  delay(150);

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);
  tft.init(172, 320);
  tft.setSPISpeed(80000000);
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);

  tft.setRotation(3);
  tft.fillScreen(COLOR_BLACK);

  // Initialize LittleFS
  if (!LittleFS.begin(false)) {
    Serial.println("[ERROR] LittleFS mount failed!");
  } else {
    Serial.println("[FS] LittleFS mounted successfully. Listing files:");
    performGifStartupRecovery();   // Recover from interrupted uploads before ensureDefaults
    ensureDefaultActiveGifs();
    File root = LittleFS.open("/", "r");
    if (root) {
      File file = root.openNextFile();
      while (file) {
        Serial.printf("  - %s (%d bytes)\n", file.name(), file.size());
        file = root.openNextFile();
      }
    }
  }

  playBootAnimation();

  // Set up Dual AP+STA WiFi
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();
  enforcePowerSavePolicy();

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    uint32_t now = millis();
    portENTER_CRITICAL_ISR(&wifiEventMux);
    globalEventSequence++;
    uint32_t seq = globalEventSequence;

    if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED ||
        event == ARDUINO_EVENT_WIFI_STA_GOT_IP ||
        event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      latestStaEvent.sequence = seq;
      latestStaEvent.timestampMs = now;
      latestStaEvent.event = event;
      if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        latestStaEvent.disconnectReason = info.wifi_sta_disconnected.reason;
      }
      latestStaEvent.pending = true;
    }

    if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED ||
        event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
      latestApEvent.sequence = seq;
      latestApEvent.timestampMs = now;
      latestApEvent.event = event;
      latestApEvent.pending = true;
    }
    portEXIT_CRITICAL_ISR(&wifiEventMux);
  });

  // Configure the local setup AP explicitly with saved or default channel
  bool apConfigOk = WiFi.softAPConfig(SETUP_AP_IP, SETUP_AP_GATEWAY, SETUP_AP_SUBNET);
  bool apStartOk = WiFi.softAP("OBD2-Dashboard", "12345678",
                               currentApChannel, false,
                               SETUP_AP_MAX_CONNECTIONS);
  currentApState = apStartOk ? AP_STATE_RUNNING : AP_STATE_STOPPED;
  enforcePowerSavePolicy();

  Serial.printf("[WIFI][AP] start: %s SSID=OBD2-Dashboard IP=%s channel=%u maxClients=%u\n",
                apStartOk ? "PASS" : "FAIL",
                WiFi.softAPIP().toString().c_str(),
                currentApChannel,
                SETUP_AP_MAX_CONNECTIONS);

  dnsServer.start(53, "*", SETUP_AP_IP);
  Serial.println("[DNS] DNS Server started for Captive Portal redirect.");

  // Configure Web Server routing
  const char * headerkeys[] = {"Content-Length"};
  server.collectHeaders(headerkeys, 1);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/setup", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });

  // iOS/macOS captive-network probes. Always serve the setup portal rather
  // than returning a normal Internet-success response, so Apple's captive
  // assistant can reliably discover the local portal.
  auto sendCaptivePortal = []() {
    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  };
  server.on("/hotspot-detect.html", HTTP_GET, sendCaptivePortal);
  server.on("/generate_204", HTTP_GET, sendCaptivePortal);
  server.on("/connecttest.txt", HTTP_GET, sendCaptivePortal);
  server.on("/ncsi.txt", HTTP_GET, sendCaptivePortal);
  server.on("/landing", HTTP_GET, []() {
    server.send_P(200, "text/html; charset=utf-8", LANDING_HTML);
  });
  server.on(
      "/upload", HTTP_POST,
      []() {
        if (uploadError) {
          int statusCode = 500;
          if (uploadErrorMsg.indexOf("CONFLICT") != -1) {
            statusCode = 409;
          } else if (uploadErrorMsg.indexOf("PAYLOAD_TOO_LARGE") != -1) {
            statusCode = 413;
          } else if (uploadErrorMsg.indexOf("invalid") != -1 || uploadErrorMsg.indexOf("Invalid") != -1 || 
              uploadErrorMsg.indexOf("allowed") != -1 || uploadErrorMsg.indexOf("empty") != -1) {
            statusCode = 400;
          } else if (uploadErrorMsg.indexOf("full") != -1 || uploadErrorMsg.indexOf("space") != -1) {
            statusCode = 507;
          }
          
          // Build detailed diagnostic JSON
          String json = "{\"ok\":false";
          json += ",\"error\":\"" + uploadErrorMsg + "\"";
          json += ",\"target\":\"" + uploadDiagTarget + "\"";
          json += ",\"temp\":\"" + uploadDiagTemp + "\"";
          json += ",\"backup\":\"" + uploadDiagBackup + "\"";
          json += ",\"temp_exists\":" + String(uploadDiagTempExists ? "true" : "false");
          json += ",\"temp_size\":" + String(uploadDiagTempSize);
          json += ",\"temp_header\":\"" + uploadDiagTempHeader + "\"";
          json += ",\"final_exists_before\":" + String(uploadDiagFinalExistsBefore ? "true" : "false");
          json += ",\"final_size_before\":" + String(uploadDiagFinalSizeBefore);
          json += ",\"final_header_before\":\"" + uploadDiagFinalHeaderBefore + "\"";
          json += ",\"backup_exists_before\":" + String(uploadDiagBackupExistsBefore ? "true" : "false");
          json += ",\"backup_remove_ok\":" + String(uploadDiagBackupRemoveOk ? "true" : "false");
          json += ",\"backup_rename_ok\":" + String(uploadDiagBackupRenameOk ? "true" : "false");
          json += ",\"littlefs_total\":" + String(LittleFS.totalBytes());
          json += ",\"littlefs_used\":" + String(LittleFS.usedBytes());
          json += ",\"free_heap\":" + String(ESP.getFreeHeap());
          json += "}";
          
          server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
          server.sendHeader("Pragma", "no-cache");
          server.sendHeader("Expires", "0");
          server.send(statusCode, "application/json; charset=utf-8", json);
          Serial.println("[UPLOAD] response sent");
        } else {
          String json = "{";
          json += "\"ok\":true,";
          json += "\"path\":\"" + uploadSuccessPath + "\",";
          json += "\"size\":" + String(uploadSuccessSize) + ",";
          json += "\"header\":\"" + uploadSuccessHeader + "\"";
          json += "}";
          
          server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
          server.sendHeader("Pragma", "no-cache");
          server.sendHeader("Expires", "0");
          server.send(200, "application/json; charset=utf-8", json);
          Serial.println("[UPLOAD] response sent");
          
          if (uploadSuccessPath == "/loop.gif") {
            Serial.println("[GIFRELOAD] upload success path=/loop.gif");
            preferences.begin("obd2_dash", false);
            showGifHome = true;
            preferences.putBool("gif_home", true);
            preferences.end();
            Serial.println("[GIFRELOAD] set showGifHome=true");
            
            if (!chargeAutoBackActive) {
              currentPage = 1;
              Serial.println("[GIFRELOAD] set currentPage=1");
            } else {
              Serial.println("[GIFRELOAD] currentPage change deferred due to active Charging AutoBack");
            }
            themeChanged = true;
            loopGifReloadRequested = true;
            // NOTE: Do NOT set showSetupScreen=false here.
            // The GIF reload will be processed cleanly after the phone disconnects,
            // when the WiFi upload stack has released heap memory.
            Serial.println("[GIFRELOAD] loopGifReloadRequested=true. Reload pending until station disconnects.");
            Serial.println("[HTTP] /loop.gif uploaded successfully. Reload pending.");
          } else {
            bootGifReloadRequested = true;
            pendingRestart = true;
            restartRequestTime = millis();
            Serial.println("[HTTP] /boot.gif uploaded successfully. Reboot scheduled in 2 seconds...");
          }
        }
      },
      handleUpload);
  server.on("/revert", HTTP_POST, handleRevert);
  server.on("/api/restore_default_loop", HTTP_POST, handleRestoreDefaultLoop);
  server.on("/api/restore_default_boot", HTTP_POST, handleRestoreDefaultBoot);
  server.on("/api/restore_default_all", HTTP_POST, handleRestoreDefaultAll);

#if ENABLE_GIF_DEBUG
  // Targeted cleanup: ONLY removes uploaded overrides, temp, and backup files.
  server.on("/api/clear_uploaded_gifs", HTTP_GET, []() {
    Serial.println("[HTTP] clear_uploaded_gifs: Removing user override files...");
    const char* targets[] = {
      "/boot.gif", "/loop.gif",
      "/boot_upload.tmp", "/loop_upload.tmp",
      "/boot_backup.gif", "/loop_backup.gif"
    };
    
    String filesJson = "[";
    for (int i = 0; i < 6; i++) {
      const char* path = targets[i];
      bool existed = LittleFS.exists(path);
      bool removeOk = false;
      if (existed) {
        removeOk = LittleFS.remove(path);
      }
      bool existsAfter = LittleFS.exists(path);
      
      if (i > 0) filesJson += ",";
      filesJson += "{";
      filesJson += "\"path\":\"" + String(path) + "\",";
      filesJson += "\"existed_before\":" + String(existed ? "true" : "false") + ",";
      filesJson += "\"remove_ok\":" + String(removeOk ? "true" : "false") + ",";
      filesJson += "\"exists_after\":" + String(existsAfter ? "true" : "false");
      filesJson += "}";
    }
    filesJson += "]";

    String json = "{\"ok\":true";
    json += ",\"files\":" + filesJson;
    json += ",\"rebooting\":true";
    json += "}";
    
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json; charset=utf-8", json);
    
    pendingRestart = true;
    restartRequestTime = millis();
    Serial.println("[HTTP] clear_uploaded_gifs completed. Rebooting in 2 seconds...");
  });

  server.on("/api/gif_diag", HTTP_GET, handleGifDiag);

  server.on("/api/gif_file_status", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");

    bool bootExists = LittleFS.exists("/boot.gif");
    size_t bootSize = 0;
    String bootHeader = "";
    if (bootExists) {
      File f = LittleFS.open("/boot.gif", "r");
      if (f) {
        bootSize = f.size();
        uint8_t header[6];
        if (f.read(header, 6) == 6) {
          char hBuf[7];
          memcpy(hBuf, header, 6);
          hBuf[6] = '\0';
          bootHeader = String(hBuf);
        }
        f.close();
      }
    }

    bool loopExists = LittleFS.exists("/loop.gif");
    size_t loopSize = 0;
    String loopHeader = "";
    if (loopExists) {
      File f = LittleFS.open("/loop.gif", "r");
      if (f) {
        loopSize = f.size();
        uint8_t header[6];
        if (f.read(header, 6) == 6) {
          char hBuf[7];
          memcpy(hBuf, header, 6);
          hBuf[6] = '\0';
          loopHeader = String(hBuf);
        }
        f.close();
      }
    }

    String json = "{";
    json += "\"boot_exists\":" + String(bootExists ? "true" : "false") + ",";
    json += "\"boot_size\":" + String(bootSize) + ",";
    json += "\"boot_header\":\"" + bootHeader + "\",";
    json += "\"loop_exists\":" + String(loopExists ? "true" : "false") + ",";
    json += "\"loop_size\":" + String(loopSize) + ",";
    json += "\"loop_header\":\"" + loopHeader + "\",";
    json += "\"littlefs_total\":" + String(LittleFS.totalBytes()) + ",";
    json += "\"littlefs_used\":" + String(LittleFS.usedBytes()) + ",";
    json += "\"free_heap\":" + String(ESP.getFreeHeap());
    json += "}";
    server.send(200, "application/json; charset=utf-8", json);
  });

  server.on("/api/display_gif_state", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");

    bool loopExists = LittleFS.exists("/loop.gif");
    size_t loopSize = 0;
    String loopHeader = "";
    if (loopExists) {
      File f = LittleFS.open("/loop.gif", "r");
      if (f) {
        loopSize = f.size();
        uint8_t header[6];
        if (f.read(header, 6) == 6) {
          char hBuf[7];
          memcpy(hBuf, header, 6);
          hBuf[6] = '\0';
          loopHeader = String(hBuf);
        }
        f.close();
      }
    }

    String json = "{";
    json += "\"currentPage\":" + String(currentPage) + ",";
    json += "\"showGifHome\":" + String(showGifHome ? "true" : "false") + ",";
    json += "\"themeChanged\":" + String(themeChanged ? "true" : "false") + ",";
    json += "\"loopGifReloadRequested\":" + String(loopGifReloadRequested ? "true" : "false") + ",";
    json += "\"theme2GifOpen\":" + String(theme2GifOpen ? "true" : "false") + ",";
    json += "\"inChargingMode\":" + String(chargeAutoBackActive ? "true" : "false") + ",";
    json += "\"autoBackActive\":" + String(autoBackActive ? "true" : "false") + ",";
    json += "\"selectedHomeMode\":\"" + String(showGifHome ? "gif" : "robo") + "\",";
    json += "\"loop_file_exists\":" + String(loopExists ? "true" : "false") + ",";
    json += "\"loop_file_size\":" + String(loopSize) + ",";
    json += "\"loop_file_header\":\"" + loopHeader + "\",";
    json += "\"lastGifSource\":\"" + lastGifSource + "\",";
    json += "\"lastGifOpenResult\":" + String(lastGifOpenResult ? "true" : "false") + ",";
    json += "\"lastGifOpenError\":\"" + lastGifOpenError + "\"";
    json += "}";
    server.send(200, "application/json; charset=utf-8", json);
  });
#endif
  server.on("/api/status", HTTP_GET, []() {
    String json = "{";
    json += "\"free_heap\":" + String(ESP.getFreeHeap() / 1024) + ",";
    json += "\"fs_used\":" + String(LittleFS.usedBytes() / 1024) + ",";
    json += "\"fs_total\":" + String(LittleFS.totalBytes() / 1024) + ",";
    json += "\"profile\":" + String(car_profile) + ",";
    json += "\"rpm_source\":" + String(rpmSourceMode) + ",";
    json += "\"rpm_source_label\":\"" + rpmSourceLabel + "\",";
    json += "\"mmcu_rpm_raw\":" + String(mmcuRpmRaw) + ",";
    json += "\"mmcu_rpm_abs\":" + String(mmcuRpmDisplayAbs) + ",";
    json += "\"mmcu_valid\":" + String(mmcuRpmValid ? "true" : "false") + ",";
    json += "\"mmcu_resp_count\":" + String(mmcuResponseCount) + ",";
    json += "\"mmcu_last_error\":\"" + mmcuLastError + "\",";

    // Energy/Cost API fields
    json += "\"energy_source_mode\":" + String(energySourceMode) + ",";
    json += "\"energy_source_label\":\"" + energySourceLabel + "\",";
    json += "\"energy_valid\":" + String(energyValid ? "true" : "false") + ",";
    json += "\"energy_error\":\"" + energyError + "\",";
    json += "\"trip_kwh_used\":" + String(tripKwhUsed, 4) + ",";
    json += "\"trip_regen_kwh\":" + String(tripRegenKwh, 4) + ",";
    json += "\"instant_power_kw\":" + String(instantPowerKw, 3) + ",";
    json += "\"trip_cost_baht\":" + String(tripCostBaht, 2) + ",";
    json += "\"elec_rate\":" + String(electricityRateBahtPerKwh, 2) + ",";
    json += "\"hv_volt\":" + String(hvBatteryVoltage, 1) + ",";
    json += "\"hv_curr\":" + String(hvBatteryCurrent, 1) + ",";
    json += "\"hv_power\":" + String(batteryPowerKw, 3) + ",";
    json += "\"bms_discharge_now\":" + String(accumulatedDischargeNow, 3) + ",";
    json +=
        "\"bms_discharge_start\":" + String(accumulatedDischargeStart, 3) + ",";
    json += "\"energy_22000e_raw\":" + String(energy22000Eraw, 2) + ",";
    json += "\"energy_22000e_delta\":" + String(energy22000Edelta, 3) + ",";
    json +=
        "\"hv_volt_valid\":" + String(hvVoltageValid ? "true" : "false") + ",";
    json +=
        "\"hv_curr_valid\":" + String(hvCurrentValid ? "true" : "false") + ",";
    json +=
        "\"hv_power_ready\":" + String(isHvPowerReady() ? "true" : "false") +
        ",";
    json += "\"hv_volt_last_update\":" + String(hvVoltageLastUpdateMs) + ",";
    json += "\"hv_curr_last_update\":" + String(hvCurrentLastUpdateMs) + ",";
    json +=
        "\"energy_22000e_adjust\":" + String(energy22000EAdjustKwh, 4) + ",";

    // New connection and debug variables
    json += "\"obd_connected\":" + String(obdConnected ? "true" : "false") + ",";
    json += "\"obd_connecting\":" + String(obdConnecting ? "true" : "false") + ",";

    // ── OBD Health Abstraction Fields (new) ──────────────────────────────────────
    json += "\"obd_health\":"                        + String((int)obdHealthState) + ",";
    json += "\"obd_health_label\":\""                + String(getObdHealthLabel()) + "\",";
    json += "\"obd_wifi_connected\":"                + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
    json += "\"obd_tcp_connected\":"                 + String((currentTcpState == TCP_STATE_ACTIVE && obdClient.connected()) ? "true" : "false") + ",";
    json += "\"obd_elm_ready\":"                     + String(currentElmState == ELM_INIT_CONFIRMED ? "true" : "false") + ",";
    json += "\"obd_data_fresh\":"                    + String(isObdDataFresh() ? "true" : "false") + ",";
    json += "\"obd_health_warning_count\":"          + String(obdHealthWarningCount) + ",";
    json += "\"obd_health_disconnect_count\":"       + String(obdHealthDisconnectCount) + ",";
    json += "\"obd_health_recovery_success_count\":" + String(obdHealthRecoverySuccessCount) + ",";

    json += "\"obd_last_good_frame\":" + String(obdLastGoodFrameMs) + ",";
    json += "\"obd_last_response\":" + String(obdLastResponseMs) + ",";
    json += "\"obd_reconnect_count\":" + String(obdReconnectCount) + ",";
    json += "\"obd_consecutive_timeouts\":" + String(obdConsecutiveTimeouts) + ",";
    json += "\"obd_last_error_reason\":\"" + obdLastErrorReason + "\",";
    json += "\"display_target_speed\":" + String(displayTargetSpeed, 1) + ",";
    json += "\"display_target_rpm\":" + String(displayTargetRpm, 1) + ",";


    // Comparison notes & connection debug fields
    json +=
        "\"car_display_energy_val\":" + String(carDisplayEnergyVal, 3) + ",";
    json += "\"car_display_energy_unit\":\"" + carDisplayEnergyUnit + "\",";
    json += "\"car_display_energy_label\":\"" + carDisplayEnergyLabel + "\",";
    json += "\"wifi_disconnect_reason\":" + String(wifiDisconnectReason) + ",";
    json += "\"wifi_reconnect_count\":" + String(wifiReconnectCount) + ",";
    json += "\"tcp_reconnect_count\":" + String(tcpReconnectCount) + ",";
    json += "\"last_wifi_disconnect_ms\":" + String(lastWiFiDisconnectMs) + ",";
    json += "\"last_tcp_disconnect_ms\":" + String(lastTCPDisconnectMs) + ",";
    json += "\"wifi_rssi\":" + String(wifiRssi) + ",";
    json += "\"wifi_ap_state\":" + String((int)currentApState) + ",";
    json += "\"wifi_ap_channel\":" + String((unsigned)currentApChannel) + ",";
    json += "\"wifi_sta_state\":" + String((int)currentStaState) + ",";
    json += "\"wifi_tcp_state\":" + String((int)currentTcpState) + ",";
    json += "\"wifi_elm_state\":" + String((int)currentElmState) + ",";
    json += "\"wifi_scan_running\":" + String(scanRunning ? "true" : "false") + ",";
    json += "\"wifi_scan_age_ms\":" + String(scanRunning && scanStartMs > 0 ? (unsigned long)(millis() - scanStartMs) : 0) + ",";
    json += "\"wifi_scan_recovery_count\":" + String(scanRecoveryCount) + ",";
    json += "\"wifi_scan_timeout_count\":" + String(scanTimeoutCount) + ",";
    json += "\"wifi_scan_fail_count\":" + String(scanFailCount) + ",";
    json += "\"wifi_disconnect_recovery_count\":" + String(wifiDisconnectRecoveryCount) + ",";
    json += "\"portal_state\":" + String((int)currentPortalState) + ",";
    json += "\"channel_sync_state\":" + String((int)channelSyncState) + ",";
    json +=
        "\"selected_vehicle_profile\":\"" + selected_vehicle_profile + "\",";
    String profileStatus = "WAITING_FOR_PROFILE_DATA";
    const VehicleProfile *sp = findVehicleProfile(selected_vehicle_profile);
    if (sp != nullptr) {
      profileStatus = sp->status;
    }
    json += "\"vehicle_profile_status\":\"" + profileStatus + "\",";
    json += "\"connection_mode\":\"" + connectionMode + "\",";

    // New AutoBack, Charging, and Driving Time fields
    json +=
        "\"autoback_enabled\":" + String(autoBackEnabled ? "true" : "false") +
        ",";
    json += "\"autoback_type\":" + String(autoBackPageType) + ",";
    json += "\"autoback_active\":" + String(autoBackActive ? "true" : "false") +
            ",";
    json += "\"charge_target_soc\":" + String(chargeTargetSoc) + ",";
    json += "\"charging_home_type\":" + String(chargingHomeType) + ",";
    json +=
        "\"in_charging_mode\":" + String(chargeAutoBackActive ? "true" : "false") +
        ",";
    json += "\"charging_sub_page\":" + String(chargingSubPage) + ",";
    json += "\"avg_charge_power_kw\":" + String(avgChargePowerKw, 3) + ",";

    // Charge Estimate telemetry (FINAL LOCK v1.3)
    String etaJsonStr = (std::isfinite(chargeEtaMinutes) && chargeEtaMinutes >= 0.0f)
                        ? String(chargeEtaMinutes, 1)
                        : "-1.0";
    json += "\"charge_state\":" + String((int)chargeEstimateState) + ",";
    json += "\"charge_detected\":" + String(chargeDetectedRaw ? "true" : "false") + ",";
    json += "\"charge_confirmed\":" + String((chargeEstimateState == CHG_EST_ACTIVE) ? "true" : "false") + ",";
    json += "\"charge_direction\":\"" + String(chargeDirectionValid ? "CHARGE" : "NOT_CHG") + "\",";
    json += "\"charge_power_inst\":" + String(std::isfinite(chargeInstantPowerKw) ? chargeInstantPowerKw : 0.0f, 3) + ",";
    json += "\"charge_power_avg\":" + String(std::isfinite(avgChargePowerKw) ? avgChargePowerKw : 0.0f, 3) + ",";
    json += "\"charge_power_valid\":" + String(chargePowerValid ? "true" : "false") + ",";
    json += "\"charge_power_samples\":" + String(chargeHistoryCount) + ",";
    json += "\"charge_soc_delta\":" + String(std::isfinite(chargeSocDelta) ? chargeSocDelta : 0.0f, 2) + ",";
    json += "\"charge_energy_needed\":" + String(std::isfinite(chargeEnergyNeededKwh) ? chargeEnergyNeededKwh : 0.0f, 3) + ",";
    json += "\"charge_eta_minutes\":" + etaJsonStr + ",";
    json += "\"charge_eta_status\":\"" + chargeEtaStatusStr + "\",";

    json += "\"driving_today\":0,";
    json += "\"driving_week\":0,";

    // SOC Diagnostic Fields — SOC V1.0
    // [R17] API Consistency Invariants:
    //   When soc_valid == true  → soc in 0-100, soc_source != NONE (0)
    //   When soc_valid == false → soc == -1.0,  soc_source == NONE (0)
    //   soc_coarse represents last parser-valid sample (may be stale).
    //   soc_last_update = ESP32 uptime ms when the validated VP_PID_SOC response was processed/received by the ESP32 [R5]
    json += "\"soc\":"             + String(socFinalValid ? batt_soc : -1.0f, 2)      + ",";
    json += "\"soc_valid\":"       + String(socFinalValid ? "true" : "false")          + ",";
    json += "\"soc_source\":"      + String((int)socSource)                            + ",";
    json += "\"soc_coarse\":"      + String(socCoarseValid   ? socBmsCoarse    : -1.0f, 2) + ",";
    json += "\"soc_detailed\":"    + String(socDetailedValid  ? socBmsDetailed  : -1.0f, 2) + ",";
    json += "\"soc_difference\":"  + String(socSourceDifference, 2)                    + ",";
    json += "\"soc_last_update\":" + String(socFinalLastUpdateMs)                      + ",";

    json += "\"slots\":[";
    for (int i = 0; i < 6; i++) {
      json += String(config_slots[i]);
      if (i < 5)
        json += ",";
    }
    json += "]";
    json += "}";
    server.send(200, "application/json", json);
  });
  server.on("/save_config", HTTP_POST, []() {
    preferences.begin("obd2_dash", false);

    // Save energy comparison notes
    if (server.hasArg("car_eng_val")) {
      carDisplayEnergyVal = server.arg("car_eng_val").toFloat();
      preferences.putFloat("car_eng_val", carDisplayEnergyVal);
      Serial.printf("[HTTP] Saved carDisplayEnergyVal: %.3f\n",
                    carDisplayEnergyVal);
    }
    if (server.hasArg("car_eng_unit")) {
      carDisplayEnergyUnit = server.arg("car_eng_unit");
      preferences.putString("car_eng_unit", carDisplayEnergyUnit);
      Serial.printf("[HTTP] Saved carDisplayEnergyUnit: %s\n",
                    carDisplayEnergyUnit.c_str());
    }
    if (server.hasArg("car_eng_lbl")) {
      carDisplayEnergyLabel = server.arg("car_eng_lbl");
      preferences.putString("car_eng_lbl", carDisplayEnergyLabel);
      Serial.printf("[HTTP] Saved carDisplayEnergyLabel: %s\n",
                    carDisplayEnergyLabel.c_str());
    }

    // Save selected vehicle profile (NVS guard against non-selectable profiles)
    String new_profile = "BYD_DOLPHIN_TH_EV";
    if (server.hasArg("selected_vehicle_profile")) {
      new_profile = server.arg("selected_vehicle_profile");
      new_profile.trim();
    }
    if (new_profile == "" || new_profile == "null" || new_profile == "undefined") {
      new_profile = "BYD_DOLPHIN_TH_EV";
    }
    const VehicleProfile *p = findVehicleProfile(new_profile);
    if (p == nullptr || !p->selectableInProduction) {
      new_profile = "BYD_DOLPHIN_TH_EV";
    }
    selected_vehicle_profile = new_profile;
    car_profile = 0; // BYD_DOLPHIN_TH_EV maps to car_profile 0 in production

    preferences.putString("selected_profile", selected_vehicle_profile);
    preferences.putInt("car_profile", car_profile);
    Serial.printf("[HTTP] Saved selected_profile: %s, mapped car_profile: %d\n",
                  selected_vehicle_profile.c_str(), car_profile);

    if (server.hasArg("dolphin_test_mode")) {
      dolphin_test_mode = server.arg("dolphin_test_mode").toInt();
      preferences.putInt("dolphin_test", dolphin_test_mode);
      Serial.printf("[HTTP] Saved dolphin_test_mode: %d\n", dolphin_test_mode);
    }

    if (server.hasArg("rpm_source")) {
      rpmSourceMode = 1; // Force EST mode internally (ignore form parameter)
      preferences.putInt("rpm_source", 1);
      Serial.printf("[HTTP] Saved rpmSourceMode forced: %d\n", rpmSourceMode);
    }

    if (server.hasArg("elec_rate")) {
      electricityRateBahtPerKwh = server.arg("elec_rate").toFloat();
      if (electricityRateBahtPerKwh < 0.0f)
        electricityRateBahtPerKwh = 4.50f;
      preferences.putFloat("elec_rate", electricityRateBahtPerKwh);
      Serial.printf("[HTTP] Saved electricity rate: %.2f\n",
                    electricityRateBahtPerKwh);
    }

    if (server.hasArg("energy_source")) {
      energySourceMode = server.arg("energy_source").toInt();
      if (energySourceMode < 0 || energySourceMode > 4)
        energySourceMode = 0;
      preferences.putInt("energy_mode", energySourceMode);
      Serial.printf("[HTTP] Saved energySourceMode: %d\n", energySourceMode);
    }

    // Save AutoBack configurations
    if (server.hasArg("autoback_enabled")) {
      autoBackEnabled = (server.arg("autoback_enabled").toInt() == 1);
      preferences.putBool("ab_enabled", autoBackEnabled);
      Serial.printf("[HTTP] Saved autoBackEnabled: %d\n", autoBackEnabled);
    }
    if (server.hasArg("autoback_type")) {
      autoBackPageType = server.arg("autoback_type").toInt();
      preferences.putInt("ab_type", autoBackPageType);
      Serial.printf("[HTTP] Saved autoBackPageType: %d\n", autoBackPageType);
    }

    // Save Charging configurations
    if (server.hasArg("charge_target_soc")) {
      int target = server.arg("charge_target_soc").toInt();
      if (target < 0 || target > 100) target = 80;
      target = ((target + 5) / 10) * 10;
      target = constrain(target, 0, 100);
      chargeTargetSoc = target;
      preferences.putInt("chg_target", chargeTargetSoc);
      Serial.printf("[HTTP] Saved chargeTargetSoc: %d\n", chargeTargetSoc);
    }
    if (server.hasArg("charging_home_type")) {
      chargingHomeType = server.arg("charging_home_type").toInt();
      preferences.putInt("chg_home", chargingHomeType);
      Serial.printf("[HTTP] Saved chargingHomeType: %d\n", chargingHomeType);
    }

    // Active Override Guard: ถ้าผู้ใช้สั่งปิด AutoBack ใน Web Portal
    // ให้ปิดสถานะใน RAM ทันที คืนค่าหน้าจอ และรีเซ็ต timer ทั้งหมด
    if (!autoBackEnabled) {
      if (autoBackActive || chargeAutoBackActive) {
        autoBackActive = false;
        chargeAutoBackActive = false;
        currentPage = previousPageBeforeAutoBack;
      }
      stoppedDurationMs = 0;
      movingDurationMs = 0;
      chargeEntryConfirmMs = 0;
      chargeExitConfirmMs = 0;
      autoBackStopwatchStartMs = 0;
      chargeAutoBackSessionUsed = false;
      chargeAutoBackConfigArmed = false;
      chargeEstimateAutoBackOwnedScreen = false;
    }

    for (int i = 0; i < 6; i++) {
      String key = "slot" + String(i);
      if (server.hasArg(key)) {
        config_slots[i] = server.arg(key).toInt();
        preferences.putInt(key.c_str(), config_slots[i]);
        Serial.printf("[HTTP] Saved %s: %d\n", key.c_str(), config_slots[i]);
      }
    }

    preferences.end();
    server.send(200, "text/plain", "OK");
    themeChanged = true;
    Serial.println("[HTTP] Config saved. Applied values to RAM. No reboot.");
  });

  server.on("/reset_trip", HTTP_POST, []() {
    // Reset all energy variables
    tripKwhUsed = 0.0f;
    tripRegenKwh = 0.0f;
    energy22000EAdjustKwh = 0.0f;
    discharge_kwh = 0.0f;
    energySessionStarted = false;
    regen_kwh = 0.0f;
    energy_kwh = 0.0f;
    tripCostBaht = 0.0f;

    // Reset VP_PID_ENERGY fallback baseline
    last_energy_raw = -1.0f;
    energy22000Eraw = 0.0f;
    energy22000Edelta = 0.0f;

    // Reset integration timer to prevent dt jump after reset
    lastEnergyUpdateMs = 0;

    // Safe BMS accumulated baseline
    if (accumulatedDischargeValidated && accumulatedDischargeNow > 0.0f) {
      accumulatedDischargeStart = accumulatedDischargeNow;
    } else {
      accumulatedDischargeStart =
          -1.0f; // wait for next valid accumulated value
    }

    // Reset validation state for fresh validation after reset
    monotonicCount = 0;
    accumulatedDischargeValidated = false;
    lastAccumulatedDischargeVal = -1.0f;

    Serial.println("[HTTP] Trip energy manually reset via web portal.");
    server.send(200, "text/plain", "OK");
  });

#if 0
  server.on("/api/sync_time", HTTP_POST, []() {
    if (server.hasArg("date")) {
      String realDate = server.arg("date");
      realDate.trim();
      Serial.printf("[DrivingTime] /api/sync_time called. Received date: '%s', previous currentDate: '%s'\n",
                    realDate.c_str(), currentDate.c_str());

      bool isValid = (dateToDays(realDate) != -1);
      if (isValid) {
        bool willMerge = LittleFS.exists("/driving_time.json") && (currentDate == "NoSync");
        Serial.printf("[DrivingTime] Date is valid. mergeNoSyncDate() will be called: %s\n",
                      willMerge ? "YES" : "NO");

        MergeResult mergeRes = mergeNoSyncDate(realDate);
        if (mergeRes == MERGE_FAILED) {
          String prevDate = currentDate;
          currentDate = realDate;
          loadDrivingTime();
          Serial.printf("[DrivingTime] mergeNoSyncDate failed! currentDate updated: %s -> %s. Bypassing pruning.\n",
                        prevDate.c_str(), currentDate.c_str());
          server.send(200, "text/plain", "WARNING_MERGE_FAILED");
          return;
        } else {
          pruneDrivingTimeHistory(realDate);
          String prevDate = currentDate;
          currentDate = realDate;
          loadDrivingTime();
          Serial.printf("[DrivingTime] Sync success. currentDate updated: %s -> %s. history pruned and loaded.\n",
                        prevDate.c_str(), currentDate.c_str());
          server.send(200, "text/plain", "OK");
          return;
        }
      } else {
        Serial.printf("[DrivingTime] Received date '%s' is INVALID (dateToDays returned -1)\n", realDate.c_str());
      }
    }
    server.send(400, "text/plain", "Bad Request");
  });

  server.on("/api/reset_driving_time", HTTP_POST, []() {
    // Debug: check state before reset
    bool existedBefore = LittleFS.exists("/driving_time.json");
    bool tmpExisted = LittleFS.exists("/driving_time.tmp");
    Serial.printf("[DrivingTime] RESET: file exists before = %s\n", existedBefore ? "YES" : "NO");
    Serial.printf("[DrivingTime] RESET: tmp exists before = %s\n", tmpExisted ? "YES" : "NO");

    // Delete driving_time.json from LittleFS
    bool removeOk = false;
    if (existedBefore) {
      removeOk = LittleFS.remove("/driving_time.json");
      Serial.printf("[DrivingTime] RESET: remove json result = %s\n", removeOk ? "OK" : "FAIL");
    }
    bool removeTmpOk = false;
    if (tmpExisted) {
      removeTmpOk = LittleFS.remove("/driving_time.tmp");
      Serial.printf("[DrivingTime] RESET: remove tmp result = %s\n", removeTmpOk ? "OK" : "FAIL");
    }

    // Debug: verify file is gone
    bool existsAfter = LittleFS.exists("/driving_time.json");
    bool tmpExistsAfter = LittleFS.exists("/driving_time.tmp");
    Serial.printf("[DrivingTime] RESET: file exists after = %s\n", existsAfter ? "YES (ERROR!)" : "NO (OK)");

    // Reset all in-memory driving counters
    drivingSecondsToday = 0;
    drivingSecondsThisWeek = 0;
    drivingTimeAccumulatedMs = 0;
    lastDrivingPersistMs = millis();
    // Keep currentDate if already synced, otherwise stays NoSync
    Serial.printf("[DrivingTime] RESET: currentDate=%s today=%d week=%d accMs=%d\n",
                  currentDate.c_str(), drivingSecondsToday, drivingSecondsThisWeek, drivingTimeAccumulatedMs);

    String out = "{";
    out += "\"ok\":true,";
    out += "\"json_existed_before\":" + String(existedBefore ? "true" : "false") + ",";
    out += "\"json_remove_ok\":" + String(removeOk ? "true" : "false") + ",";
    out += "\"json_exists_after\":" + String(existsAfter ? "true" : "false") + ",";
    out += "\"tmp_existed_before\":" + String(tmpExisted ? "true" : "false") + ",";
    out += "\"tmp_remove_ok\":" + String(removeTmpOk ? "true" : "false") + ",";
    out += "\"tmp_exists_after\":" + String(tmpExistsAfter ? "true" : "false") + ",";
    out += "\"current_date\":\"" + currentDate + "\",";
    out += "\"today_seconds\":" + String(drivingSecondsToday) + ",";
    out += "\"week_seconds\":" + String(drivingSecondsThisWeek) + ",";
    out += "\"accumulated_ms\":" + String(drivingTimeAccumulatedMs);
    out += "}";

    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
    server.send(200, "application/json", out);
  });

#if ENABLE_DEBUG_API
  server.on("/api/debug_driving_time", HTTP_GET, []() {
    String rawJson = "";
    int jsonSize = 0;
    bool jsonExists = LittleFS.exists("/driving_time.json");
    if (!jsonExists) {
      rawJson = "FILE_MISSING";
    } else {
      File f = LittleFS.open("/driving_time.json", "r");
      if (f) {
        jsonSize = f.size();
        rawJson = f.readString();
        f.close();
        if (rawJson.length() > 2048) {
          rawJson = rawJson.substring(0, 2048) + "...[TRUNCATED]";
        }
      } else {
        rawJson = "READ_FAILED";
      }
    }
    
    String rawTmp = "";
    int tmpSize = 0;
    bool tmpExists = LittleFS.exists("/driving_time.tmp");
    if (!tmpExists) {
      rawTmp = "FILE_MISSING";
    } else {
      File f = LittleFS.open("/driving_time.tmp", "r");
      if (f) {
        tmpSize = f.size();
        rawTmp = f.readString();
        f.close();
        if (rawTmp.length() > 2048) {
          rawTmp = rawTmp.substring(0, 2048) + "...[TRUNCATED]";
        }
      } else {
        rawTmp = "READ_FAILED";
      }
    }

    bool nosyncPresent = false;
    if (jsonExists && rawJson != "READ_FAILED") {
      nosyncPresent = (rawJson.indexOf("\"date\":\"NoSync\"") != -1);
    }

    // Escape quotes and special characters
    String escapedJson = escapeJsonString(rawJson);
    String escapedTmp = escapeJsonString(rawTmp);

    String out = "{";
    out += "\"current_date\":\"" + currentDate + "\",";
    out += "\"driving_seconds_today\":" + String(drivingSecondsToday) + ",";
    out += "\"driving_seconds_this_week\":" + String(drivingSecondsThisWeek) + ",";
    out += "\"driving_time_accumulated_ms\":" + String(drivingTimeAccumulatedMs) + ",";
    out += "\"last_driving_persist_ms\":" + String(lastDrivingPersistMs) + ",";
    out += "\"driving_time_json_exists\":" + String(jsonExists ? "true" : "false") + ",";
    out += "\"driving_time_json_size\":" + String(jsonSize) + ",";
    out += "\"driving_time_json_raw\":\"" + escapedJson + "\",";
    out += "\"driving_time_tmp_exists\":" + String(tmpExists ? "true" : "false") + ",";
    out += "\"driving_time_tmp_size\":" + String(tmpSize) + ",";
    out += "\"driving_time_tmp_raw\":\"" + escapedTmp + "\",";
    out += "\"nosync_present\":" + String(nosyncPresent ? "true" : "false");
    out += "}";

    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
    server.send(200, "application/json", out);
  });
#endif

  server.on("/api/driving_time", HTTP_GET, []() {
    String daysPart = "[]";
    if (LittleFS.exists("/driving_time.json")) {
      File f = LittleFS.open("/driving_time.json", "r");
      if (f) {
        String json = f.readString();
        f.close();
        json.trim();
        // Check for structural validity in file content
        if (json.startsWith("{\"days\":[") && json.endsWith("]}")) {
          int daysIdx = json.indexOf("\"days\":");
          if (daysIdx != -1) {
            String rawDays = json.substring(daysIdx + 7);
            rawDays.trim();
            if (rawDays.endsWith("}")) {
              rawDays = rawDays.substring(0, rawDays.length() - 1);
              rawDays.trim();
            }
            if (rawDays.length() > 0 && rawDays.charAt(0) == '[' && rawDays.endsWith("]")) {
              daysPart = rawDays;
            }
          }
        }
      }
    }

    String out = "{";
    out += "\"synced\":" + String((currentDate != "NoSync") ? "true" : "false") + ",";
    out += "\"current_date\":\"" + currentDate + "\",";
    out += "\"today_seconds\":" + String(drivingSecondsToday) + ",";
    out += "\"week_seconds\":" + String(drivingSecondsThisWeek) + ",";
    out += "\"days\":" + daysPart;
    out += "}";

    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
    server.send(200, "application/json", out);
  });
#endif

  server.onNotFound([]() {
    String host = server.hostHeader();
    if (host != "192.168.4.1" && host != "device.local") {
      Serial.printf("[HTTP] Captive Redirect to setup: host=%s uri=%s\n", host.c_str(),
                    server.uri().c_str());
      server.sendHeader("Location", "http://192.168.4.1/setup", true);
      server.send(302, "text/plain", "");
    } else {
      server.send(404, "text/plain", "Not Found");
    }
  });

  server.begin();
  Serial.println("[HTTP] Web Portal Server started.");

  // Keep ESP32-managed STA reconnect policy under our own state machine.
  // AP remains active throughout; only the STA side is disconnected here.
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, false);

  obdClient.setTimeout(10);

  Serial.println("===== Init Done! =====");
  Serial.println("SHORT PRESS = next page | LONG PRESS (1.5s) = switch theme");
}

void loop() {
  // Upload inactivity auto-recovery timeout
  if (uploadInProgress && (millis() - lastUploadActivityMs > 10000)) {
    uploadInProgress = false;
    Serial.println("[UPLOAD] Timeout recovery: uploadInProgress reset to false due to 10s inactivity");
  }

  // Process Captive Portal DNS Queries
  dnsServer.processNextRequest();

  server.handleClient();

  // Deferred restart: non-blocking execution (zero delay) when safe
  if (pendingRestart && !uploadInProgress && (millis() - restartRequestTime > 2000)) {
    Serial.println("[SYS] Executing deferred restart now...");
    ESP.restart();
  }

  // Supervisor Non-Blocking Restart Handler with Final Safety Check
  if (supervisorRestartPending && (millis() - supervisorRestartRequestMs > 100)) {
    bool apHealthy = (WiFi.softAPIP() == SETUP_AP_IP);
    bool staHealthy = (WiFi.status() == WL_CONNECTED);
    bool apClientActive = setupApClientConnected || (WiFi.softAPgetStationNum() > 0);

    if (uploadInProgress || apClientActive || apHealthy || staHealthy) {
      supervisorRestartPending = false;
      wifiStackFailureStartMs = 0;
      Serial.println("[WIFI][SUPERVISOR] Level-4 restart cancelled at final safety check — system recovered or client active.");
    } else {
      Serial.println("[WIFI][SUPERVISOR] Level-4 restart final safety check passed — rebooting now!");
      ESP.restart();
    }
  }

  static unsigned long lastLoopMs = 0;
  unsigned long nowMs = millis();
  if (lastLoopMs == 0)
    lastLoopMs = nowMs;
  uint32_t dtMs = nowMs - lastLoopMs;
  lastLoopMs = nowMs;

  // Level-4 Global Wi-Fi Failure Guard (True Global Radio Failure > 120s with Hysteresis & Upload Immunity)
  bool apHealthy = (WiFi.softAPIP() == SETUP_AP_IP);
  bool staHealthy = (WiFi.status() == WL_CONNECTED);
  bool apClientActive = setupApClientConnected || (WiFi.softAPgetStationNum() > 0);

  if (!apHealthy && !staHealthy && !apClientActive && !uploadInProgress) {
    if (wifiStackFailureStartMs == 0) {
      wifiStackFailureStartMs = nowMs;
      Serial.println("[WIFI][SUPERVISOR] Global Wi-Fi failure detected — arming Level-4 timer (120s)");
    } else if (nowMs - wifiStackFailureStartMs > 120000UL) {
      // 120s expired: request restart idempotently
      if (!supervisorRestartPending) {
        Serial.println("[WIFI][SUPERVISOR] Level-4 Recovery: Wi-Fi stack unrecoverable > 120s. Requesting restart...");
        requestSupervisorRestart();
      }
    }
  } else {
    // Hysteresis Auto-Cancel: Reset timer and pending restart immediately if any component recovers
    if (wifiStackFailureStartMs != 0 || supervisorRestartPending) {
      wifiStackFailureStartMs = 0;
      supervisorRestartPending = false;
      Serial.println("[WIFI][SUPERVISOR] Level-4 timer & restart disarmed — active client / upload / healthy link.");
    }
  }

  // Consume AP Events from callback under Critical Section
  WiFiEventRecord localApEvent = {0, 0, (WiFiEvent_t)0, 0, false};
  bool hasApEvent = false;
  portENTER_CRITICAL(&wifiEventMux);
  if (latestApEvent.pending && isSequenceNewer(latestApEvent.sequence, lastProcessedApSequence)) {
    localApEvent.sequence = latestApEvent.sequence;
    localApEvent.timestampMs = latestApEvent.timestampMs;
    localApEvent.event = latestApEvent.event;
    localApEvent.disconnectReason = latestApEvent.disconnectReason;
    localApEvent.pending = true;
    latestApEvent.pending = false;
    lastProcessedApSequence = localApEvent.sequence;
    hasApEvent = true;
  }
  portEXIT_CRITICAL(&wifiEventMux);

  if (hasApEvent) {
    if (localApEvent.event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
      setupApClientConnected = true;
      setupApClientDisconnectPending = false;
    } else if (localApEvent.event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
      setupApClientConnected = false;
      setupApClientDisconnectPending = true;
    }
  }

  // Quantitative Heap Soak Diagnostics (Every 60s)
  static uint32_t lastHeapLogMs = 0;
  if (nowMs - lastHeapLogMs >= 60000) {
    lastHeapLogMs = nowMs;
    uint32_t curFree = ESP.getFreeHeap();
    uint32_t curMin = ESP.getMinFreeHeap();
    uint32_t curMaxAlloc = ESP.getMaxAllocHeap();
    if (curMin < minFreeHeap || minFreeHeap == 0) minFreeHeap = curMin;
    if (curMaxAlloc < minLargestFreeBlock || minLargestFreeBlock == 0) minLargestFreeBlock = curMaxAlloc;
    Serial.printf("[DIAG][SOAK] uptime=%lu s, curFree=%u, minFree=%u, maxAlloc=%u (bootFree=%u, minAlloc=%u)\n",
                  nowMs / 1000, curFree, minFreeHeap, curMaxAlloc, bootFreeHeap, minLargestFreeBlock);
  }

  // AP watchdog: non-destructive interface check every 5 seconds with 10s cooldown
  static uint32_t lastApWatchdogMs = 0;
  static uint32_t lastApRecoveryActionMs = 0;
  if (nowMs - lastApWatchdogMs >= 5000) {
    lastApWatchdogMs = nowMs;
    if (nowMs - lastApRecoveryActionMs >= AP_WATCHDOG_COOLDOWN_MS) {
      // Mode recovery: strictly only if mode corrupted AND STA is not in active communication
      if (WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA) {
        if (currentStaState != STA_STATE_CONNECTING && currentStaState != STA_STATE_CONNECTED) {
          Serial.println("[WIFI][SUPERVISOR] AP watchdog: restoring WIFI_AP_STA mode");
          currentApState = AP_STATE_RECOVERING;
          WiFi.mode(WIFI_AP_STA);
          enforcePowerSavePolicy();
          lastApRecoveryActionMs = nowMs;
        }
      }
      // IP recovery: restore SoftAP config without touching STA
      if (WiFi.softAPIP() != SETUP_AP_IP) {
        Serial.println("[WIFI][SUPERVISOR] AP watchdog: restoring AP IP configuration");
        currentApState = AP_STATE_RECOVERING;
        WiFi.softAPConfig(SETUP_AP_IP, SETUP_AP_GATEWAY, SETUP_AP_SUBNET);
        enforcePowerSavePolicy();
        lastApRecoveryActionMs = nowMs;
      }
    }
  }

  // Check deferred channel harmonization if pending
  checkChannelHarmonization();

  // Always run Wi-Fi STA/TCP state machines unconditionally to consume events and maintain link
  handleConnection();

  bool apStationPresent = setupApClientConnected || (WiFi.softAPgetStationNum() > 0);

  if (apStationPresent) {
    // Cancel pending disconnect immediately in loop
    setupApClientDisconnectPending = false;
    setupApDisconnectCandidateMs = 0;

    // Setup Mode Active: sync timing variables
    lastLoopMs = nowMs;

    if (currentPortalState == PORTAL_STATE_INACTIVE || !wasStationConnected) {
      currentPortalState = PORTAL_STATE_ENTER;
      logHeapGuard("PORTAL_ENTER");

      // Execute mode entry operations strictly ONCE per connection session
      if (autoBackActive) {
        autoBackActive = false;
        stoppedDurationMs = 0;
        movingDurationMs = 0;
        Serial.println("[AutoBack] Suspended due to entering Setup Mode.");
      }

      // Station just connected, stop normal screen updates and show portal info on TFT
      tft.fillScreen(COLOR_BLACK);
      tft.setTextSize(4);
      tft.setTextColor(COLOR_CYAN, COLOR_BLACK);
      tft.setCursor(20, 20);
      tft.print("SETUP PORTAL");

      tft.setTextSize(2);
      tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
      tft.setCursor(20, 76);
      tft.print("SSID: OBD2-Dashboard");
      tft.setCursor(20, 100);
      tft.print("IP  : 192.168.4.1");
      tft.setCursor(20, 130);
      tft.print("Upload from phone");

      // Free memory resources to ensure stable upload processing
      if (theme2GifOpen) {
        gif.close();
        theme2GifOpen = false;
      }

      wasStationConnected = true;
      showSetupScreen = true;
      currentApState = AP_STATE_CLIENT_CONNECTED;
      currentPortalState = uploadInProgress ? PORTAL_STATE_UPLOAD : PORTAL_STATE_ACTIVE;
      Serial.println("[WIFI][SUPERVISOR] Setup Portal Client connected. Switched to Setup Screen.");
    } else {
      currentPortalState = uploadInProgress ? PORTAL_STATE_UPLOAD : PORTAL_STATE_ACTIVE;
    }
  } else {
    // No AP client detected currently
    currentApState = AP_STATE_RUNNING;

    if (wasStationConnected) {
      // Robust candidate timestamp initialization (immune to pre-set pending flag from callback)
      if (!setupApClientDisconnectPending || setupApDisconnectCandidateMs == 0) {
        setupApClientDisconnectPending = true;
        setupApDisconnectCandidateMs = nowMs;
      }

      // Debounce disconnect: confirm absence of all stations after 400ms (handles uint32_t rollover)
      if (nowMs - setupApDisconnectCandidateMs >= SETUP_AP_DISCONNECT_DEBOUNCE_MS) {
        if (!uploadInProgress && !setupApClientConnected && (WiFi.softAPgetStationNum() == 0)) {
          currentPortalState = PORTAL_STATE_EXIT;
          tft.fillScreen(COLOR_BLACK);
          themeChanged = true;
          gifHomeNeedClear = true;

          wasStationConnected = false;
          showSetupScreen = false;

          // ── Portal Exit Recovery Transaction (PRESERVE-FIRST INVARIANT) ──
          // When exiting Setup Portal, preserve healthy connections first;
          // recover only what is actually broken.
          if (WiFi.status() == WL_CONNECTED) {
            // STA is already associated with OBD2 AP!
            // Explicit Lock: NEVER call requestStaReconnect() when STA is WL_CONNECTED!
            bool tcpElmActive = (currentTcpState == TCP_STATE_ACTIVE) &&
                                (currentElmState == ELM_INIT_CONFIRMED) &&
                                obdClient.connected();
            if (tcpElmActive) {
              // Healthy socket & confirmed ELM: PRESERVE!
              // Preserve Path Immutability: ZERO socket stop, ZERO state rewriting.
              // Note: STATE_ACTIVE is managed by health logic / telemetry freshness.
              Serial.println("[WIFI][SUPERVISOR] Portal exit: STA and TCP/ELM healthy. Preserving connection.");
            } else if (currentTcpState == TCP_STATE_CONNECTING) {
              // Handshake already in flight: continue without interruption.
              currentState = STATE_TCP_CONNECTING;
              Serial.println("[WIFI][SUPERVISOR] Portal exit: STA associated, continuing TCP connect.");
            } else {
              // Last-Moment Recheck before recovery:
              bool recheckActive = (currentTcpState == TCP_STATE_ACTIVE) &&
                                   (currentElmState == ELM_INIT_CONFIRMED) &&
                                   obdClient.connected();
              if (recheckActive) {
                Serial.println("[WIFI][SUPERVISOR] Portal exit: Connection recovered during recheck. Preserving.");
              } else {
                // TCP/ELM broken or idle: perform clean TCP/ELM recovery
                currentState = STATE_TCP_CONNECTING;
                obdConnected = false;
                currentTcpState = TCP_STATE_CONNECTING;
                currentElmState = ELM_INIT_IDLE;
                obdClient.stop();
                Serial.println("[WIFI][SUPERVISOR] Portal exit: STA associated, recovering TCP/ELM.");
              }
            }
          } else {
            // STA is genuinely not connected, trigger standard STA reconnect
            requestStaReconnect();
          }

          setupApClientDisconnectPending = false;
          setupApDisconnectCandidateMs = 0;

          currentPortalState = PORTAL_STATE_INACTIVE;
          logHeapGuard("PORTAL_EXIT");
          Serial.println("[WIFI][SUPERVISOR] No AP client. Leaving SETUP PORTAL and resuming dashboard.");

          // Execute deferred channel sync if one was pending while client was connected
          checkChannelHarmonization();
        }
      }
    }

    // Normal Dashboard Mode processing (only when NOT in Setup Mode)
    if (!wasStationConnected) {
      currentPortalState = PORTAL_STATE_INACTIVE;
      checkTouchInput();

      // Run feature logic layers only in normal mode
      updateChargingModeLogic(vehicleSpeedKmh, dtMs);
      updateAutoBack(vehicleSpeedKmh, dtMs);
      // updateDrivingTime(vehicleSpeedKmh, dtMs);

      // Render gauge UI when not uploading
      if (!uploadInProgress) {
        drawGaugeUI();
      }
    }
  }

  // Yield CPU to FreeRTOS scheduler to prevent overheating and system lag
  delay(2);
}