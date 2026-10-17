#pragma once

// ------------------------------------------------------------
// Presentation-only gate.
// MUST NOT be interpreted as OBD health or connection state.
// MUST NOT mutate obdHealthState, wifiEverConnected, obdLastGoodFrameMs,
// currentStaState, currentTcpState, currentElmState, or any OBD/Wi-Fi state.
// ------------------------------------------------------------
inline bool isObdPreConnectionPresentation(wl_status_t wifiStatus) {
  // 1. Explicitly disconnected by the health state machine
  if (obdHealthState == OBD_HEALTH_DISCONNECTED) {
    return true;
  }
  // 2. No Wi-Fi station link to OBD adapter (boot, portal exit, scanning, idle)
  // When STA is not associated, the UI must use pre-connection presentation
  // because an active OBD telemetry link cannot currently exist.
  // STATE_WIFI_CONNECTING is verified from dashboard_config.h line 113.
  if (wifiStatus != WL_CONNECTED || currentState == STATE_WIFI_CONNECTING) {
    return true;
  }
  return false;
}

inline bool isObdPreConnectionPresentation() {
  return isObdPreConnectionPresentation(WiFi.status());
}

// ------------------------------------------------------------
// Theme 02 Home: looping GIF
// ------------------------------------------------------------
void drawGIFHome_T02() {
  static unsigned long nextFrameTime = 0;

  // Periodic check log (every 2 seconds) to avoid serial spam
  static unsigned long lastEnteredLogMs = 0;
  if (millis() - lastEnteredLogMs > 2000) {
    Serial.println("[GIFRELOAD] drawGIFHome_T02 entered");
    lastEnteredLogMs = millis();
  }

  if (gifHomeNeedClear) {
    tft.fillScreen(COLOR_BLACK);
    memset(gifCanvas, 0, sizeof(gifCanvas));
    gifHomeNeedClear = false;
    nextFrameTime = millis();
  }

  // Throttle retries if open failed previously
  if (gifOpenFailed && (millis() - lastGifOpenAttemptMs < 5000)) {
    // Show code-drawn safe screen
    tft.setTextSize(3);
    tft.setTextColor(COLOR_CYAN, COLOR_BLACK);
    tft.setCursor(60, 60);
    tft.print("DASHBOARD");
    tft.setTextSize(2);
    tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
    tft.setCursor(50, 100);
    tft.print("GIF Load Failed");
    return;
  }

  if (!theme2GifOpen) {
    gif.begin(GIF_PALETTE_RGB565_LE);
    // Clear canvas buffer in RAM to prevent stale compositor state, without TFT flicker
    memset(gifCanvas, 0, sizeof(gifCanvas));

    lastGifOpenAttemptMs = millis();
    bool opened = openGIFSafely(gif, "/loop.gif");

    if (opened) {
      theme2GifOpen = true;
      gifOpenFailed = false;
      nextFrameTime = millis();
      Serial.println("[GIF] Loop GIF opened successfully");
    } else {
      gifOpenFailed = true;
      tft.fillScreen(COLOR_BLACK);
      tft.setTextSize(3);
      tft.setTextColor(COLOR_CYAN, COLOR_BLACK);
      tft.setCursor(60, 60);
      tft.print("DASHBOARD");
      tft.setTextSize(2);
      tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
      tft.setCursor(50, 100);
      tft.print("GIF Load Failed");
      Serial.printf("[GIF] Failed to open loop GIF! Error code: %d\n",
                    gif.getLastError());
      return;
    }
  }

  if (millis() >= nextFrameTime) {
    int delayMs = 0;
    int rc = gif.playFrame(false, &delayMs);
    if (rc < 0) {
      // ONLY close and reset on error (rc < 0)
      Serial.printf("[GIF] playFrame error: %d, last error: %d\n", rc,
                     gif.getLastError());
      gif.close();
      theme2GifOpen = false;
      gifHomeNeedClear = true;
    } else {
      // rc == 1 (active frame) or rc == 0 (last frame).
      // If last frame (rc == 0), clear RAM canvas buffer for the next loop
      // start, but do NOT close/reopen the file to prevent file-opening
      // stutter.
      if (rc == 0) {
        memset(gifCanvas, 0, sizeof(gifCanvas));
      }
      if (delayMs <= 0)
        delayMs = 33; // safe floor, not 100ms

      if (millis() > nextFrameTime + 150) {
        // If we fell too far behind, resync timer to current time to avoid
        // rapid catch-up frame stutter
        nextFrameTime = millis() + delayMs;
      } else {
        // Standard accumulation timing to absorb decode/draw execution overhead
        nextFrameTime += delayMs;
      }
    }
  }
}

// ============================================================
// THEME 01 — Original Robo Eyes + Segmented Bar
// ============================================================

void drawRoboEyes_T01() {
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();
  if (now - lastUpdate <
      33) // 30 FPS for LCD response time sync and SPI stability
    return;
  lastUpdate = now;

  static float currentOffsetX = 0.0;
  static float currentOffsetY = 0.0;
  static float currentHeight = 86.0;
  static float targetOffsetX = 0.0;
  static float targetOffsetY = 0.0;
  static float targetHeight = 86.0;

  // Decoupled timers for blink and eye movements
  static unsigned long lastBlinkTime = 0;
  static unsigned long nextBlinkDelay = 6000;
  static bool blinkActive = false;

  static unsigned long lastMoveTime = 0;
  static unsigned long nextMoveDelay = 3000;

  // 1. Blinking Logic (Decoupled & Less Frequent)
  if (!blinkActive) {
    if (now - lastBlinkTime > nextBlinkDelay) {
      blinkActive = true;
      targetHeight = 4.0; // Close eye
      lastBlinkTime = now;
      nextBlinkDelay = random(8000, 16000); // Blink every 8 to 16 seconds
    }
  } else {
    // If eyes are fully closed, open them back up
    if (currentHeight <= 6.0f) {
      targetHeight = 86.0; // Reopen eye
      blinkActive = false;
    }
  }

  // 2. Movement Logic (Look around every 3-7 seconds, only when not in the
  // middle of closing for a blink)
  if (!blinkActive && (targetHeight >= 80.0f)) {
    if (now - lastMoveTime > nextMoveDelay) {
      lastMoveTime = now;
      nextMoveDelay = random(3000, 7000); // Move eyes every 3 to 7 seconds

      // Determine if looking around or looking center
      int randAction = random(0, 100);
      if (randAction < 30) {
        // Return to center
        targetOffsetX = 0.0;
        targetOffsetY = 0.0;
      } else {
        // Look somewhere else
        targetOffsetX = random(-45, 46);
        targetOffsetY = random(-20, 21);
      }
    }

    // Subtle micro-movements (saccades) when eyes are relatively static
    static unsigned long lastSaccadeTime = 0;
    if (now - lastSaccadeTime > (unsigned long)random(1500, 3500)) {
      lastSaccadeTime = now;
      if (abs(targetOffsetX - currentOffsetX) < 3.0f &&
          abs(targetOffsetY - currentOffsetY) < 3.0f) {
        targetOffsetX += random(-12, 13) / 10.0f;
        targetOffsetY += random(-8, 9) / 10.0f;

        // Constrain targets
        if (targetOffsetX > 45.0f)
          targetOffsetX = 45.0f;
        if (targetOffsetX < -45.0f)
          targetOffsetX = -45.0f;
        if (targetOffsetY > 20.0f)
          targetOffsetY = 20.0f;
        if (targetOffsetY < -20.0f)
          targetOffsetY = -20.0f;
      }
    }
  }

  // 3. Smooth Exponential Easing (Low-Pass Filter) instead of spring physics
  currentOffsetX += (targetOffsetX - currentOffsetX) * 0.15f;
  currentOffsetY += (targetOffsetY - currentOffsetY) * 0.15f;
  currentHeight += (targetHeight - currentHeight) * 0.25f;

  // Snap to target coordinates if very close, preventing float boundary
  // noise/border ripple
  if (abs(targetOffsetX - currentOffsetX) < 0.1f) {
    currentOffsetX = targetOffsetX;
  }
  if (abs(targetOffsetY - currentOffsetY) < 0.1f) {
    currentOffsetY = targetOffsetY;
  }
  if (abs(targetHeight - currentHeight) < 0.1f) {
    currentHeight = targetHeight;
  }

  // True double buffering using global gifCanvas
  memset(gifCanvas, 0, sizeof(gifCanvas));

  int eyeWidth = 52;
  int eyeHeight = (int)currentHeight;
  int cornerRadius = 12;
  if (cornerRadius > eyeHeight / 2)
    cornerRadius = eyeHeight / 2;
  if (cornerRadius < 1)
    cornerRadius = 1;

  // Absolute coordinates on the full 320x172 screen (using rounding instead of
  // truncation)
  int leftEyeX = 106 +
                 (int)(currentOffsetX + (currentOffsetX >= 0 ? 0.5f : -0.5f)) -
                 eyeWidth / 2;
  int rightEyeX = 214 +
                  (int)(currentOffsetX + (currentOffsetX >= 0 ? 0.5f : -0.5f)) -
                  eyeWidth / 2;
  int centerY =
      86 + (int)(currentOffsetY + (currentOffsetY >= 0 ? 0.5f : -0.5f));
  int eyeY = centerY - eyeHeight / 2;

  // Draw rounded rectangles directly in 16-bit buffer
  auto fillRoundRect16 = [](int rx, int ry, int rw, int rh, int r,
                            uint16_t color) {
    int x_left = rx + r;
    int x_right = rx + rw - r - 1;
    int y_top = ry + r;
    int y_bottom = ry + rh - r - 1;

    for (int y = ry; y < ry + rh; y++) {
      if (y < 0 || y >= 172)
        continue;
      for (int x = rx; x < rx + rw; x++) {
        if (x < 0 || x >= 320)
          continue;

        if (x < x_left && y < y_top) {
          int dx = x_left - x;
          int dy = y_top - y;
          if (dx * dx + dy * dy > r * r)
            continue;
        } else if (x > x_right && y < y_top) {
          int dx = x - x_right;
          int dy = y_top - y;
          if (dx * dx + dy * dy > r * r)
            continue;
        } else if (x < x_left && y > y_bottom) {
          int dx = x_left - x;
          int dy = y - y_bottom;
          if (dx * dx + dy * dy > r * r)
            continue;
        } else if (x > x_right && y > y_bottom) {
          int dx = x - x_right;
          int dy = y - y_bottom;
          if (dx * dx + dy * dy > r * r)
            continue;
        }

        gifCanvas[y * 320 + x] = color;
      }
    }
  };

  fillRoundRect16(leftEyeX, eyeY, eyeWidth, eyeHeight, cornerRadius,
                  COLOR_WHITE);
  fillRoundRect16(rightEyeX, eyeY, eyeWidth, eyeHeight, cornerRadius,
                  COLOR_WHITE);

  // Push full 320x172 screen buffer to TFT to prevent vertical clipping of eyes
  // when looking up/down
  tft.drawRGBBitmap(0, 0, gifCanvas, 320, 172);
}

enum GaugeFooterMode {
  FOOTER_PAGE_INDEX,
  FOOTER_BATTERY_TEMP
};

// ------------------------------------------------------------
// Theme 01 Light Bar — Threshold-Correct + Progressive Visual Remapping
// ------------------------------------------------------------
struct Theme01VisualBarConfig {
  uint16_t segColors[20];
  float segVals[20];
  float cachedMinVal;
  float cachedMaxVal;
  float cachedThG;
  float cachedThY;
  float cachedThO;
  bool cachedLowCrit;
  bool cachedTripEnergy;
  bool cachedStrictLess;
  bool initialized;
};

static void resolveTheme01VisualBar(
    float minVal, float maxVal,
    float thGreen, float thYellow, float thOrange,
    bool lowIsCritical, bool isTripEnergy, bool isYellowStrictLess,
    Theme01VisualBarConfig& cfg
) {
  cfg.cachedMinVal = minVal;
  cfg.cachedMaxVal = maxVal;
  cfg.cachedThG = thGreen;
  cfg.cachedThY = thYellow;
  cfg.cachedThO = thOrange;
  cfg.cachedLowCrit = lowIsCritical;
  cfg.cachedTripEnergy = isTripEnergy;
  cfg.cachedStrictLess = isYellowStrictLess;
  cfg.initialized = true;

  float range = maxVal - minVal;

  // Case 1: Trip Energy -> solid CYAN across all 20 segments
  if (isTripEnergy) {
    float step = (range > 0.0f) ? (range / 19.0f) : 0.0f;
    for (int i = 0; i < 20; i++) {
      cfg.segColors[i] = COLOR_CYAN;
      cfg.segVals[i] = minVal + (float)i * step;
    }
    return;
  }

  // Case 2: Zero threshold fallback (e.g. Charging Estimate) -> solid GREEN
  if (thGreen == 0.0f && thYellow == 0.0f && thOrange == 0.0f) {
    float step = (range > 0.0f) ? (range / 19.0f) : 0.0f;
    for (int i = 0; i < 20; i++) {
      cfg.segColors[i] = COLOR_GREEN;
      cfg.segVals[i] = minVal + (float)i * step;
    }
    return;
  }

  // Case 3: Invalid range guard
  if (range <= 0.0f) {
    for (int i = 0; i < 20; i++) {
      cfg.segColors[i] = COLOR_GREEN;
      cfg.segVals[i] = minVal;
    }
    return;
  }

  // Minimum visual widths: Green=5, Yellow=3, Orange=3, Red=3 (sum = 14)
  // Extra pool = 20 - 14 = 6
  float rawW[4];
  uint16_t zoneColor[4];
  int minW[4];

  if (!lowIsCritical) {
    // High-Critical: Zone 0: Green, Zone 1: Yellow, Zone 2: Orange, Zone 3: Red
    zoneColor[0] = COLOR_GREEN;
    zoneColor[1] = COLOR_YELLOW;
    zoneColor[2] = COLOR_ORANGE;
    zoneColor[3] = COLOR_RED;
    minW[0] = 5; minW[1] = 3; minW[2] = 3; minW[3] = 3;

    float gBound = constrain(thGreen, minVal, maxVal);
    float yBound = constrain(thYellow, minVal, maxVal);
    float oBound = constrain(thOrange, minVal, maxVal);

    rawW[0] = gBound - minVal;
    rawW[1] = max(0.0f, yBound - gBound);
    rawW[2] = max(0.0f, oBound - yBound);
    rawW[3] = max(0.0f, maxVal - oBound);
  } else {
    // Low-Critical: Zone 0: Red, Zone 1: Orange, Zone 2: Yellow, Zone 3: Green
    zoneColor[0] = COLOR_RED;
    zoneColor[1] = COLOR_ORANGE;
    zoneColor[2] = COLOR_YELLOW;
    zoneColor[3] = COLOR_GREEN;
    minW[0] = 3; minW[1] = 3; minW[2] = 3; minW[3] = 5;

    float oBound = constrain(thOrange, minVal, maxVal);
    float yBound = constrain(thYellow, minVal, maxVal);
    float gBound = constrain(thGreen, minVal, maxVal);

    rawW[0] = oBound - minVal;
    rawW[1] = max(0.0f, yBound - oBound);
    rawW[2] = max(0.0f, gBound - yBound);
    rawW[3] = max(0.0f, maxVal - gBound);
  }

  int w[4] = { minW[0], minW[1], minW[2], minW[3] };
  float totRaw = rawW[0] + rawW[1] + rawW[2] + rawW[3];

  if (totRaw > 0.0f) {
    float quota[4];
    int extraInt[4];
    float rem[4];
    int sumExtra = 0;

    for (int k = 0; k < 4; k++) {
      quota[k] = 6.0f * (rawW[k] / totRaw);
      extraInt[k] = (int)quota[k];
      rem[k] = quota[k] - (float)extraInt[k];
      sumExtra += extraInt[k];
    }

    int remainingPool = 6 - sumExtra;
    while (remainingPool > 0) {
      int bestK = -1;
      float maxRem = -1.0f;
      for (int k = 0; k < 4; k++) {
        if (rem[k] > maxRem) {
          maxRem = rem[k];
          bestK = k;
        }
      }
      if (bestK >= 0) {
        extraInt[bestK]++;
        rem[bestK] = -2.0f; // mark as used
        remainingPool--;
      } else {
        break;
      }
    }

    for (int k = 0; k < 4; k++) {
      w[k] += extraInt[k];
    }
  } else {
    w[0] = 5; w[1] = 5; w[2] = 5; w[3] = 5;
  }

  // Resolve boundaries: b[0] = 0, b[1], b[2], b[3], b[4] = 20
  int b[5];
  b[0] = 0;
  b[1] = b[0] + w[0];
  b[2] = b[1] + w[1];
  b[3] = b[2] + w[2];
  b[4] = 20;

  // Compute activation thresholds segVals and assign colors
  if (!lowIsCritical) {
    // High-Critical zone thresholds
    float v0 = minVal;
    float v1 = constrain(thGreen, minVal, maxVal);
    float v2 = constrain(thYellow, v1, maxVal);
    float v3 = constrain(thOrange, v2, maxVal);
    float v4 = maxVal;
    float v[5] = { v0, v1, v2, v3, v4 };

    for (int k = 0; k < 4; k++) {
      int wK = b[k + 1] - b[k];
      float vStart = v[k];
      float vEnd = v[k + 1];

      for (int j = 0; j < wK; j++) {
        int idx = b[k] + j;
        if (idx >= 0 && idx < 20) {
          cfg.segColors[idx] = zoneColor[k];
          if (k == 0) {
            // Green zone: segments 0..w[0]-1 span [minVal..thGreen]
            cfg.segVals[idx] = vStart + (float)j * ((vEnd - vStart) / (float)wK);
          } else if (k == 2 && isYellowStrictLess) {
            // Strict-less yellow (e.g. RPM): Orange begins at exactly thYellow (vStart)
            float step = (vEnd - vStart) / (float)wK;
            cfg.segVals[idx] = vStart + (float)j * step;
          } else {
            // Non-Green zones: first segment activates at the smallest representable float
            // strictly above vStart, preserving the existing linear activation distribution
            // within the zone with the boundary moved from the synthetic +0.01f offset
            // to the correct IEEE 754 epsilon above vStart.
            float zoneStart = std::nextafterf(vStart, vStart + 1.0f);
            float zoneWidth = vEnd - zoneStart;
            if (zoneWidth <= 0.0f) zoneWidth = 0.0f;
            float step = (wK > 1) ? (zoneWidth / (float)wK) : 0.0f;
            cfg.segVals[idx] = zoneStart + (float)j * step;
          }
        }
      }
    }
  } else {
    // Low-Critical zone thresholds: [minVal, thOrange, thYellow, thGreen, maxVal]
    float v0 = minVal;
    float v1 = constrain(thOrange, minVal, maxVal);
    float v2 = constrain(thYellow, v1, maxVal);
    float v3 = constrain(thGreen, v2, maxVal);
    float v4 = maxVal;
    float v[5] = { v0, v1, v2, v3, v4 };

    for (int k = 0; k < 4; k++) {
      int wK = b[k + 1] - b[k];
      float vStart = v[k];
      float vEnd = v[k + 1];

      for (int j = 0; j < wK; j++) {
        int idx = b[k] + j;
        if (idx >= 0 && idx < 20) {
          cfg.segColors[idx] = zoneColor[k];
          float step = (vEnd - vStart) / (float)wK;
          cfg.segVals[idx] = vStart + (float)j * step;
        }
      }
    }
  }
}

void drawDataPage_T01_Unified(
  GaugeFooterMode footerMode,
  const String& footerText,
  String labelText,
  String valStr,
  float rawVal,
  float minVal,
  float maxVal,
  uint16_t containerColor,
  uint16_t txtColor,
  uint16_t dotColor,
  bool lowIsCritical = false,
  bool isTripEnergy = false,
  float thGreen = 0.0f,
  float thYellow = 0.0f,
  float thOrange = 0.0f,
  bool isYellowStrictLess = false
) {
  static GaugeFooterMode prevFooterMode = FOOTER_PAGE_INDEX;
  static String prevFooterText = "";
  static String prevLabelText = "";
  static String prevValStr = "";
  static float prevRawVal = -999.0f;
  static uint16_t prevDotColor = 0;
  static float prevBattTemp = -999.0f;
  static int prevState = -1;
  static ObdHealthState previousHealthState        = OBD_HEALTH_DISCONNECTED;
  static bool           previousBlinkVisible       = true;
  static bool           previousPreConnection      = true;
  static bool           healthIndicatorInitialized = false;
  static StaState       prevStaState               = STA_STATE_IDLE;
  static wl_status_t    prevWifiStatus             = WL_IDLE_STATUS;

  StaState currentSta = currentStaState;
  wl_status_t currentWifi = WiFi.status();

  bool pageChanged = themeChanged || 
                     (footerMode != prevFooterMode) || 
                     (footerText != prevFooterText) || 
                     (labelText != prevLabelText);
  bool stateChanged = (currentState != prevState);
  bool valChanged = (valStr != prevValStr);
  bool rawValChanged = (rawVal != prevRawVal);
  bool tempChanged = (batt_temp != prevBattTemp);
  bool isPreConn = isObdPreConnectionPresentation(currentWifi);
  bool preConnChanged = (isPreConn != previousPreConnection);
  bool blinkVisible = isPreConn ? false : getObdHealthBlinkVisible();
  bool healthStateChanged = !healthIndicatorInitialized || (obdHealthState != previousHealthState) || preConnChanged;
  bool indicatorAnimationChanged = (blinkVisible != previousBlinkVisible);
  uint16_t indicatorColor = isPreConn ? COLOR_DARKGREY : getObdHealthIndicatorColor();
  bool staStateChanged = (currentSta != prevStaState);
  bool wifiStatusChanged = (currentWifi != prevWifiStatus);
  bool statusTextChanged = (currentState != STATE_ACTIVE && (staStateChanged || wifiStatusChanged));

  // Presentation-string sanity guard (INV-VALID-04):
  // Ensure valStr contains genuine numeric telemetry (leading whitespace stripped, optional sign, followed by digit or .digit)
  // Rejects placeholders: "--", "--MIN", "NO CHG", "CALC", "WAIT", "DONE", "N/A"
  // Pure display guard — does NOT modify parser or raw data semantics
  bool isNumericVal = false;
  {
    String s = valStr;
    s.trim();
    if (s.length() > 0) {
      int idx = 0;
      if (s[0] == '+' || s[0] == '-') {
        idx++;
      }
      if (idx < s.length()) {
        if (isdigit(s[idx])) {
          isNumericVal = true;
        } else if (s[idx] == '.' && (idx + 1 < s.length()) && isdigit(s[idx + 1])) {
          isNumericVal = true;
        }
      }
    }
  }

  // Data validity check (INV-VALID-03): Active state + Fresh OBD telemetry + Verified Numeric string
  // Decoupled strictly from dotColor per INV-VALID-03
  bool isMetricValid = (currentState == STATE_ACTIVE) &&
                       isObdDataFresh() &&
                       isNumericVal;

  static bool previousMetricValid = false;
  bool metricValidityChanged = (isMetricValid != previousMetricValid);

  // Single-capture Metric Severity & Blink resolution (synchronized Circle & Bar)
  static bool previousMetricBlinkVisible = true;
  static uint16_t prevCircleRenderColor  = 0xFFFF;

  uint32_t now = millis(); // Captured globally once per frame
  bool metricBlinkVisible = true;
  uint16_t metricSeverityColor = COLOR_DARKGREY;
  uint16_t currentCircleRenderColor = COLOR_DARKGREY;

  if (!isMetricValid) {
    // 1. INVALID PATH (Priority 1: Overrides Trip Energy and all severity)
    metricSeverityColor = COLOR_DARKGREY;
    metricBlinkVisible = true; // Solid Darkgrey (Blink = OFF)
    currentCircleRenderColor = COLOR_DARKGREY;
  } else if (isTripEnergy) {
    // 2. TRIP ENERGY PATH (Priority 2: Explicit Informational, Non-severity)
    metricSeverityColor = COLOR_CYAN;
    metricBlinkVisible = true; // Solid Cyan (Blink = OFF)
    currentCircleRenderColor = COLOR_CYAN;
  } else {
    // 3. NORMAL SEVERITY PATH (Priority 3: dotColor Single Source of Truth)
    metricSeverityColor = dotColor;

    // Absolute millis timing ensures immune blink phase
    if (dotColor == COLOR_GREEN || dotColor == COLOR_YELLOW) {
      metricBlinkVisible = true; // Solid (Blink = OFF)
    } else if (dotColor == COLOR_ORANGE) {
      metricBlinkVisible = ((now / 350) % 2) == 0; // Blink 350 ms
    } else if (dotColor == COLOR_RED) {
      metricBlinkVisible = ((now / 200) % 2) == 0; // Blink 200 ms (Fastest)
    } else {
      metricBlinkVisible = true;
    }

    currentCircleRenderColor = metricBlinkVisible ? metricSeverityColor : COLOR_DARKGREY;
  }

  bool metricBlinkChanged = (metricBlinkVisible != previousMetricBlinkVisible);

  if (pageChanged || stateChanged) {
    tft.fillScreen(COLOR_BLACK);
  }

  int barStartX = 11;
  int barStartY = 12;
  int segWidth = 14;
  int segHeight = 16;
  int segSpacing = 1;
  int numSegments = 20;

  static int scanX = 0;
  static int scanDir = 1;
  static unsigned long lastScan = 0;
  bool scanChanged = false;

  if (currentState != STATE_ACTIVE) {
    if (millis() - lastScan > 50) {
      lastScan = millis();
      scanX += scanDir;
      if (scanX >= numSegments - 1 || scanX <= 0)
        scanDir = -scanDir;
      scanChanged = true;
    }
  }

  bool isDiscoveryActive = isPreConn || (currentState != STATE_ACTIVE);

  // Cached Theme 01 visual bar configuration (recomputed only on page/config change)
  static Theme01VisualBarConfig visualBarConfig = {
    {}, {}, -999.0f, -999.0f, -999.0f, -999.0f, -999.0f, false, false, false, false
  };

  bool configChanged = !visualBarConfig.initialized ||
                       (minVal != visualBarConfig.cachedMinVal) ||
                       (maxVal != visualBarConfig.cachedMaxVal) ||
                       (thGreen != visualBarConfig.cachedThG) ||
                       (thYellow != visualBarConfig.cachedThY) ||
                       (thOrange != visualBarConfig.cachedThO) ||
                       (lowIsCritical != visualBarConfig.cachedLowCrit) ||
                       (isTripEnergy != visualBarConfig.cachedTripEnergy) ||
                       (isYellowStrictLess != visualBarConfig.cachedStrictLess);

  if (pageChanged || configChanged) {
    resolveTheme01VisualBar(minVal, maxVal, thGreen, thYellow, thOrange,
                            lowIsCritical, isTripEnergy, isYellowStrictLess,
                            visualBarConfig);
  }

  // Redraw trigger: rawValChanged triggers immediate fill update; blink changes do NOT redraw bar
  bool barNeedRedraw = pageChanged || configChanged || stateChanged || rawValChanged ||
                       metricValidityChanged ||
                       (isDiscoveryActive && scanChanged);

  // Pixel Art Segmented Bar (Theme 01)
  if (barNeedRedraw) {
    if (isMetricValid) {
      // Priority 1: Segmented visual rendering (Progressive Threshold-Aware Remapping)
      for (int i = 0; i < numSegments; i++) {
        int x = barStartX + (i * (segWidth + segSpacing));
        bool isLit = (rawVal >= visualBarConfig.segVals[i]);
        uint16_t segmentColor = visualBarConfig.segColors[i];

        tft.fillRect(x, barStartY, segWidth, segHeight,
                     isLit ? segmentColor : COLOR_DARKGREY);
      }
    } else if (isDiscoveryActive) {
      // Priority 2: Wi-Fi discovery sweep animation
      uint16_t sweepColor = isPreConn ? COLOR_GREEN : getObdHealthColor();
      for (int i = 0; i < numSegments; i++) {
        int x = barStartX + (i * (segWidth + segSpacing));
        tft.fillRect(x, barStartY, segWidth, segHeight,
                     (abs(i - scanX) <= 1) ? sweepColor : COLOR_DARKGREY);
      }
    } else {
      // Priority 3: Active state but metric not valid yet: all segments dark grey
      for (int i = 0; i < numSegments; i++) {
        int x = barStartX + (i * (segWidth + segSpacing));
        tft.fillRect(x, barStartY, segWidth, segHeight, COLOR_DARKGREY);
      }
    }
  }

  if (pageChanged || stateChanged || valChanged || statusTextChanged) {
    tft.fillRect(0, 34, 320, 96, COLOR_BLACK);
    if (currentState == STATE_ACTIVE) {
      String numPart = valStr;
      String unitPart = "";
      numPart.trim();

      // Parse unit out of the string
      if (numPart.endsWith(" km/h")) {
        unitPart = "km/h";
        numPart = numPart.substring(0, numPart.length() - 5);
      } else if (numPart.endsWith("km/h")) {
        unitPart = "km/h";
        numPart = numPart.substring(0, numPart.length() - 4);
      } else if (numPart.endsWith(" V") || numPart.endsWith(" C") ||
                 numPart.endsWith(" R") || numPart.endsWith(" K") ||
                 numPart.endsWith(" %") || numPart.endsWith("MIN") || 
                 numPart.endsWith("h") || numPart.endsWith("m") ||
                 numPart.endsWith("h ")) {
        if (numPart.endsWith("MIN")) {
          unitPart = "MIN";
          numPart = numPart.substring(0, numPart.length() - 3);
        } else if (numPart.endsWith("m")) {
          if (numPart.indexOf(' ') != -1) {
            // It has space, e.g. "1h 25m". Keep as numPart, no unitPart
            unitPart = "";
          } else {
            // e.g. "25m". Let's extract "m" as unit!
            unitPart = "m";
            numPart = numPart.substring(0, numPart.length() - 1);
          }
        } else {
          unitPart = numPart.substring(numPart.length() - 1);
          numPart = numPart.substring(0, numPart.length() - 2);
        }
      } else if (numPart.endsWith("V") || numPart.endsWith("C") ||
                 numPart.endsWith("R") || numPart.endsWith("K") ||
                 numPart.endsWith("%")) {
        unitPart = numPart.substring(numPart.length() - 1);
        numPart = numPart.substring(0, numPart.length() - 1);
      }
      numPart.trim();
      unitPart.trim();

      int numLen = numPart.length();
      int unitLen = unitPart.length();
      uint8_t textSz = 7;
      int charWidth = 42;

      if (numLen + unitLen >= 9) {
        textSz = 5;
        charWidth = 30;
      }

      int numWidth = numLen * charWidth - 4;
      int unitWidth = 0;
      int gap = 0;

      if (unitLen > 0) {
        gap = 6;
        float unitScale = 0.6;
        if (unitPart == "km/h") {
          unitScale = 0.45;
        } else if (unitPart == "MIN") {
          unitScale = 0.5;
        } else if (unitPart == "m") {
          unitScale = 0.6;
        }
        unitWidth = unitLen * (charWidth * unitScale) - 2;
      }

      int totalWidth = numWidth + gap + unitWidth;
      int xPos = (320 - totalWidth) / 2;
      if (xPos < 0)
        xPos = 0;

      int numHeight = 8 * textSz;
      int yPos = 34 + (96 - numHeight) / 2;

      // Draw Number
      tft.setTextSize(textSz);
      tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
      tft.setCursor(xPos, yPos);
      tft.print(numPart);
      tft.setCursor(xPos + 1, yPos);
      tft.print(numPart);

      // Draw Unit aligned to baseline
      if (unitLen > 0) {
        uint8_t unitSz = textSz - 2;
        if (unitPart == "km/h") {
          unitSz = textSz - 3;
        } else if (unitPart == "MIN") {
          unitSz = textSz - 3;
        } else if (unitPart == "m") {
          unitSz = textSz - 2;
        }
        if (unitSz < 3)
          unitSz = 3;
        int unitHeight = 8 * unitSz;
        int unitY = yPos + numHeight - unitHeight;

        tft.setTextSize(unitSz);
        tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
        tft.setCursor(xPos + numWidth + gap, unitY);
        tft.print(unitPart);
        tft.setCursor(xPos + numWidth + gap + 1, unitY);
        tft.print(unitPart);
      }
    } else {
      tft.setTextSize(4);
      tft.setTextColor(COLOR_GREY, COLOR_BLACK);

      String statusText;
      if (currentWifi == WL_CONNECTED) {
        statusText = "PORT CONN";
      } else if (!wifiEverConnected ||
                 currentSta == STA_STATE_SCANNING ||
                 currentSta == STA_STATE_WAIT_FOR_ADAPTER) {
        statusText = "SCAN WIFI";
      } else if (currentSta == STA_STATE_CONNECTING ||
                 currentSta == STA_STATE_BACKOFF) {
        statusText = "WIFI CONN";
      } else {
        // Fail-safe (INV-TEXT-10): never expose WIFI CONN outside
        // an explicit in-session reconnect state.
        statusText = "SCAN WIFI";
      }

      // [DIAGNOSTIC-A1] Transition log: fires on every STA state change (no rate limit)
      {
        static StaState _lastLoggedSta = STA_STATE_IDLE;
        if (currentSta != _lastLoggedSta) {
          Serial.printf(
            "[UI][WIFI_STATE] %d -> %d\n",
            (int)_lastLoggedSta,
            (int)currentSta
          );
          _lastLoggedSta = currentSta;
        }
      }

      // [DIAGNOSTIC-A2] Snapshot log: full status every 500ms
      {
        static uint32_t _lastUiWifiLog = 0;
        if (millis() - _lastUiWifiLog > 500) {
          _lastUiWifiLog = millis();
          Serial.printf(
            "[UI][WIFI_TEXT] currentSta=%d prevSta=%d currentWifi=%d everConn=%d text=%s\n",
            (int)currentSta, (int)prevStaState, (int)currentWifi,
            (int)wifiEverConnected, statusText.c_str()
          );
        }
      }

      int xPos = (320 - (statusText.length() * 24 - 2)) / 2;
      tft.setCursor(xPos, 34 + 30);
      tft.print(statusText);
    }
  }

  // A. Static / Text region: redraws ONLY when text, page, state, or battery temp changes.
  // Never triggered by dotChanged, healthStateChanged, or indicatorAnimationChanged.
  // Clears only up to x = 276, leaving the dot area (x = 282..306) completely untouched.
  if (pageChanged || stateChanged || tempChanged) {
    tft.fillRect(0, 134, 276, 38, COLOR_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
    tft.setCursor(10, 148);

    if (footerMode == FOOTER_PAGE_INDEX) {
      tft.print(footerText);
    } else {
      // FOOTER_BATTERY_TEMP: Draw battery temp instead of page index
      char tempBuf[12];
      if (currentState == STATE_ACTIVE && batt_temp > -40.0f && batt_temp < 100.0f) {
        sprintf(tempBuf, "%.0f\xF7""C", batt_temp);
      } else {
        sprintf(tempBuf, "--\xF7""C");
      }
      tft.print(tempBuf);
    }

    tft.fillRect(52, 136, 216, 28, containerColor);
    tft.setTextColor(txtColor);
    int labelLen = labelText.length();
    int labelXPos = 52 + (216 - (labelLen * 12 - 2)) / 2;
    tft.setCursor(labelXPos, 143);
    tft.print(labelText);
  }

  // B. Status Indicator region: isolated dot rendering
  bool circleChanged = (currentCircleRenderColor != prevCircleRenderColor) ||
                       metricValidityChanged ||
                       (dotColor != prevDotColor);

  // Redraws ONLY on presentation changes — NEVER triggered by healthStateChanged!
  if (pageChanged || stateChanged || circleChanged || metricBlinkChanged) {
    tft.fillCircle(294, 150, 12, COLOR_BLACK);
    tft.fillCircle(294, 150, 12, currentCircleRenderColor);
  }

  // Update isolated state tracking
  previousMetricBlinkVisible = metricBlinkVisible;
  prevCircleRenderColor      = currentCircleRenderColor;
  previousMetricValid        = isMetricValid;

  prevFooterMode = footerMode;
  prevFooterText = footerText;
  prevLabelText = labelText;
  prevValStr = valStr;
  prevRawVal = rawVal;
  prevDotColor = dotColor;
  prevBattTemp = batt_temp;
  prevState = currentState;
  previousHealthState        = obdHealthState;
  previousBlinkVisible       = blinkVisible;
  previousPreConnection      = isPreConn;
  prevStaState               = currentSta;
  prevWifiStatus             = currentWifi;
  healthIndicatorInitialized = true;
  if (themeChanged)
    themeChanged = false;
}

void drawDataPage_T01(int pageNum, String labelText, String valStr,
                      float rawVal, float minVal, float maxVal,
                      uint16_t containerColor, uint16_t txtColor,
                      uint16_t dotColor, bool lowIsCritical = false,
                      bool isTripEnergy = false,
                      float thGreen = 0.0f,
                      float thYellow = 0.0f,
                      float thOrange = 0.0f,
                      bool isYellowStrictLess = false) {
  char pageIndexStr[8];
  sprintf(pageIndexStr, "%d/6", pageNum - 1);
  drawDataPage_T01_Unified(FOOTER_PAGE_INDEX, pageIndexStr, labelText, valStr,
                           rawVal, minVal, maxVal, containerColor, txtColor,
                           dotColor, lowIsCritical, isTripEnergy,
                           thGreen, thYellow, thOrange, isYellowStrictLess);
}

void drawGaugeUI_T01() {
  if (currentPage < 2 || currentPage > 7)
    return;
  int slotIdx = currentPage - 2;
  int funcId = config_slots[slotIdx];

  // Lock colors to page index (slotIdx) rather than function ID
  static const uint16_t slotMainColors[6] = {COLOR_RED,    COLOR_NAVY,
                                             COLOR_YELLOW, COLOR_ORANGE,
                                             COLOR_CYAN,   COLOR_GREEN};
  static const uint16_t slotBgColors[6] = {COLOR_WHITE, COLOR_WHITE,
                                           COLOR_BLACK, COLOR_BLACK,
                                           COLOR_BLACK, COLOR_BLACK};

  uint16_t mainColor = slotMainColors[slotIdx];
  uint16_t bgColor = slotBgColors[slotIdx];

  switch (funcId) {
  case 0: { // 12V Battery
    float thG = 13.0f, thY = 12.7f, thO = 12.5f;
    uint16_t dotColor;
    if (batt12v >= thG)
      dotColor = COLOR_GREEN;
    else if (batt12v >= thY)
      dotColor = COLOR_YELLOW;
    else if (batt12v >= thO)
      dotColor = COLOR_ORANGE;
    else
      dotColor = COLOR_RED;
    char valBuf[16];
    sprintf(valBuf, "%.2fV", batt12v);
    drawDataPage_T01(currentPage, "12V BATTERY", valBuf, batt12v, 11.0, 15.0,
                     mainColor, bgColor, dotColor, true, false, thG, thY, thO, false);
    break;
  }
  case 1: { // Battery Temp
    float thG = 38.0f, thY = 41.0f, thO = 45.0f;
    uint16_t dotColor;
    if (batt_temp <= thG)
      dotColor = COLOR_GREEN;
    else if (batt_temp <= thY)
      dotColor = COLOR_YELLOW;
    else if (batt_temp <= thO)
      dotColor = COLOR_ORANGE;
    else
      dotColor = COLOR_RED;
    char valBuf[16];
    sprintf(valBuf, "%.1fC", batt_temp);
    drawDataPage_T01(currentPage, "BATTERY TEMP", valBuf, batt_temp, 10.0, 50.0,
                     mainColor, bgColor, dotColor, false, false, thG, thY, thO, false);
    break;
  }
  case 2: { // Motor RPM
    // Round to nearest 10 RPM to reduce flicker
    int rpmDisplay = (int)displayRpm;
    rpmDisplay = ((rpmDisplay + 5) / 10) * 10;

    static int lastDisplayedRpm = 0;
    static unsigned long lastRpmTextDrawMs = 0;
    if (millis() - lastRpmTextDrawMs > 200 || lastRpmTextDrawMs == 0) {
      lastDisplayedRpm = rpmDisplay;
      lastRpmTextDrawMs = millis();
    }
    int rpmToShow = lastDisplayedRpm;
    float thG = 6000.0f, thY = 9000.0f, thO = 13000.0f;
    uint16_t dotColor;
    if (rpmToShow <= (int)thG)
      dotColor = COLOR_GREEN;
    else if (rpmToShow < (int)thY)
      dotColor = COLOR_YELLOW;
    else if (rpmToShow <= (int)thO)
      dotColor = COLOR_ORANGE;
    else
      dotColor = COLOR_RED;
     char valBuf[16];
     sprintf(valBuf, "%d", rpmToShow);

     // RPM source label based on rpmSourceMode
     String label;
     if (rpmSourceMode == 2) {
       label = mmcuRpmValid ? "MOTOR RPM (MMCU)" : "MOTOR RPM (N/A)";
     } else if (rpmSourceMode == 1) {
       label = "MOTOR RPM (EST)";
     } else {
       // AUTO mode: show actual source
       bool mmcuRecent = (mmcuLastUpdateMs > 0 && millis() - mmcuLastUpdateMs < 3000);
       label = (mmcuRpmValid && mmcuRecent) ? "MOTOR RPM (ACT)" : "MOTOR RPM (EST)";
     }

    drawDataPage_T01(currentPage, label, valBuf, (float)rpmToShow, 0.0,
                     10000.0, mainColor, bgColor, dotColor, false, false, thG, thY, thO, /*isYellowStrictLess=*/true);
    break;
  }
  case 3: { // Motor Temp
    float thG = 85.0f, thY = 95.0f, thO = 105.0f;
    uint16_t dotColor;
    if (motor_temp <= thG)
      dotColor = COLOR_GREEN;
    else if (motor_temp <= thY)
      dotColor = COLOR_YELLOW;
    else if (motor_temp <= thO)
      dotColor = COLOR_ORANGE;
    else
      dotColor = COLOR_RED;
    char valBuf[16];
    sprintf(valBuf, "%.1fC", motor_temp);
    drawDataPage_T01(currentPage, "MOTOR TEMP", valBuf, motor_temp, 30.0, 120.0,
                     mainColor, bgColor, dotColor, false, false, thG, thY, thO, false);
    break;
  }
  case 4: { // Battery SOH
    float thG = 85.0f, thY = 80.0f, thO = 70.0f;
    uint16_t dotColor;
    if (hv_soh >= thG)
      dotColor = COLOR_GREEN;
    else if (hv_soh >= thY)
      dotColor = COLOR_YELLOW;
    else if (hv_soh >= thO)
      dotColor = COLOR_ORANGE;
    else
      dotColor = COLOR_RED;
    char valBuf[16];
    float soh_truncated = (int)(hv_soh * 100) / 100.0;
    sprintf(valBuf, "%.2f%%", soh_truncated);
    drawDataPage_T01(currentPage, "BATTERY SOH", valBuf, hv_soh, 60.0, 100.0,
                     mainColor, bgColor, dotColor, true, false, thG, thY, thO, false);
    break;
  }
  case 5: { // Trip Energy (net discharge this session)
    uint16_t dotColor = COLOR_CYAN;
    char valBuf[16];
    
    Serial.printf("[DISPLAY] Trip: %.3f kWh (discharge=%.3f, regen=%.3f) valid=%d src=%s\n",
                  tripKwhUsed, tripRegenKwh, discharge_kwh, energyValid, energySourceLabel.c_str());

    if (currentState != STATE_ACTIVE || !energyValid) {
      sprintf(valBuf, "--");
    } else {
      float display_val = tripKwhUsed;
      if (display_val < 1.0f)
        sprintf(valBuf, "%.3f", display_val);
      else if (display_val < 10.0f)
        sprintf(valBuf, "%.2f", display_val);
      else
        sprintf(valBuf, "%.1f", display_val); // Use 1 decimal for values >= 10.0f
    }

    drawDataPage_T01(currentPage, "TRIP kWh (DIS)", valBuf, tripKwhUsed, 0.0f, 70.0f,
                     mainColor, bgColor, dotColor, false, /*isTripEnergy=*/true);
    break;
  }
  case 6: { // Battery SOC
    float thG = 40.0f, thY = 25.0f, thO = 10.0f;
    uint16_t dotColor = COLOR_DARKGREY; // Clean architecture default: NO DATA
    char valBuf[16];
    if (!socFinalValid || batt_soc < 0.0f) {
        // No valid BMS SOC — show invalid marker
        // Data-validity state only — not a UI redesign
        strncpy(valBuf, "--", sizeof(valBuf));
        drawDataPage_T01(currentPage, "BATTERY SOC", valBuf, 0.0f, 0.0, 100.0,
                         mainColor, bgColor, dotColor, true, false, thG, thY, thO, false);
    } else {
        if (batt_soc >= thG)
            dotColor = COLOR_GREEN;
        else if (batt_soc >= thY)
            dotColor = COLOR_YELLOW;
        else if (batt_soc >= thO)
            dotColor = COLOR_ORANGE;
        else
            dotColor = COLOR_RED;
        // 2 decimal places (VP_PID_SOC = 1% real resolution, float for future source)
        // Example: BMS=54 → stored=54.00f → displayed="54.00%"
        sprintf(valBuf, "%.2f%%", batt_soc);
        drawDataPage_T01(currentPage, "BATTERY SOC", valBuf, batt_soc, 0.0, 100.0,
                         mainColor, bgColor, dotColor, true, false, thG, thY, thO, false);
    }
    break;
  }
  case 7: { // Vehicle Speed
    float thG = 110.0f, thY = 130.0f, thO = 150.0f;
    uint16_t dotColor;
    int spdDisplay = (int)displaySpeedKmh;
    if (spdDisplay <= (int)thG)
      dotColor = COLOR_GREEN;
    else if (spdDisplay <= (int)thY)
      dotColor = COLOR_YELLOW;
    else if (spdDisplay <= (int)thO)
      dotColor = COLOR_ORANGE;
    else
      dotColor = COLOR_RED;
    char valBuf[16];
    sprintf(valBuf, "%d km/h", spdDisplay);
    drawDataPage_T01(currentPage, "SPEED", valBuf, (float)spdDisplay, 0.0,
                     180.0, mainColor, bgColor, dotColor, false, false, thG, thY, thO, false);
    break;
  }
  }
}

// ============================================================
// Time-based Display Smoothing (EMA with dt)
// ============================================================
// Called once per frame before drawing. Updates displaySpeedKmh
// and displayRpm from raw targets (vehicle_speed, motor_rpm).
// These smoothed values are ONLY for TFT rendering.
void updateDisplaySmoothing() {
  unsigned long now = millis();
  if (lastSmoothTime == 0) {
    lastSmoothTime = now;
    displaySpeedKmh = displayTargetSpeed;
    displayRpm = displayTargetRpm;
    return;
  }

  float dt = (now - lastSmoothTime) / 1000.0f;
  lastSmoothTime = now;

  // Clamp dt to avoid instability on long pauses or reconnect delays
  dt = constrain(dt, 0.01f, 0.10f);
  float dtScale = dt * 20.0f; // Normalized to 20 fps

  float targetSpeed = displayTargetSpeed;
  float targetRpm   = displayTargetRpm;

  // Snap to 0 when car is stopped
  if (targetSpeed < 1.0f) {
    displaySpeedKmh = 0.0f;
    displayRpm = 0.0f;
    return;
  }

  // Speed adaptive display smoothing
  float diffSpeed = fabsf(targetSpeed - displaySpeedKmh);
  float baseAlphaSpeed = 0.20f;
  if (diffSpeed > 30.0f) {
    baseAlphaSpeed = 0.75f;
  } else if (diffSpeed > 15.0f) {
    baseAlphaSpeed = 0.55f;
  } else if (diffSpeed > 5.0f) {
    baseAlphaSpeed = 0.35f;
  }
  
  float alphaSpeed = baseAlphaSpeed * dtScale;
  alphaSpeed = constrain(alphaSpeed, 0.10f, 0.90f);

  // Snap on large jumps (reconnect / data loss recovery)
  if (diffSpeed > 80.0f) {
    displaySpeedKmh = targetSpeed;
  } else {
    displaySpeedKmh += (targetSpeed - displaySpeedKmh) * alphaSpeed;
  }

  // RPM adaptive display smoothing
  float diffRpm = fabsf(targetRpm - displayRpm);
  float baseAlphaRpm = 0.12f;
  if (diffRpm > 2000.0f) {
    baseAlphaRpm = 0.55f;
  } else if (diffRpm > 1000.0f) {
    baseAlphaRpm = 0.40f;
  } else if (diffRpm > 300.0f) {
    baseAlphaRpm = 0.25f;
  }

  float alphaRpm = baseAlphaRpm * dtScale;
  alphaRpm = constrain(alphaRpm, 0.10f, 0.90f);

  if (diffRpm > 5000.0f) {
    displayRpm = targetRpm;
  } else {
    displayRpm += (targetRpm - displayRpm) * alphaRpm;
  }
}

// ============================================================
// Charging Estimate Page Layout Renderer
// ============================================================
void drawDataPage_T01_Charging(String labelText, String valStr,
                               float rawVal, float minVal, float maxVal,
                               uint16_t containerColor, uint16_t txtColor,
                               uint16_t dotColor, bool lowIsCritical = false,
                               String pageIndicatorStr = "") {
  // Legacy wrapper routing to unified core with FOOTER_BATTERY_TEMP
  drawDataPage_T01_Unified(FOOTER_BATTERY_TEMP, "", labelText, valStr,
                           rawVal, minVal, maxVal, containerColor, txtColor,
                           dotColor, lowIsCritical);
}

void drawChargeEstimateAutoBack() {
  String valStr = "--";
  uint16_t dotColor = COLOR_RED;

  if (chargeEtaStatusStr == "WAIT_SOC") {
    valStr = "--";
    dotColor = COLOR_RED;
  } else if (chargeEtaStatusStr == "NO_CHG") {
    valStr = "NO CHG";
    dotColor = COLOR_ORANGE;
  } else if (chargeEtaStatusStr == "CALC") {
    valStr = "CALC";
    dotColor = COLOR_ORANGE;
  } else if (chargeEtaStatusStr == "DONE") {
    valStr = "DONE";
    dotColor = COLOR_GREEN;
  } else if (chargeEtaStatusStr == "VALID" && std::isfinite(chargeEtaMinutes) && chargeEtaMinutes >= 0.0f) {
    int totalMins = (int)lroundf(chargeEtaMinutes);
    char timeBuf[16];
    if (totalMins >= 1000) {
      sprintf(timeBuf, " %d min", totalMins);
    } else {
      sprintf(timeBuf, "%d min", totalMins);
    }
    valStr = timeBuf;
    dotColor = COLOR_GREEN;
  }

  char labelBuf[32];
  sprintf(labelBuf, "TIME TO %d%% EST", chargeTargetSoc);

  // Call the unified drawer explicitly with FOOTER_BATTERY_TEMP
  drawDataPage_T01_Unified(FOOTER_BATTERY_TEMP, "", labelBuf, valStr, batt_soc, 0.0f, 100.0f,
                           COLOR_CYAN, COLOR_BLACK, dotColor, false);
}

// ============================================================
// AutoBack Stopwatch Page Renderer (HH:MM:SS format, size 6 font, centered)
// ============================================================
void drawAutoBackStopwatch() {
  static String lastStopwatchStr = "";
  if (themeChanged) {
    tft.fillScreen(COLOR_BLACK);
    lastStopwatchStr = "";
    themeChanged = false;
  }

  uint32_t elapsedMs = millis() - autoBackStopwatchStartMs;
  uint32_t elapsedSecs = elapsedMs / 1000;
  uint32_t h = elapsedSecs / 3600;
  uint32_t m = (elapsedSecs % 3600) / 60;
  uint32_t s = elapsedSecs % 60;

  char buf[12];
  sprintf(buf, "%02d:%02d:%02d", h, m, s);
  String curStr(buf);

  if (curStr != lastStopwatchStr) {
    tft.setTextSize(6);
    tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
    int x = 19;
    int y = 62;
    tft.setCursor(x, y);
    tft.print(curStr);
    lastStopwatchStr = curStr;
  }
}

// ============================================================
// Main gauge UI dispatcher — routes to active theme
// ============================================================
#if ENABLE_CHG_DEBUG_PAGE
void drawChgDebugPage() {
  tft.setTextColor(COLOR_CYAN, COLOR_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 8);
  tft.print("CHG DBG");

  tft.setCursor(160, 8);
  tft.print("ENERGY");

  tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
  char buf[32];

  // Left Column: SOC Telemetry
  // 1. SOC RAW (ตัวเลขจริง เช่น SOC 68.00)
  tft.setCursor(10, 30);
  if (socCoarseValid && socBmsCoarse >= 0.0f) {
    snprintf(buf, sizeof(buf), "SOC  %.2f", socBmsCoarse);
  } else {
    snprintf(buf, sizeof(buf), "SOC  --   ");
  }
  tft.print(buf);

  // 2. S-FINAL
  tft.setCursor(10, 50);
  snprintf(buf, sizeof(buf), "FINL %s", socFinalValid ? "VALID  " : "INVALID");
  tft.print(buf);

  // 3. SRC
  tft.setCursor(10, 70);
  const char* srcStr = "NONE  ";
  if (socSource == SOC_SOURCE_BMS_COARSE) {
    srcStr = VP_REQ_SOC;
  } else if (socSource == SOC_SOURCE_BMS_DETAILED) {
    srcStr = VP_REQ_SOH;
  }
  snprintf(buf, sizeof(buf), "SRC  %s", srcStr);
  tft.print(buf);

  // 4. AGE
  tft.setCursor(10, 90);
  if (socCoarseLastUpdateMs == 0) {
    snprintf(buf, sizeof(buf), "AGE  --   ");
  } else {
    uint32_t ageMs = millis() - socCoarseLastUpdateMs;
    snprintf(buf, sizeof(buf), "AGE  %lums  ", (unsigned long)ageMs);
  }
  tft.print(buf);

  // 5. VP_PID_SOC observation
  tft.setCursor(10, 110);
  snprintf(buf, sizeof(buf), "2205 %s", (socCoarseLastUpdateMs > 0) ? "SEEN" : "NONE");
  tft.print(buf);

  // 6. SESSION (Invariant 5: Runtime Diagnostic SES)
  tft.setCursor(10, 130);
  snprintf(buf, sizeof(buf), "SES  %s", energySessionStarted ? "YES  " : "NO   ");
  tft.print(buf);

  // Right Column: Energy Integration Telemetry (Read-Only)
  // 1. HV V
  tft.setCursor(160, 30);
  if (hvVoltageValid && hvBatteryVoltage >= 200.0f) {
    snprintf(buf, sizeof(buf), "HV V %.1f ", hvBatteryVoltage);
  } else {
    snprintf(buf, sizeof(buf), "HV V --    ");
  }
  tft.print(buf);

  // 2. HV A
  tft.setCursor(160, 50);
  if (hvCurrentValid) {
    snprintf(buf, sizeof(buf), "HV A %+0.2f", hvBatteryCurrent);
  } else {
    snprintf(buf, sizeof(buf), "HV A --    ");
  }
  tft.print(buf);

  // 3. READY
  tft.setCursor(160, 70);
  bool ready = isHvPowerReady();
  if (ready) {
    tft.setTextColor(COLOR_GREEN, COLOR_BLACK);
    tft.print("RDY  YES  ");
  } else {
    tft.setTextColor(COLOR_YELLOW, COLOR_BLACK);
    tft.print("RDY  NO   ");
  }
  tft.setTextColor(COLOR_WHITE, COLOR_BLACK);

  // 4. POWER
  tft.setCursor(160, 90);
  if (ready) {
    snprintf(buf, sizeof(buf), "PWR %+0.3f", batteryPowerKw);
  } else {
    snprintf(buf, sizeof(buf), "PWR  --   ");
  }
  tft.print(buf);

  // 5. USED
  tft.setCursor(160, 110);
  snprintf(buf, sizeof(buf), "USD %0.3f", tripKwhUsed);
  tft.print(buf);

  // 6. REGEN
  tft.setCursor(160, 130);
  snprintf(buf, sizeof(buf), "RGN %0.3f", tripRegenKwh);
  tft.print(buf);

  // Bottom Line: Explicit Skip Reason
  tft.setTextSize(1);
  tft.setCursor(10, 154);
  String skipStr = integrationSkipReason;
  skipStr.toUpperCase();
  snprintf(buf, sizeof(buf), "SKIP: %-20s", skipStr.c_str());
  tft.print(buf);
}
#endif

#if ENABLE_CHG_EST_DEBUG_PAGE
void drawChgEstDebugPage() {
  tft.setTextSize(2);
  tft.setTextColor(COLOR_CYAN, COLOR_BLACK);
  tft.setCursor(8, 6);
  tft.print("CHG EST DBG");
  tft.setTextSize(1);
  tft.setTextColor(COLOR_YELLOW, COLOR_BLACK);
  tft.setCursor(240, 6);
  tft.print("TEMP P9");

  char buf[32];

  // Col 1 (x = 8): CHARGE & POWER
  tft.setTextColor(COLOR_CYAN, COLOR_BLACK);
  tft.setCursor(8, 26); tft.print("[CHARGE]");
  tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
  const char* stStr = (chargeEstimateState == CHG_EST_ACTIVE) ? "ACTIVE  " :
                      (chargeEstimateState == CHG_EST_CANDIDATE) ? "CAND    " :
                      (chargeEstimateState == CHG_EST_END_CANDIDATE) ? "END_CAND" : "NOT_CHG ";
  tft.setCursor(8, 40); snprintf(buf, sizeof(buf), "STATE  %s", stStr); tft.print(buf);
  tft.setCursor(8, 54); snprintf(buf, sizeof(buf), "DETECT %-3s", chargeDetectedRaw ? "YES" : "NO"); tft.print(buf);
  tft.setCursor(8, 68); snprintf(buf, sizeof(buf), "CONF   %-3s", (chargeEstimateState == CHG_EST_ACTIVE) ? "YES" : "NO"); tft.print(buf);
  tft.setCursor(8, 82); snprintf(buf, sizeof(buf), "DIR    %-7s", chargeDirectionValid ? "CHARGE" : "NOT_CHG"); tft.print(buf);
  uint32_t aAge = (hvCurrentLastUpdateMs > 0) ? (millis() - hvCurrentLastUpdateMs) : 99999;
  tft.setCursor(8, 96); snprintf(buf, sizeof(buf), "AGE    %lums ", (unsigned long)aAge); tft.print(buf);

  tft.setTextColor(COLOR_CYAN, COLOR_BLACK);
  tft.setCursor(8, 114); tft.print("[POWER]");
  tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
  if (std::isfinite(chargeInstantPowerKw)) {
    tft.setCursor(8, 128); snprintf(buf, sizeof(buf), "INST  %0.3f kW", chargeInstantPowerKw); tft.print(buf);
  } else {
    tft.setCursor(8, 128); tft.print("INST  --       ");
  }
  if (std::isfinite(avgChargePowerKw)) {
    tft.setCursor(8, 142); snprintf(buf, sizeof(buf), "AVG   %0.3f kW", avgChargePowerKw); tft.print(buf);
  } else {
    tft.setCursor(8, 142); tft.print("AVG   --       ");
  }
  tft.setCursor(8, 156); snprintf(buf, sizeof(buf), "SMPLS %-3d", chargeHistoryCount); tft.print(buf);

  // Col 2 (x = 114): HV DATA & ETA
  tft.setTextColor(COLOR_CYAN, COLOR_BLACK);
  tft.setCursor(114, 26); tft.print("[HV DATA]");
  tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
  if (std::isfinite(hvBatteryVoltage)) {
    tft.setCursor(114, 40); snprintf(buf, sizeof(buf), "V     %0.1f V ", hvBatteryVoltage); tft.print(buf);
  } else {
    tft.setCursor(114, 40); tft.print("V     --        ");
  }
  if (std::isfinite(hvBatteryCurrent)) {
    tft.setCursor(114, 54); snprintf(buf, sizeof(buf), "A    %+0.3f A", hvBatteryCurrent); tft.print(buf);
  } else {
    tft.setCursor(114, 54); tft.print("A     --        ");
  }
  if (std::isfinite(chargeInstantPowerKw)) {
    tft.setCursor(114, 68); snprintf(buf, sizeof(buf), "PWR  %+0.3f kW", chargeInstantPowerKw); tft.print(buf);
  } else {
    tft.setCursor(114, 68); tft.print("PWR   --        ");
  }
  // Consumes captured telemetry snapshot: chargeHvReady and chargeObdFresh
  tft.setCursor(114, 82); snprintf(buf, sizeof(buf), "RDY   %-3s", chargeHvReady ? "YES" : "NO"); tft.print(buf);
  tft.setCursor(114, 96); snprintf(buf, sizeof(buf), "FRESH %-3s", chargeObdFresh ? "YES" : "NO"); tft.print(buf);

  tft.setTextColor(COLOR_CYAN, COLOR_BLACK);
  tft.setCursor(114, 114); tft.print("[ETA]");
  tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
  if (std::isfinite(chargeEnergyNeededKwh)) {
    tft.setCursor(114, 128); snprintf(buf, sizeof(buf), "ENG   %0.3fkWh", chargeEnergyNeededKwh); tft.print(buf);
  } else {
    tft.setCursor(114, 128); tft.print("ENG   --       ");
  }
  if (chargeEtaMinutes >= 0.0f && std::isfinite(chargeEtaMinutes)) {
    tft.setCursor(114, 142); snprintf(buf, sizeof(buf), "TIME  %d min ", (int)lroundf(chargeEtaMinutes)); tft.print(buf);
  } else {
    tft.setCursor(114, 142); tft.print("TIME  --      ");
  }
  tft.setCursor(114, 156); snprintf(buf, sizeof(buf), "STAT  %-8s", chargeEtaStatusStr.c_str()); tft.print(buf);

  // Col 3 (x = 216): SOC TELEMETRY
  tft.setTextColor(COLOR_CYAN, COLOR_BLACK);
  tft.setCursor(216, 26); tft.print("[SOC]");
  tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
  if (socFinalValid && std::isfinite(batt_soc)) {
    tft.setCursor(216, 40); snprintf(buf, sizeof(buf), "NOW   %0.2f%%", batt_soc); tft.print(buf);
  } else {
    tft.setCursor(216, 40); tft.print("NOW   --        ");
  }
  tft.setCursor(216, 54); snprintf(buf, sizeof(buf), "TGT   %d%%  ", chargeTargetSoc); tft.print(buf);
  if (std::isfinite(chargeSocDelta)) {
    tft.setCursor(216, 68); snprintf(buf, sizeof(buf), "DELTA %0.2f%%", chargeSocDelta); tft.print(buf);
  } else {
    tft.setCursor(216, 68); tft.print("DELTA --        ");
  }
  tft.setCursor(216, 82); snprintf(buf, sizeof(buf), "VALID %-3s", socFinalValid ? "YES" : "NO"); tft.print(buf);
  const char* sSrc = (socSource == SOC_SOURCE_BMS_COARSE) ? VP_REQ_SOC :
                     (socSource == SOC_SOURCE_BMS_DETAILED) ? VP_REQ_SOH : "INVALID";
  tft.setCursor(216, 96); snprintf(buf, sizeof(buf), "SRC   %-6s", sSrc); tft.print(buf);
}
#endif

void drawGaugeUI() {
  if (currentPage > TOTAL_PAGES) {
    currentPage = 1;
  }
  static int lastPage = -1;
  static bool lastShowGifHome = false;
  static bool lastChargeAutoBackActive = false;
  static bool lastAutoBackActive = false;

  // Reload logic at higher-level display loop: defer if in Charging AutoBack Mode
  bool shouldReloadNow = loopGifReloadRequested;
  if (chargeAutoBackActive) {
    shouldReloadNow = false;
  }

  if (shouldReloadNow) {
    Serial.println("[GIFRELOAD] reload flag detected in higher-level drawGaugeUI");
    if (theme2GifOpen) {
      Serial.println("[GIFRELOAD] closing current gif in drawGaugeUI");
      gif.close();
      theme2GifOpen = false;
    }
    themeChanged = true;
    gifHomeNeedClear = true;
    loopGifReloadRequested = false;
    gifOpenFailed = false;
    lastGifOpenAttemptMs = 0;
  }

  // Track state transitions to clear screen and force full update without residual artifacts
  if (currentPage != lastPage || 
      showGifHome != lastShowGifHome || 
      chargeAutoBackActive != lastChargeAutoBackActive || 
      autoBackActive != lastAutoBackActive ||
      shouldReloadNow) {
    
    if (theme2GifOpen) {
      gif.close();
      theme2GifOpen = false;
    }
    gifHomeNeedClear = true;
    themeChanged = true;
    tft.fillScreen(COLOR_BLACK);
    
    lastPage = currentPage;
    lastShowGifHome = showGifHome;
    lastChargeAutoBackActive = chargeAutoBackActive;
    lastAutoBackActive = autoBackActive;
  }

  // Update display smoothing every frame
  updateDisplaySmoothing();

  // Priority Stack: Alert Override > Charge AutoBack > Stop AutoBack > Existing Normal Mode
  if (chargeAutoBackActive) {
    drawChargeEstimateAutoBack();
  } else if (autoBackActive) {
    if (autoBackPageType == 0) {
      drawGIFHome_T02();
    } else if (autoBackPageType == 1) {
      drawRoboEyes_T01();
    } else if (autoBackPageType == 2) {
      drawAutoBackStopwatch();
    } else {
      if (showGifHome) {
        drawGIFHome_T02();
      } else {
        drawRoboEyes_T01();
      }
    }
  } else {
    // Existing Normal Mode
    if (currentPage == 1) {
      if (showGifHome) {
        drawGIFHome_T02();
      } else {
        drawRoboEyes_T01();
      }
    }
#if ENABLE_CHG_DEBUG_PAGE
    else if (currentPage == 8) {
      drawChgDebugPage();
    }
#endif
#if ENABLE_CHG_EST_DEBUG_PAGE
    else if (currentPage == 9) {
      drawChgEstDebugPage();
    }
#endif
    else {
      drawGaugeUI_T01();
    }
  }
}
