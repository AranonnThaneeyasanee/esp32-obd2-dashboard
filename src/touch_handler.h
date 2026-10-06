#pragma once

// ============================================================
// Touch Sensor Input Check
// Short press (<1500ms) = page switch
// Long press (>=1500ms) = theme switch
// ============================================================
void checkTouchInput() {
  if (WiFi.softAPgetStationNum() > 0) {
    return; // Ignore touch input while setup portal is active (prevents black
            // screen)
  }
  static bool touched = false;
  static unsigned long touchStartTime = 0;
  static bool longPressFired = false;

  bool val = digitalRead(TOUCH_PIN);

  if (val == HIGH && !touched) {
    // Rising edge — start timing
    touched = true;
    touchStartTime = millis();
    longPressFired = false;
  }

  if (val == HIGH && touched && !longPressFired) {
    // Check if held long enough (>= 1500ms)
    if (millis() - touchStartTime >= 1500) {
      longPressFired = true;
      if (chargeAutoBackActive || autoBackActive) {
        Serial.println(">>> Long press ignored during active ChargeAutoBack/AutoBack Mode");
      } else if (currentPage == 1) {
        // Toggle Home GIF vs Robo Eyes
        preferences.begin("obd2_dash", false);
        showGifHome = !showGifHome;
        preferences.putBool("gif_home", showGifHome);
        preferences.end();
        Serial.print(">>> Home Screen Style switched: ");
        Serial.println(showGifHome ? "GIF Loop" : "Robo Eyes");
        themeChanged = true; // Trigger full redraw
        tft.fillScreen(COLOR_BLACK);
      }
    }
  }

  if (val == LOW && touched) {
    // Falling edge
    if (!longPressFired) {
      if (chargeAutoBackActive) {
        chargeAutoBackActive = false;
        chargeAutoBackSessionUsed = true; // Prevents auto-popup again in this session
        currentPage = previousPageBeforeAutoBack;
        themeChanged = true;
        tft.fillScreen(COLOR_BLACK);
        Serial.println(">>> Charge Estimate AutoBack exited immediately via touch, marked session as used");
      } else if (autoBackActive) {
        autoBackActive = false;
        currentPage = previousPageBeforeAutoBack;
        themeChanged = true;
        tft.fillScreen(COLOR_BLACK);
        Serial.println(">>> AutoBack exited immediately via touch");
      } else {
        // Short press — page switch
        currentPage++;
        if (currentPage > TOTAL_PAGES) {
          currentPage = 1;
        }
        Serial.print(">>> Page Switched to: ");
        Serial.println(currentPage);
        tft.fillScreen(COLOR_BLACK);
      }
    }
    touched = false;
    longPressFired = false;
  }
}
