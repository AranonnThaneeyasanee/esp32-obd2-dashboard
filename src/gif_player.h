#pragma once

// ============================================================
// Boot GIF Animation
// ============================================================
void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s = pDraw->pPixels;
  uint16_t *usPalette = pDraw->pPalette;
  int x;

  int canvasW = gif.getCanvasWidth();
  int canvasH = gif.getCanvasHeight();
  if (canvasW <= 0)
    canvasW = 320;
  if (canvasH <= 0)
    canvasH = 172;

  if (canvasW == 320 && canvasH == 172) {
    // Fast Path (No Scaling)
    int absY = pDraw->iY + pDraw->y;
    if (absY >= 0 && absY < 172) {
      int startX = pDraw->iX;
      int len = pDraw->iWidth;
      // Clamp horizontally
      if (startX < 0) {
        len += startX;
        s -= startX;
        startX = 0;
      }
      if (startX + len > 320) {
        len = 320 - startX;
      }
      if (len > 0) {
        uint16_t *dest = &gifCanvas[absY * 320 + startX];
        if (pDraw->ucHasTransparency) {
          uint8_t trans = pDraw->ucTransparent;
          for (x = 0; x < len; x++) {
            uint8_t pixel = s[x];
            if (pixel != trans) {
              dest[x] = usPalette[pixel];
            }
          }
        } else {
          for (x = 0; x < len; x++) {
            dest[x] = usPalette[s[x]];
          }
        }
      }
    }
  } else {
    // Slow Path (With Scaling - Precalculated X mappings to avoid inner-loop division)
    int absY = pDraw->iY + pDraw->y;
    int targetY_start = (absY * 172) / canvasH;
    int targetY_end = ((absY + 1) * 172) / canvasH;

    // Clamp vertical bounds
    if (targetY_start < 0)
      targetY_start = 0;
    if (targetY_end > 172)
      targetY_end = 172;

    if (targetY_start < targetY_end) {
      // Stack allocation for precalculated X values to eliminate division in loop
      int width_to_map = pDraw->iWidth;
      if (width_to_map > 480)
        width_to_map = 480;

      static int targetX_starts[480];
      static int targetX_ends[480];
      for (x = 0; x < width_to_map; x++) {
        int absX = pDraw->iX + x;
        targetX_starts[x] = (absX * 320) / canvasW;
        targetX_ends[x] = ((absX + 1) * 320) / canvasW;
      }

      for (x = 0; x < pDraw->iWidth; x++) {
        uint8_t pixel_index = s[x];

        // Skip transparent pixels
        if (pDraw->ucHasTransparency && pixel_index == pDraw->ucTransparent) {
          continue;
        }

        uint16_t color = usPalette[pixel_index];
        int targetX_start =
            (x < 480) ? targetX_starts[x] : ((pDraw->iX + x) * 320) / canvasW;
        int targetX_end = (x < 480) ? targetX_ends[x]
                                    : (((pDraw->iX + x + 1) * 320) / canvasW);

        // Clamp horizontal bounds
        if (targetX_start < 0)
          targetX_start = 0;
        if (targetX_end > 320)
          targetX_end = 320;

        if (targetX_start < targetX_end) {
          for (int ty = targetY_start; ty < targetY_end; ty++) {
            for (int tx = targetX_start; tx < targetX_end; tx++) {
              gifCanvas[ty * 320 + tx] = color;
            }
          }
        }
      }
    }
  }

  // If this is the last line of the current frame, push ONLY the updated
  // bounding box to TFT
  if (pDraw->y == pDraw->iHeight - 1) {
    int targetX, targetY, targetW, targetH;
    if (canvasW == 320 && canvasH == 172) {
      targetX = pDraw->iX;
      targetY = pDraw->iY;
      targetW = pDraw->iWidth;
      targetH = pDraw->iHeight;
    } else {
      int targetX_start = (pDraw->iX * 320) / canvasW;
      int targetX_end =
          (((pDraw->iX + pDraw->iWidth) * 320) + canvasW - 1) / canvasW;
      int targetY_start = (pDraw->iY * 172) / canvasH;
      int targetY_end =
          (((pDraw->iY + pDraw->iHeight) * 172) + canvasH - 1) / canvasH;

      targetX = targetX_start;
      targetY = targetY_start;
      targetW = targetX_end - targetX_start;
      targetH = targetY_end - targetY_start;
    }

    // Clamp bounding box to screen coordinates
    if (targetX < 0) {
      targetW += targetX;
      targetX = 0;
    }
    if (targetY < 0) {
      targetH += targetY;
      targetY = 0;
    }
    if (targetX + targetW > 320)
      targetW = 320 - targetX;
    if (targetY + targetH > 172)
      targetH = 172 - targetY;

    if (targetW > 0 && targetH > 0) {
      if (targetX == 0 && targetY == 0 && targetW == 320 && targetH == 172) {
        // Full screen refresh can use the direct Adafruit GFX API
        tft.drawRGBBitmap(0, 0, gifCanvas, 320, 172);
      } else {
        // Sub-rectangle windowed writes using raw SPI transactions
        tft.startWrite();
        tft.setAddrWindow(targetX, targetY, targetW, targetH);
        for (int16_t row = 0; row < targetH; row++) {
          tft.writePixels(&gifCanvas[(targetY + row) * 320 + targetX], targetW);
        }
        tft.endWrite();
      }
    }
  }
}

void drawSafeBootScreen() {
  tft.fillScreen(COLOR_BLACK);
  tft.setTextColor(COLOR_CYAN, COLOR_BLACK);
  tft.setTextSize(3);
  // Center "OBD2 DASHBOARD" (14 chars * 18 pixels wide = 252 pixels)
  int xPos = (320 - 252) / 2;
  tft.setCursor(xPos, 50);
  tft.print("OBD2 DASHBOARD");

  tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
  tft.setTextSize(2);
  // Center "Starting..." (11 chars * 12 pixels = 132 pixels)
  tft.setCursor((320 - 132) / 2, 100);
  tft.print("Starting...");
}

bool openGIFSafely(AnimatedGIF &gifObj, const char *path) {
  bool isLoopGif = (strcmp(path, "/loop.gif") == 0);
  bool isBootGif = (strcmp(path, "/boot.gif") == 0);

  if (isLoopGif) {
    Serial.printf("[GIFRELOAD] openGIFSafely source=LittleFS %s\n", path);
    lastGifSource = "none";
    lastGifOpenResult = false;
    lastGifOpenError = "";
  }

  bool opened = false;

  // 1. Try opening active GIF from LittleFS
  if (LittleFS.exists(path)) {
    File f = LittleFS.open(path, "r");
    if (f) {
      size_t sz = f.size();
      if (sz > 0 && sz <= 204800) {
        uint8_t h[6];
        if (f.read(h, 6) == 6 &&
            h[0] == 'G' && h[1] == 'I' && h[2] == 'F' &&
            h[3] == '8' && (h[4] == '7' || h[4] == '9') && h[5] == 'a') {
          if (ESP.getFreeHeap() >= 15000) {
            f.close();
            // Explicitly close any stale gifFile handle before opening to avoid LittleFS conflicts
            if (gifFile) { gifFile.close(); }
            opened = gifObj.open(path, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw);
            if (opened) {
              Serial.println("[GIF_TXN] DECODER_RESULT=SUCCESS");
              if (isLoopGif) {
                lastGifSource = "littlefs_loop";
                lastGifOpenResult = true;
              }
            } else {
              int errCode = gifObj.getLastError();
              Serial.printf("[GIF_TXN] DECODER_RESULT=FAIL error=%d\n", errCode);
              Serial.printf("[GIF] Failed to open active %s! (AnimatedGIF error code: %d)\n", path, errCode);
              if (isLoopGif) {
                lastGifOpenError = "AnimatedGIF open failed, code: " + String(errCode);
              }
            }
          } else {
            f.close();
            Serial.printf("[GIF] Safely reject active %s: Heap too low (%d bytes)\n", path, ESP.getFreeHeap());
            if (isLoopGif) {
              lastGifOpenError = "Heap too low: " + String(ESP.getFreeHeap()) + " bytes";
            }
          }
        } else {
          f.close();
          Serial.printf("[GIF] Safely reject active %s: Invalid GIF header signature\n", path);
          if (isLoopGif) {
            lastGifOpenError = "Invalid header signature";
          }
        }
      } else {
        f.close();
        Serial.printf("[GIF] Safely reject active %s: Invalid size %d (max 200KB)\n", path, sz);
        if (isLoopGif) {
          lastGifOpenError = "Invalid size: " + String(sz) + " bytes";
        }
      }
    } else {
      Serial.printf("[GIF] Safely reject active %s: Cannot open file\n", path);
      if (isLoopGif) {
        lastGifOpenError = "Cannot open file";
      }
    }
  }

  // 2. If active file fails, try default GIF from LittleFS (/defaults/...)
  if (!opened) {
    String defaultPath = "";
    if (isLoopGif) {
      defaultPath = "/defaults/loop.gif";
    } else if (isBootGif) {
      defaultPath = "/defaults/boot.gif";
    }

    if (defaultPath.length() > 0 && LittleFS.exists(defaultPath)) {
      File f = LittleFS.open(defaultPath, "r");
      if (f) {
        size_t sz = f.size();
        if (sz > 0 && sz <= 204800) {
          uint8_t h[6];
          if (f.read(h, 6) == 6 &&
              h[0] == 'G' && h[1] == 'I' && h[2] == 'F' &&
              h[3] == '8' && (h[4] == '7' || h[4] == '9') && h[5] == 'a') {
            if (ESP.getFreeHeap() >= 15000) {
              f.close();
              // Explicitly close any stale gifFile handle before opening to avoid LittleFS conflicts
              if (gifFile) { gifFile.close(); }
              opened = gifObj.open(defaultPath.c_str(), GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw);
              if (opened) {
                if (isLoopGif) {
                  lastGifSource = "builtin_fallback";
                  lastGifOpenResult = true;
                  Serial.println("[GIF] Switched to default loop.gif asset.");
                } else {
                  Serial.println("[GIF] Switched to default boot.gif asset.");
                }
              } else {
                Serial.printf("[GIF] Failed to open default %s! (AnimatedGIF open failed)\n", defaultPath.c_str());
              }
            } else {
              f.close();
              Serial.printf("[GIF] Safely reject default %s: Heap too low (%d bytes)\n", defaultPath.c_str(), ESP.getFreeHeap());
            }
          } else {
            f.close();
            Serial.printf("[GIF] Safely reject default %s: Invalid GIF header signature\n", defaultPath.c_str());
          }
        } else {
          f.close();
          Serial.printf("[GIF] Safely reject default %s: Invalid size %d\n", defaultPath.c_str(), sz);
        }
      }
    }
  }

  // 3. If default file fails, try emergency fallback in PROGMEM (if set)
  if (!opened) {
    if (isLoopGif && EMERGENCY_LOOP_GIF_LEN > 0) {
      opened = gifObj.open((uint8_t *)EMERGENCY_LOOP_GIF, EMERGENCY_LOOP_GIF_LEN, GIFDraw);
      if (opened) {
        lastGifSource = "builtin_fallback";
        lastGifOpenResult = true;
        Serial.println("[GIF] Switched to emergency PROGMEM loop GIF fallback.");
      }
    } else if (isBootGif && EMERGENCY_BOOT_GIF_LEN > 0) {
      opened = gifObj.open((uint8_t *)EMERGENCY_BOOT_GIF, EMERGENCY_BOOT_GIF_LEN, GIFDraw);
      if (opened) {
        Serial.println("[GIF] Switched to emergency PROGMEM boot GIF fallback.");
      }
    }
  }

  return opened;
}

void playBootAnimation() {
  tft.fillScreen(COLOR_BLACK);
  memset(gifCanvas, 0, sizeof(gifCanvas));
  gif.begin(GIF_PALETTE_RGB565_LE);

  bool opened = openGIFSafely(gif, "/boot.gif");

  if (opened) {
    Serial.printf("Boot GIF: %dx%d\n", gif.getCanvasWidth(),
                  gif.getCanvasHeight());
    int frameCount = 0;
    while (gif.playFrame(true, NULL) > 0)
      frameCount++;
    Serial.printf("Boot animation complete! (%d frames)\n", frameCount + 1);
    gif.close();
  } else {
    Serial.println("[WARN] Failed to open boot GIF. Showing code-drawn safe boot screen.");
    drawSafeBootScreen();
    delay(1500); // Allow user to see the starting screen
  }

  tft.fillScreen(COLOR_BLACK);
}
