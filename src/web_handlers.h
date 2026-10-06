#pragma once

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

// Maximum allowed GIF file size — shared by upload validation and startup recovery.
// Update this value to change the limit everywhere automatically.
#define GIF_MAX_FILE_BYTES (200UL * 1024UL)

// Read first 6 bytes of a file and return as String (empty if file missing/unreadable)
String readFileHeader(const String& path) {
  File f = LittleFS.open(path, "r");
  if (!f) return "";
  uint8_t h[6];
  if (f.read(h, 6) != 6) { f.close(); return ""; }
  f.close();
  char buf[7];
  memcpy(buf, h, 6);
  buf[6] = '\0';
  return String(buf);
}

bool isValidGifHeader(const String& hdr) {
  return (hdr == "GIF87a" || hdr == "GIF89a");
}

// Lightweight GIF file validation for startup recovery.
// Checks: exists, size > 0, size <= GIF_MAX_FILE_BYTES, valid GIF header,
// Logical Screen Descriptor width > 0, height > 0.
// Reads exactly 10 bytes then closes immediately.
// Does NOT decode frames, does NOT call any GIF renderer or player.
bool isGifFileValid(const String& path) {
  if (!LittleFS.exists(path)) return false;

  File f = LittleFS.open(path, "r");
  if (!f) return false;

  size_t sz = f.size();
  if (sz < 13 || sz > GIF_MAX_FILE_BYTES) {
    f.close();
    return false;
  }

  uint8_t buf[10];
  if (f.read(buf, 10) != 10) {
    f.close();
    return false;
  }

  // Bytes 0-5: GIF signature + version (GIF87a / GIF89a)
  if (!(buf[0]=='G' && buf[1]=='I' && buf[2]=='F' &&
        buf[3]=='8' && (buf[4]=='7' || buf[4]=='9') && buf[5]=='a')) {
    f.close();
    return false;
  }

  // Bytes 6-7: Logical Screen Width (little-endian uint16)
  uint16_t w = (uint16_t)buf[6] | ((uint16_t)buf[7] << 8);
  // Bytes 8-9: Logical Screen Height (little-endian uint16)
  uint16_t h = (uint16_t)buf[8] | ((uint16_t)buf[9] << 8);
  if (w == 0 || h == 0) {
    f.close();
    return false;
  }

  // Structural Integrity Check: Seek to last byte and verify GIF Trailer (0x3B)
  if (!f.seek(sz - 1)) {
    f.close();
    return false;
  }

  uint8_t trailer = 0;
  if (f.read(&trailer, 1) != 1 || trailer != 0x3B) {
    f.close();
    return false;
  }

  f.close();
  return true;
}

/**
 * @brief Generic Byte-Copy Helper with Size Verification
 * 
 * Contract & Guarantees:
 * 1. Copies file block-by-block from srcPath to destPath.
 * 2. Verifies dest.size() == src.size() post-copy.
 * 3. NOTE: Does NOT perform GIF format or image structure validation.
 * 4. GIF callers MUST perform separate format validation using isGifFileValid(destPath).
 */
bool copyFileVerified(const String& srcPath, const String& destPath) {
  File src = LittleFS.open(srcPath, "r");
  if (!src) {
    Serial.printf("[HTTP] CopyV: cannot open src %s\n", srcPath.c_str());
    return false;
  }
  size_t srcSize = src.size();
  
  File dest = LittleFS.open(destPath, "w");
  if (!dest) {
    Serial.printf("[HTTP] CopyV: cannot open dest %s\n", destPath.c_str());
    src.close();
    return false;
  }
  
  uint8_t buf[1024];
  size_t bytesRead = 0;
  bool writeOk = true;
  while ((bytesRead = src.read(buf, sizeof(buf))) > 0) {
    if (dest.write(buf, bytesRead) != bytesRead) {
      writeOk = false;
      break;
    }
    yield();
  }
  dest.close();
  src.close();
  
  if (!writeOk) {
    Serial.println("[HTTP] CopyV: write error during copy");
    LittleFS.remove(destPath);
    return false;
  }
  
  // Verify destination size matches source
  File check = LittleFS.open(destPath, "r");
  if (!check) {
    Serial.println("[HTTP] CopyV: cannot reopen dest for verify");
    return false;
  }
  size_t destSize = check.size();
  check.close();
  
  if (destSize != srcSize) {
    Serial.printf("[HTTP] CopyV: size mismatch src=%d dest=%d\n", srcSize, destSize);
    LittleFS.remove(destPath);
    return false;
  }
  
  Serial.printf("[HTTP] CopyV: success %s -> %s (%d bytes)\n", srcPath.c_str(), destPath.c_str(), destSize);
  return true;
}

// ============================================================
// Filesystem & WiFi Web Portal Helpers
// ============================================================
void *GIFOpenFile(const char *fname, int32_t *pSize) {
  gifFile = LittleFS.open(fname, "r");
  if (gifFile) {
    *pSize = gifFile.size();
    return (void *)&gifFile;
  }
  return NULL;
}

void GIFCloseFile(void *pHandle) {
  if (gifFile) {
    gifFile.close();
  }
}

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  int32_t readBytes = 0;
  if (gifFile) {
    readBytes = gifFile.read(pBuf, iLen);
    pFile->iPos += readBytes;
  }
  return readBytes;
}

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
  if (gifFile) {
    if (gifFile.seek(iPosition)) {
      pFile->iPos = iPosition;
      return iPosition;
    }
  }
  return pFile->iPos;
}

void handleRoot() {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.sendHeader("Location", "/setup", true);
  server.send(302, "text/plain", "");
}

void handleUpload() {
  HTTPUpload &upload = server.upload();
  
  static size_t lastPrintedSize = 0;
  static size_t uploadReceivedBytes = 0;

  // 1. Timeout / Stale Lock Recovery check BEFORE updating lastUploadActivityMs
  if (uploadInProgress && (millis() - lastUploadActivityMs > 15000)) {
    uploadInProgress = false;
    uploadError = false;
    uploadErrorMsg = "";
    Serial.println("[UPLOAD] Stale upload lock timeout recovered (no activity for 15s).");
  }

  // 2. Track last activity timestamp on incoming activity
  lastUploadActivityMs = millis();

  if (upload.status == UPLOAD_FILE_START) {
    // Server-side race guard: reject if another upload is already running
    if (uploadInProgress) {
      uploadError = true;
      uploadErrorMsg = "CONFLICT: Another upload is already in progress.";
      Serial.println("[HTTP] REJECTED: concurrent upload attempt while uploadInProgress=true");
      return;
    }
    uploadInProgress = true;
    lastPrintedSize = 0;
    uploadReceivedBytes = 0;

    uploadError = false;
    uploadErrorMsg = "";
    writeBufferLen = 0;

    uploadSuccessPath = "";
    uploadSuccessSize = 0;
    uploadSuccessHeader = "";

    String type = server.arg("type");
    if (type.length() == 0) {
      type = server.arg("target");
    }
    
    String tempPath = (type == "loop") ? "/loop_upload.tmp" : "/boot_upload.tmp";
    if (LittleFS.exists(tempPath)) {
      LittleFS.remove(tempPath);
    }
    
    Serial.println("[UPLOAD] FILE_START");
    Serial.printf("[UPLOAD] filename=%s\n", upload.filename.c_str());
    Serial.printf("[UPLOAD] expected framework totalSize=%d\n", upload.totalSize);
    Serial.printf("[UPLOAD] tempPath=%s\n", tempPath.c_str());
    Serial.printf("[UPLOAD] freeHeap=%d\n", ESP.getFreeHeap());

    uploadFile = LittleFS.open(tempPath, "w");
    if (!uploadFile) {
      uploadError = true;
      uploadErrorMsg = "Failed to open temporary file on filesystem for writing.";
      Serial.println("[HTTP] Failed to open temporary file for writing.");
      uploadInProgress = false;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile && !uploadError) {
      size_t freeBytes = (LittleFS.totalBytes() > LittleFS.usedBytes()) ? (LittleFS.totalBytes() - LittleFS.usedBytes()) : 0;
      size_t bytesToCopy = upload.currentSize;

      // Arithmetic overflow-safe payload boundary guard
      if (bytesToCopy > GIF_MAX_FILE_BYTES || uploadReceivedBytes > GIF_MAX_FILE_BYTES - bytesToCopy || (upload.totalSize > 0 && upload.totalSize > GIF_MAX_FILE_BYTES)) {
        uploadError = true;
        uploadErrorMsg = "PAYLOAD_TOO_LARGE: File size exceeds 200KB limit.";
        Serial.printf("[HTTP] File exceeds 200KB limit. Received: %d, Chunk: %d\n", uploadReceivedBytes, bytesToCopy);
        uploadFile.close();
        String type = server.arg("type");
        if (type.length() == 0) type = server.arg("target");
        String tempPath = (type == "loop") ? "/loop_upload.tmp" : "/boot_upload.tmp";
        if (LittleFS.exists(tempPath)) LittleFS.remove(tempPath);
        uploadInProgress = false;
      } else if (freeBytes < bytesToCopy + (10 * 1024)) { // 10KB filesystem margin during stream write
        uploadError = true;
        uploadErrorMsg = "INSUFFICIENT_FLASH_SPACE: Free: " + String(freeBytes) + "B";
        Serial.printf("[HTTP] Flash full during stream write. Free: %d, Chunk: %d\n", freeBytes, bytesToCopy);
        uploadFile.close();
        String type = server.arg("type");
        if (type.length() == 0) type = server.arg("target");
        String tempPath = (type == "loop") ? "/loop_upload.tmp" : "/boot_upload.tmp";
        if (LittleFS.exists(tempPath)) LittleFS.remove(tempPath);
        uploadInProgress = false;
      } else {
        size_t offset = 0;

        while (bytesToCopy > 0) {
          size_t space = WRITE_BUFF_SIZE - writeBufferLen;
          size_t chunk = (bytesToCopy < space) ? bytesToCopy : space;

          memcpy(writeBuffer + writeBufferLen, upload.buf + offset, chunk);
          writeBufferLen += chunk;
          offset += chunk;
          bytesToCopy -= chunk;

          if (writeBufferLen == WRITE_BUFF_SIZE) {
            size_t written = uploadFile.write(writeBuffer, WRITE_BUFF_SIZE);
            yield(); // Let WiFi/TCP stack process packets
            if (written != WRITE_BUFF_SIZE) {
              uploadError = true;
              uploadErrorMsg = "Failed to write data to flash storage.";
              Serial.println("[UPLOAD] WRITE_ERROR");
              Serial.printf("[UPLOAD] requested=%d, written=%d, receivedBefore=%d\n", WRITE_BUFF_SIZE, written, uploadReceivedBytes);
              uploadFile.close();
              String type = server.arg("type");
              if (type.length() == 0) type = server.arg("target");
              String tempPath = (type == "loop") ? "/loop_upload.tmp" : "/boot_upload.tmp";
              if (LittleFS.exists(tempPath)) LittleFS.remove(tempPath);
              uploadInProgress = false;
              break;
            } else {
              // Increment ONLY after File.write() reports full chunk written!
              uploadReceivedBytes += written;
            }
            writeBufferLen = 0;
          }
        }
      }
    }
    yield(); // Yield after each chunk to keep WiFi alive
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Serial.println("[UPLOAD] FILE_ABORTED");
    Serial.printf("[UPLOAD] filename=%s, receivedBytes=%d, frameworkTotal=%d\n",
                  upload.filename.c_str(), uploadReceivedBytes, upload.totalSize);
    if (uploadFile) uploadFile.close();
    String type = server.arg("type");
    if (type.length() == 0) type = server.arg("target");
    String tempPath = (type == "loop") ? "/loop_upload.tmp" : "/boot_upload.tmp";
    if (LittleFS.exists(tempPath)) LittleFS.remove(tempPath);
    uploadInProgress = false;
  } else if (upload.status == UPLOAD_FILE_END) {
    String type = server.arg("type");
    if (type.length() == 0) {
      type = server.arg("target");
    }
    String tempPath = (type == "loop") ? "/loop_upload.tmp" : "/boot_upload.tmp";
    String finalPath = (type == "loop") ? "/loop.gif" : "/boot.gif";
    String backupPath = (type == "loop") ? "/loop_backup.gif" : "/boot_backup.gif";

    // 1. Early Error Gate: If File.write() failed during upload stream, terminate immediately!
    if (uploadError) {
      Serial.println("[GIF_TXN] UPLOAD_RESULT=FAIL");
      Serial.println("[GIF_TXN] reason=WRITE_ERROR");
      if (uploadFile) uploadFile.close();
      if (LittleFS.exists(tempPath)) LittleFS.remove(tempPath);
      uploadInProgress = false;
      return;
    }

    if (uploadFile) {
      if (writeBufferLen > 0) {
        size_t written = uploadFile.write(writeBuffer, writeBufferLen);
        yield();
        if (written != writeBufferLen) {
          uploadError = true;
          uploadErrorMsg = "Failed to write remaining data to flash storage.";
          Serial.println("[UPLOAD] WRITE_ERROR");
          Serial.printf("[UPLOAD] requested=%d, written=%d, receivedBefore=%d\n", writeBufferLen, written, uploadReceivedBytes);
        } else {
          uploadReceivedBytes += written;
        }
      }
      uploadFile.flush();
      uploadFile.close();
    }

    if (uploadError) {
      Serial.println("[GIF_TXN] UPLOAD_RESULT=FAIL");
      Serial.println("[GIF_TXN] reason=WRITE_ERROR");
      if (LittleFS.exists(tempPath)) LittleFS.remove(tempPath);
      uploadInProgress = false;
      return;
    }

    // Reset diagnostic state
    uploadDiagTarget = finalPath;
    uploadDiagTemp = tempPath;
    uploadDiagBackup = backupPath;
    uploadDiagTempExists = false;
    uploadDiagTempSize = 0;
    uploadDiagTempHeader = "";
    uploadDiagFinalExistsBefore = false;
    uploadDiagFinalSizeBefore = 0;
    uploadDiagFinalHeaderBefore = "";
    uploadDiagBackupExistsBefore = false;
    uploadDiagBackupRemoveOk = false;
    uploadDiagBackupRenameOk = false;

    // Authoritative filesystem verification: re-open tempPath from LittleFS
    size_t tempDiskSize = 0;
    File checkTemp = LittleFS.open(tempPath, "r");
    if (checkTemp) {
      tempDiskSize = checkTemp.size();
      checkTemp.close();
    }
    uploadDiagTempSize = tempDiskSize;

    // Required Diagnostic Logging: UPLOAD_SIZE_DIAG
    Serial.println("[GIF_TXN] UPLOAD_SIZE_DIAG");
    Serial.printf("  frameworkTotal=%d\n", upload.totalSize);
    Serial.printf("  receivedBytes=%d\n", uploadReceivedBytes);
    Serial.printf("  diskSize=%d\n", tempDiskSize);

    Serial.println("[GIF_TXN] UPLOAD_BUFFER_DIAG");
    Serial.printf("  pendingBuffer=%d\n", writeBufferLen);
    Serial.printf("  writeError=%s\n", uploadError ? "true" : "false");

    // Framework size consistency check (frameworkTotal=0 is NOT treated as a mismatch)
    bool frameworkSizeOk = (upload.totalSize == 0) || (upload.totalSize == tempDiskSize);

    // Size Integrity Gate
    if (tempDiskSize != uploadReceivedBytes || !frameworkSizeOk || tempDiskSize == 0 || tempDiskSize > GIF_MAX_FILE_BYTES) {
      uploadError = true;
      uploadErrorMsg = "Payload size mismatch or invalid. Disk=" + String(tempDiskSize) + "B, Received=" + String(uploadReceivedBytes) + "B, FrameworkTotal=" + String(upload.totalSize) + "B";
      Serial.println("[GIF_TXN] UPLOAD_RESULT=FAIL");
      Serial.println("[GIF_TXN] reason=SIZE_MISMATCH");
      if (LittleFS.exists(tempPath)) LittleFS.remove(tempPath);
      uploadInProgress = false;
      return;
    }

    // GIF Signature & Trailer Diagnostics
    uint8_t sig[8] = {0};
    uint8_t trailerByte = 0;
    bool trailerValid = false;

    File tempF = LittleFS.open(tempPath, "r");
    if (tempF) {
      size_t sz = tempF.size();
      if (sz >= 8) tempF.read(sig, 8);
      if (sz > 0 && tempF.seek(sz - 1)) {
        tempF.read(&trailerByte, 1);
        if (trailerByte == 0x3B) trailerValid = true;
      }
      tempF.close();
    }

    Serial.println("[GIF_TXN] GIF_SIGNATURE_DIAG");
    Serial.printf("  first8=%02X %02X %02X %02X %02X %02X %02X %02X\n",
                  sig[0], sig[1], sig[2], sig[3], sig[4], sig[5], sig[6], sig[7]);
    Serial.println("[GIF_TXN] GIF_TRAILER_DIAG");
    Serial.printf("  size=%d\n", tempDiskSize);
    Serial.printf("  lastByte=0x%02X\n", trailerByte);
    Serial.printf("  valid=%s\n", trailerValid ? "true" : "false");

    // Step 0: Validate temp file with Single Source of Truth
    if (!isGifFileValid(tempPath)) {
      uploadError = true;
      uploadErrorMsg = "Temp file invalid (failed isGifFileValid check).";
      Serial.println("[GIF_TXN] UPLOAD_RESULT=FAIL");
      Serial.println("[GIF_TXN] reason=GIF_TRAILER_INVALID");
      if (LittleFS.exists(tempPath)) LittleFS.remove(tempPath);
      uploadInProgress = false;
      return;
    }

    Serial.println("[GIF_TXN] TEMP_VALID");

    // Size-Aware Space Diagnostics before transaction commit
    size_t freeBytes = (LittleFS.totalBytes() > LittleFS.usedBytes()) ? (LittleFS.totalBytes() - LittleFS.usedBytes()) : 0;
    size_t activeFileSize = 0;
    if (LittleFS.exists(finalPath)) {
      File actF = LittleFS.open(finalPath, "r");
      if (actF) {
        activeFileSize = actF.size();
        actF.close();
      }
    }
    size_t backupFileSize = 0;
    if (LittleFS.exists(backupPath)) {
      File bF = LittleFS.open(backupPath, "r");
      if (bF) {
        backupFileSize = bF.size();
        bF.close();
      }
    }
    const size_t overheadBytes = 20 * 1024; // 20KB transaction safety margin
    // Reserve space for maximum copy fallback (either active->backup or temp->final) + overhead
    const size_t fallbackCopyBytes = (activeFileSize > tempDiskSize) ? activeFileSize : tempDiskSize;
    const size_t requiredTxnBytes = fallbackCopyBytes + overheadBytes;
    bool spacePass = (freeBytes >= requiredTxnBytes);

    Serial.println("[GIF_TXN] SPACE_DIAG");
    Serial.printf("  free=%u\n", (unsigned)freeBytes);
    Serial.printf("  incoming=%u\n", (unsigned)tempDiskSize);
    Serial.printf("  active=%u\n", (unsigned)activeFileSize);
    Serial.printf("  backup=%u\n", (unsigned)backupFileSize);
    Serial.printf("  overhead=%u\n", (unsigned)overheadBytes);
    Serial.printf("  fallbackReserve=%u\n", (unsigned)fallbackCopyBytes);
    Serial.printf("  required=%u\n", (unsigned)requiredTxnBytes);
    Serial.printf("  result=%s\n", spacePass ? "PASS" : "FAIL");

    if (!spacePass) {
      uploadError = true;
      uploadErrorMsg = "Insufficient space for commit transaction. Free: " + String(freeBytes) + "B, Required: " + String(requiredTxnBytes) + "B";
      Serial.println("[GIF_TXN] UPLOAD_RESULT=FAIL");
      Serial.println("[GIF_TXN] reason=INSUFFICIENT_SPACE");
      if (LittleFS.exists(tempPath)) LittleFS.remove(tempPath);
      uploadInProgress = false;
      return;
    }

    // --- Step 1: Handle existing final file ---
    bool needRestore = false;
    uploadDiagFinalExistsBefore = LittleFS.exists(finalPath);
    if (uploadDiagFinalExistsBefore) {
      bool finalIsValid = isGifFileValid(finalPath);
      if (!finalIsValid) {
        Serial.println("[HTTP] Existing final is invalid. Deleting directly.");
        LittleFS.remove(finalPath);
      } else {
        uploadDiagBackupExistsBefore = LittleFS.exists(backupPath);
        if (uploadDiagBackupExistsBefore) {
          uploadDiagBackupRemoveOk = LittleFS.remove(backupPath);
          Serial.printf("[GIF_TXN] OLD_BACKUP_REMOVE: %s\n", uploadDiagBackupRemoveOk ? "ok" : "fail");
        }

        uploadDiagBackupRenameOk = LittleFS.rename(finalPath, backupPath);
        Serial.printf("[HTTP] Rename final->backup: %s\n", uploadDiagBackupRenameOk ? "ok" : "fail");

        if (!uploadDiagBackupRenameOk) {
          Serial.println("[HTTP] Rename failed. Trying copyFileVerified...");
          bool copyOk = copyFileVerified(finalPath, backupPath);
          if (copyOk) {
            String bkHeader = readFileHeader(backupPath);
            if (isValidGifHeader(bkHeader)) {
              LittleFS.remove(finalPath);
              uploadDiagBackupRenameOk = true;
            } else {
              LittleFS.remove(backupPath);
            }
          }
        }

        if (!uploadDiagBackupRenameOk) {
          uploadError = true;
          uploadErrorMsg = "Failed to create backup of existing file.";
          Serial.println("[GIF_TXN] UPLOAD_RESULT=FAIL");
          Serial.println("[GIF_TXN] reason=BACKUP_FAIL");
          if (LittleFS.exists(tempPath)) LittleFS.remove(tempPath);
          uploadInProgress = false;
          return;
        }
        needRestore = true;
        Serial.println("[GIF_TXN] BACKUP_CREATED");
      }
    }

    // --- Step 2: Move temp to final ---
    Serial.println("[GIF_TXN] COMMIT_START");
    bool moveOk = LittleFS.rename(tempPath, finalPath);
    Serial.printf("[HTTP] Rename temp->final: %s\n", moveOk ? "ok" : "fail");

    if (!moveOk) {
      Serial.println("[HTTP] Rename temp->final failed. Trying copyFileVerified...");
      moveOk = copyFileVerified(tempPath, finalPath);
      Serial.printf("[HTTP] CopyV temp->final: %s\n", moveOk ? "ok" : "fail");
    }

    // --- Step 3: Post-commit verify with isGifFileValid ---
    size_t finalDiskSize = 0;
    File finalF = LittleFS.open(finalPath, "r");
    if (finalF) {
      finalDiskSize = finalF.size();
      finalF.close();
    }

    Serial.println("[GIF_TXN] POST_COMMIT_DIAG");
    Serial.printf("  expected=%d\n", upload.totalSize);
    Serial.printf("  received=%d\n", uploadReceivedBytes);
    Serial.printf("  temp=%d\n", tempDiskSize);
    Serial.printf("  final=%d\n", finalDiskSize);
    Serial.printf("  trailer=0x%02X\n", trailerByte);

    bool finalValid = moveOk &&
                      (finalDiskSize == tempDiskSize) &&
                      (finalDiskSize == uploadReceivedBytes) &&
                      (finalDiskSize > 0) &&
                      (finalDiskSize <= GIF_MAX_FILE_BYTES) &&
                      frameworkSizeOk &&
                      isGifFileValid(finalPath);

    if (finalValid) {
      Serial.println("[GIF_TXN] BACKUP_DELETE & TEMP_DELETE");
      if (LittleFS.exists(backupPath)) LittleFS.remove(backupPath);
      if (LittleFS.exists(tempPath)) LittleFS.remove(tempPath);
      uploadSuccessPath   = finalPath;
      uploadSuccessSize   = finalDiskSize;
      uploadSuccessHeader = readFileHeader(finalPath);

      Serial.println("[GIF_TXN] UPLOAD_RESULT=SUCCESS");
      Serial.printf("[GIF_TXN] size=%d\n", finalDiskSize);
      Serial.printf("[GIF_TXN] trailer=0x%02X\n", trailerByte);
      Serial.println("[GIF_TXN] postCommit=PASS");
      uploadInProgress = false;
    } else {
      Serial.println("[GIF_TXN] UPLOAD_RESULT=FAIL");
      Serial.println("[GIF_TXN] reason=POST_COMMIT_VERIFY_FAIL");
      Serial.println("[GIF_TXN] RESTORE_START");

      if (needRestore && LittleFS.exists(backupPath)) {
        if (LittleFS.exists(finalPath)) LittleFS.remove(finalPath);
        bool restoreOk = copyFileVerified(backupPath, finalPath);

        if (restoreOk && isGifFileValid(finalPath)) {
          Serial.println("[GIF_TXN] RESTORE_OK — active verified, BACKUP_DELETE");
          if (LittleFS.exists(backupPath)) LittleFS.remove(backupPath);
          if (LittleFS.exists(tempPath)) LittleFS.remove(tempPath);
        } else {
          Serial.println("[GIF_TXN] RESTORE_FAIL — BACKUP_PRESERVED for startup recovery");
          if (LittleFS.exists(finalPath)) LittleFS.remove(finalPath);
        }
      } else {
        if (LittleFS.exists(finalPath)) {
          if (!LittleFS.exists(tempPath)) {
            bool restoreTempOk = LittleFS.rename(finalPath, tempPath);
            if (restoreTempOk) {
              if (isGifFileValid(tempPath)) {
                Serial.println("[GIF_TXN] TEMP_RESTORE_SUCCESS — Preserved valid TEMP via rename");
              } else {
                Serial.println("[GIF_TXN] TEMP_RESTORE_VERIFY_FAIL — Keeping TEMP for recovery");
              }
            } else {
              bool copyTempOk = copyFileVerified(finalPath, tempPath);
              if (copyTempOk && isGifFileValid(tempPath)) {
                LittleFS.remove(finalPath);
                Serial.println("[GIF_TXN] TEMP_RESTORE_SUCCESS — Preserved valid TEMP via copy");
              } else {
                Serial.println("[GIF_TXN] TEMP_RESTORE_FAIL — Retaining files for diagnostics");
              }
            }
          }
        }
      }
      if (LittleFS.exists(tempPath)) LittleFS.remove(tempPath);
      uploadError = true;
      uploadErrorMsg = "Transaction failed post-commit validation. Previous file restored if available.";
      uploadInProgress = false;
    }
  }
}
/**
 * @brief GIF-Specific Atomic Transaction Helper
 * 
 * Contract & Guarantees:
 * 1. Validate source file exists and passes GIF size/header checks.
 * 2. Evaluate existing destination backup (.bak) for recovery safety.
 * 3. Perform safe rename-swap (dst -> dst.bak, tmp -> dst).
 * 4. Execute post-commit validation using isGifFileValid(dst).
 * 5. Rollback automatically if post-commit verification fails.
 * 6. MUST NOT intentionally delete the last available recovery artifact
 *    within the transaction before the new destination is verified.
 * 
 * System-level recovery guarantees are handled by the caller and
 * Startup Recovery layer.
 */
bool copyFileAtomic(const char* src, const char* dst) {
  // 1. Check if source exists and is valid
  if (!LittleFS.exists(src)) {
    Serial.printf("[FS] copyFileAtomic: source file %s does not exist!\n", src);
    return false;
  }
  
  File srcFile = LittleFS.open(src, "r");
  if (!srcFile) {
    Serial.printf("[FS] copyFileAtomic: failed to open source file %s\n", src);
    return false;
  }
  
  size_t srcSize = srcFile.size();
  if (srcSize == 0 || srcSize > GIF_MAX_FILE_BYTES) {
    Serial.printf("[FS] copyFileAtomic: source file %s has invalid size %d (max 200KB)\n", src, srcSize);
    srcFile.close();
    return false;
  }
  
  // Verify source GIF header
  uint8_t header[6];
  if (srcFile.read(header, 6) != 6) {
    Serial.printf("[FS] copyFileAtomic: failed to read header from source file %s\n", src);
    srcFile.close();
    return false;
  }
  
  if (!(header[0] == 'G' && header[1] == 'I' && header[2] == 'F' && 
        header[3] == '8' && (header[4] == '7' || header[4] == '9') && header[5] == 'a')) {
    Serial.printf("[FS] copyFileAtomic: source file %s has invalid GIF header\n", src);
    srcFile.close();
    return false;
  }
  
  // Seek back to start of source file
  srcFile.seek(0);
  
  // Create temporary filename
  String tmpPath = String(dst) + ".tmp";
  
  // Clean stale temp file if it exists
  if (LittleFS.exists(tmpPath)) {
    LittleFS.remove(tmpPath);
  }
  
  File tmpFile = LittleFS.open(tmpPath, "w");
  if (!tmpFile) {
    Serial.printf("[FS] copyFileAtomic: failed to open temp file %s for writing\n", tmpPath.c_str());
    srcFile.close();
    return false;
  }
  
  // Copy loop
  uint8_t buffer[512];
  size_t bytesCopied = 0;
  bool copySuccess = true;
  
  while (srcFile.available() > 0) {
    int bytesRead = srcFile.read(buffer, sizeof(buffer));
    if (bytesRead < 0) {
      Serial.printf("[FS] copyFileAtomic: error reading source file during copy\n");
      copySuccess = false;
      break;
    }
    if (bytesRead > 0) {
      int bytesWritten = tmpFile.write(buffer, bytesRead);
      if (bytesWritten != bytesRead) {
        Serial.printf("[FS] copyFileAtomic: mismatch in written bytes (%d vs %d)\n", bytesWritten, bytesRead);
        copySuccess = false;
        break;
      }
      bytesCopied += bytesWritten;
    }
    yield(); // Let ESP32 background tasks run
  }
  
  srcFile.close();
  tmpFile.close();
  
  if (!copySuccess || bytesCopied != srcSize) {
    Serial.printf("[FS] copyFileAtomic: copy failed or size mismatch (%d copied vs %d source). Cleaning temp file...\n", bytesCopied, srcSize);
    LittleFS.remove(tmpPath);
    return false;
  }
  
  // Double-check temp file validity
  File checkTmp = LittleFS.open(tmpPath, "r");
  if (!checkTmp) {
    Serial.printf("[FS] copyFileAtomic: failed to open check temp file %s\n", tmpPath.c_str());
    LittleFS.remove(tmpPath);
    return false;
  }
  
  size_t checkSize = checkTmp.size();
  if (checkSize != srcSize) {
    Serial.printf("[FS] copyFileAtomic: temp file size check failed (%d vs %d)\n", checkSize, srcSize);
    checkTmp.close();
    LittleFS.remove(tmpPath);
    return false;
  }
  
  uint8_t checkHeader[6];
  if (checkTmp.read(checkHeader, 6) != 6 || 
      !(checkHeader[0] == 'G' && checkHeader[1] == 'I' && checkHeader[2] == 'F' && 
        checkHeader[3] == '8' && (checkHeader[4] == '7' || checkHeader[4] == '9') && checkHeader[5] == 'a')) {
    Serial.printf("[FS] copyFileAtomic: temp file check failed or invalid GIF header\n");
    checkTmp.close();
    LittleFS.remove(tmpPath);
    return false;
  }
  
  checkTmp.close();
  
  // [H1] Safe swap: evaluate existing .bak before removing it — it may be a recovery source
  String dstBak = String(dst) + ".bak";
  bool hadDst = LittleFS.exists(dst);
  if (hadDst) {
    if (LittleFS.exists(dstBak)) {
      bool bakIsValid = isGifFileValid(dstBak);
      bool dstIsValid = isGifFileValid(String(dst));
      if (bakIsValid && !dstIsValid) {
        // [H1] Existing .bak is the ONLY valid recovery source — preserve it, abort transaction
        Serial.printf("[FS] copyFileAtomic: .bak is valid recovery source but dst is invalid. Preserving .bak.\n");
        LittleFS.remove(tmpPath);
        return false;
      }
      // Safe to remove: dst is valid (fresh backup will supersede) OR .bak is invalid (not a source)
      LittleFS.remove(dstBak);
    }
    // Move existing dst aside so we can restore it if commit fails
    if (!LittleFS.rename(dst, dstBak)) {
      Serial.printf("[FS] copyFileAtomic: cannot move dst to .bak — aborting safely, dst intact\n");
      LittleFS.remove(tmpPath);
      return false;
    }
  }

  // Commit: rename tmp -> dst
  if (!LittleFS.rename(tmpPath, dst)) {
    Serial.printf("[FS] copyFileAtomic: rename tmp->dst failed. Restoring dst from .bak\n");
    if (hadDst) LittleFS.rename(dstBak, dst);
    LittleFS.remove(tmpPath);
    return false;
  }

  // [H2] Post-commit verify — rename success != file valid
  if (!isGifFileValid(String(dst))) {
    Serial.printf("[FS] copyFileAtomic: post-commit verify FAILED for %s — restoring .bak\n", dst);
    LittleFS.remove(dst);
    if (hadDst) LittleFS.rename(dstBak, dst);
    return false;
  }

  // All good — clean up the now-redundant backup of old dst
  if (hadDst) LittleFS.remove(dstBak);

  Serial.printf("[FS] copyFileAtomic: successfully copied %s to %s (%d bytes)\n", src, dst, bytesCopied);
  return true;
}

void ensureDefaultActiveGifs() {
  if (!LittleFS.exists("/loop.gif")) {
    if (LittleFS.exists("/defaults/loop.gif")) {
      Serial.println("[FS] /loop.gif missing. Copying /defaults/loop.gif -> /loop.gif...");
      copyFileAtomic("/defaults/loop.gif", "/loop.gif");
    } else {
      Serial.println("[FS] WARNING: /defaults/loop.gif is missing!");
    }
  }
  
  if (!LittleFS.exists("/boot.gif")) {
    if (LittleFS.exists("/defaults/boot.gif")) {
      Serial.println("[FS] /boot.gif missing. Copying /defaults/boot.gif -> /boot.gif...");
      copyFileAtomic("/defaults/boot.gif", "/boot.gif");
    } else {
      Serial.println("[FS] WARNING: /defaults/boot.gif is missing!");
    }
  }
}

// =============================================================================
// GIF Startup Recovery System
// Runs once at boot after LittleFS.mount(), before ensureDefaultActiveGifs().
//
// Inspects and recovers ONLY these 6 paths:
//   /boot.gif            /loop.gif
//   /boot_upload.tmp     /loop_upload.tmp
//   /boot_backup.gif     /loop_backup.gif
//
// NEVER modifies /defaults/boot.gif or /defaults/loop.gif.
// Default restoration remains exclusively the responsibility of ensureDefaultActiveGifs().
//
// Restore method: copyFileVerified(backup -> active) is always used.
// The backup is preserved until the copied active file has been validated.
// rename() is NOT used in a way that destroys the backup before validation.
// If restore or validation fails, the backup is preserved for ensureDefaultActiveGifs().
//
// This function is idempotent: if called again when active GIF is valid, it
// takes no action beyond logging "active valid".
// =============================================================================
void performGifStartupRecovery() {
  Serial.println("[GIF RECOVERY] === Starting GIF startup recovery ===");

  // Process both GIF types
  const char* types[2][3] = {
    { "boot.gif",  "/boot.gif",  "/boot_upload.tmp"  },  // [0]: name, active, tmp
    { "loop.gif",  "/loop.gif",  "/loop_upload.tmp"  },  // [1]: name, active, tmp
  };
  const char* backups[2] = { "/boot_backup.gif", "/loop_backup.gif" };

  for (int i = 0; i < 2; i++) {
    const char* gifName   = types[i][0];
    String activePath     = String(types[i][1]);
    String tmpPath        = String(types[i][2]);
    String backupPath     = String(backups[i]);

    Serial.printf("[GIF RECOVERY] --- Checking %s ---\n", gifName);
    Serial.printf("[GIF RECOVERY] checking %s\n", gifName);

    bool tmpExists    = LittleFS.exists(tmpPath);
    bool activeValid  = isGifFileValid(activePath);
    bool backupValid  = isGifFileValid(backupPath);

    if (tmpExists) {
      // ---------------------------------------------------------------
      // Interrupted upload detected: a .tmp file was left behind
      // ---------------------------------------------------------------
      Serial.println("[GIF RECOVERY] interrupted upload detected (tmp found)");

      if (activeValid) {
        // Active is intact — the tmp is just stale, safe to remove
        Serial.printf("[GIF RECOVERY] active valid, removing stale tmp\n");
        LittleFS.remove(tmpPath);

      } else {
        // Active is missing or corrupt
        Serial.println("[GIF RECOVERY] active invalid/missing");

        if (backupValid) {
          // Restore backup -> active using copyFileVerified so backup stays
          // intact until active has been validated
          Serial.println("[GIF RECOVERY] backup valid, attempting restore");
          Serial.printf("[GIF RECOVERY] restoring backup -> %s\n", activePath.c_str());

          bool copyOk = copyFileVerified(backupPath, activePath);
          if (copyOk && isGifFileValid(activePath)) {
            Serial.println("[GIF RECOVERY] restore successful, removing backup");
            LittleFS.remove(backupPath);
            activeValid = true;  // [Item 2] Mark active valid so stale tmp is cleaned up
          } else {
            // Copy failed or resulting file is invalid — preserve the backup
            Serial.println("[GIF RECOVERY] restore failed, preserving backup for fallback");
            // Remove potentially incomplete active file to avoid confusion
            if (LittleFS.exists(activePath)) LittleFS.remove(activePath);
          }

        } else {
          // Case C: Neither backup nor active is valid
          if (LittleFS.exists(backupPath)) {
            LittleFS.remove(backupPath);  // invalid backup, safe to remove [H8]
          }

          // [H4/A4] Case E: check if tmp can serve as a recovery candidate
          bool tmpValid = isGifFileValid(tmpPath);
          if (tmpValid) {
            Serial.println("[GIF RECOVERY] Case E: tmp valid — attempting safe restore tmp -> active");
            // [H4] Use copyFileVerified (opens in 'w' mode = creates/overwrites — no pre-delete needed)
            // Keep tmp until verify passes so next boot can retry if this fails [H9]
            bool copyOk = copyFileVerified(tmpPath, activePath);
            if (copyOk && isGifFileValid(activePath)) {
              Serial.println("[GIF RECOVERY] Case E: restore SUCCESS — removing tmp");
              LittleFS.remove(tmpPath);  // [H8] only remove after verify passes
            } else {
              // Copy failed or active invalid — KEEP tmp, fallback to defaults
              Serial.println("[GIF RECOVERY] Case E: restore FAILED — keeping tmp, fallback to defaults");
              if (LittleFS.exists(activePath)) LittleFS.remove(activePath); // remove incomplete active
              // tmp preserved — next boot can retry [H9 idempotent]
            }
          } else {
            // tmp is also invalid — [H8] only remove confirmed-invalid tmp
            Serial.println("[GIF RECOVERY] no valid recovery source — fallback to defaults");
            if (LittleFS.exists(tmpPath)) LittleFS.remove(tmpPath);
          }
        }

        // Always remove tmp after processing if still present and we went through backup restore path
        // (Case E keeps tmp until verify, so check again)
        if (LittleFS.exists(tmpPath) && activeValid) {
          // active became valid via backup restore path — tmp is stale
          Serial.println("[GIF RECOVERY] removing stale tmp after backup restore");
          LittleFS.remove(tmpPath);
        }
      }

    } else {
      // ---------------------------------------------------------------
      // No interrupted upload — normal boot or stale backup cleanup
      // ---------------------------------------------------------------
      if (activeValid) {
        // Active is fine — clean up any stale backup (only safe because active is valid)
        Serial.println("[GIF RECOVERY] active valid");
        if (LittleFS.exists(backupPath)) {
          Serial.println("[GIF RECOVERY] removing stale backup");
          LittleFS.remove(backupPath);
        }

      } else {
        // Active is missing or corrupt
        Serial.println("[GIF RECOVERY] active invalid/missing");

        if (backupValid) {
          // Restore backup -> active using copyFileVerified so backup stays
          // intact until active has been validated
          Serial.println("[GIF RECOVERY] backup valid, attempting restore");
          Serial.printf("[GIF RECOVERY] restoring backup -> %s\n", activePath.c_str());

          bool copyOk = copyFileVerified(backupPath, activePath);
          if (copyOk && isGifFileValid(activePath)) {
            Serial.println("[GIF RECOVERY] restore successful, removing backup");
            LittleFS.remove(backupPath);
          } else {
            // Preserve backup — ensureDefaultActiveGifs() will handle final fallback
            Serial.println("[GIF RECOVERY] restore failed, preserving backup for fallback");
            if (LittleFS.exists(activePath)) LittleFS.remove(activePath);
          }

        } else {
          // Case C: No valid recovery source — clean up invalid backup if present
          if (LittleFS.exists(backupPath)) {
            LittleFS.remove(backupPath);  // invalid backup, safe to remove [H8]
          }

          // [H4/A4] Case E: tmp doesn't exist here (tmpExists=false branch),
          // but check if a .tmp file exists under a different scenario (shouldn't normally happen)
          // No tmp recovery possible in this branch — fallback to defaults
          Serial.println("[GIF RECOVERY] no valid recovery source — fallback to defaults");
        }
      }
    }
  }

  Serial.println("[GIF RECOVERY] === GIF startup recovery complete ===");
}

void handleRestoreDefaultLoop() {
  Serial.println("[HTTP] Restoring default loop animation...");
  if (!LittleFS.exists("/defaults/loop.gif")) {
    server.send(400, "application/json; charset=utf-8", "{\"status\":\"error\",\"message\":\"Default loop.gif missing. Please re-upload filesystem image.\"}");
    return;
  }

  if (copyFileAtomic("/defaults/loop.gif", "/loop.gif")) {
    preferences.begin("obd2_dash", false);
    showGifHome = true;
    preferences.putBool("gif_home", true);
    preferences.end();

    if (!chargeAutoBackActive) {
      currentPage = 1;
    }

    if (theme2GifOpen) {
      gif.close();
      theme2GifOpen = false;
    }

    loopGifReloadRequested = true;
    gifHomeNeedClear = true;
    themeChanged = true;
    // showSetupScreen intentionally NOT set here — reload processed after disconnect

    server.send(200, "application/json; charset=utf-8", "{\"status\":\"success\",\"message\":\"Loop GIF restored successfully\"}");
  } else {
    server.send(500, "application/json; charset=utf-8", "{\"status\":\"error\",\"message\":\"Failed to copy default loop.gif to /loop.gif\"}");
  }
}

void handleRestoreDefaultBoot() {
  Serial.println("[HTTP] Restoring default boot animation...");
  if (!LittleFS.exists("/defaults/boot.gif")) {
    server.send(400, "application/json; charset=utf-8", "{\"status\":\"error\",\"message\":\"Default boot.gif missing. Please re-upload filesystem image.\"}");
    return;
  }

  if (copyFileAtomic("/defaults/boot.gif", "/boot.gif")) {
    pendingRestart = true;
    restartRequestTime = millis();
    server.send(200, "application/json; charset=utf-8", "{\"status\":\"success\",\"message\":\"Boot GIF restored successfully. Reboot scheduled.\"}");
  } else {
    server.send(500, "application/json; charset=utf-8", "{\"status\":\"error\",\"message\":\"Failed to copy default boot.gif to /boot.gif\"}");
  }
}

void handleRestoreDefaultAll() {
  Serial.println("[HTTP] Restoring all default animations...");
  bool loopExists = LittleFS.exists("/defaults/loop.gif");
  bool bootExists = LittleFS.exists("/defaults/boot.gif");

  if (!loopExists || !bootExists) {
    server.send(400, "application/json; charset=utf-8", "{\"status\":\"error\",\"message\":\"One or more default GIF files are missing. Please re-upload filesystem image.\"}");
    return;
  }

  bool loopOk = copyFileAtomic("/defaults/loop.gif", "/loop.gif");
  bool bootOk = copyFileAtomic("/defaults/boot.gif", "/boot.gif");

  if (loopOk && bootOk) {
    preferences.begin("obd2_dash", false);
    showGifHome = true;
    preferences.putBool("gif_home", true);
    preferences.end();

    if (!chargeAutoBackActive) {
      currentPage = 1;
    }

    if (theme2GifOpen) {
      gif.close();
      theme2GifOpen = false;
    }

    loopGifReloadRequested = true;
    gifHomeNeedClear = true;
    themeChanged = true;
    // showSetupScreen intentionally NOT set here — reboot is already scheduled

    pendingRestart = true;
    restartRequestTime = millis();
    server.send(200, "application/json; charset=utf-8", "{\"status\":\"success\",\"message\":\"All GIFs restored successfully. Reboot scheduled.\"}");
  } else {
    server.send(500, "application/json; charset=utf-8", "{\"status\":\"error\",\"message\":\"Failed to restore all default GIFs\"}");
  }
}

void handleRevert() {
  handleRestoreDefaultAll();
}

// ============================================================
// TEMPORARY DIAGNOSTIC — GIF LZW Structure Verifier
// Call via HTTP GET /api/gif_diag after uploading a GIF.
// REMOVE after LZW fix is confirmed correct.
// ============================================================
// Struct for GIF diagnostic reporting
struct GifDiagResult {
  bool     headerValid = false;
  bool     logicalScreenValid = false;
  char     headerStr[7] = {0};
  uint16_t canvasW = 0;
  uint16_t canvasH = 0;

  bool     gctPresent = false;
  uint16_t gctColors = 0;
  uint32_t gctBytes = 0;

  bool     duplicateHeaderFound = false;
  uint32_t duplicateHeaderOffset = 0;

  uint32_t frames = 0;
  uint32_t lastFrameOffset = 0;
  uint16_t lastFrameWidth = 0;
  uint16_t lastFrameHeight = 0;

  bool     trailerFound = false;
  uint32_t trailerOffset = 0;
  bool     structureOk = true;

  uint32_t errorOffset = 0;
  uint8_t  errorByte = 0;
  const char* errorReason = "NONE";
};

void handleGifDiag() {
  const char* path = "/loop.gif";
  GifDiagResult diag;

  if (!LittleFS.exists(path)) {
    Serial.println("[GIF_DIAG] file=/loop.gif NOT FOUND");
    server.send(404, "text/plain", "loop.gif not found");
    return;
  }

  File f = LittleFS.open(path, "r");
  if (!f) {
    Serial.println("[GIF_DIAG] OPEN FAILED");
    server.send(500, "text/plain", "open failed");
    return;
  }

  // --- File Identity ---
  size_t fileSize = f.size();
  Serial.printf("[GIF_DIAG] activeFile=/loop.gif\n");
  Serial.printf("[GIF_DIAG] fileSize=%u\n", (unsigned)fileSize);
  Serial.printf("[GIF_DIAG] freeHeap_before=%u\n", (unsigned)ESP.getFreeHeap());

  // --- Parse GIF Header (6 bytes: GIF87a or GIF89a) ---
  uint8_t hdr[6];
  if (f.read(hdr, 6) != 6) {
    Serial.println("[GIF_DIAG] ERROR: invalid GIF header read");
    diag.errorReason = "INVALID_HEADER_READ";
    diag.errorOffset = 0;
    diag.structureOk = false;
    f.close(); server.send(200, "text/plain", "ERROR: invalid header"); return;
  }

  memcpy(diag.headerStr, hdr, 6);
  diag.headerStr[6] = '\0';
  diag.headerValid = (memcmp(hdr, "GIF87a", 6) == 0) || (memcmp(hdr, "GIF89a", 6) == 0);
  if (!diag.headerValid) {
    Serial.printf("[GIF_DIAG] ERROR: invalid GIF version '%.6s'\n", (char*)hdr);
    diag.errorReason = "INVALID_VERSION";
    diag.errorOffset = 0;
    diag.errorByte = hdr[0];
    diag.structureOk = false;
    f.close(); server.send(200, "text/plain", "ERROR: invalid version"); return;
  }
  Serial.printf("[GIF_DIAG] header=%.6s PASS\n", diag.headerStr);

  // --- Logical Screen Descriptor (7 bytes) ---
  uint8_t lsd[7];
  if (f.read(lsd, 7) != 7) {
    Serial.println("[GIF_DIAG] ERROR: truncated LSD");
    diag.errorReason = "TRUNCATED_LSD";
    diag.errorOffset = 6;
    diag.structureOk = false;
    f.close(); server.send(200, "text/plain", "ERROR: LSD"); return;
  }
  diag.canvasW = lsd[0] | (lsd[1] << 8);
  diag.canvasH = lsd[2] | (lsd[3] << 8);

  uint8_t  packed = lsd[4];
  diag.gctPresent   = (packed >> 7) & 1;
  uint8_t  gctSizeBits = packed & 0x07;
  diag.gctColors    = diag.gctPresent ? (2 << gctSizeBits) : 0;
  diag.gctBytes     = diag.gctPresent ? 3 * (size_t)diag.gctColors : 0;

  diag.logicalScreenValid = (diag.canvasW > 0 && diag.canvasH > 0 &&
                             diag.canvasW == 320 && diag.canvasH == 172);

  Serial.printf("[GIF_DIAG] logicalScreen=%ux%u %s hasGCT=%d gctEntries=%d\n",
                diag.canvasW, diag.canvasH, diag.logicalScreenValid ? "DIMS_OK" : "DIMS_FAIL",
                (int)diag.gctPresent, diag.gctColors);

  if (!diag.logicalScreenValid) {
    diag.structureOk = false;
    diag.errorReason = "INVALID_LSD_DIMS";
    diag.errorOffset = 6;
  }

  // Skip GCT
  if (diag.gctPresent) {
    size_t newPos = f.position() + diag.gctBytes;
    if (newPos > fileSize || !f.seek(newPos)) {
      Serial.println("[GIF_DIAG] ERROR: GCT exceeds EOF or seek failed");
      diag.errorReason = "GCT_OVERRUN";
      diag.errorOffset = 13;
      diag.structureOk = false;
      f.close(); server.send(200, "text/plain", "ERROR: GCT"); return;
    }
  }

  // --- Parse Blocks ---
  while (f.available()) {
    uint32_t blockStartPos = (uint32_t)f.position();
    uint8_t blockByte;
    if (f.read(&blockByte, 1) != 1) break;

    if (blockByte == 0x3B) {
      diag.trailerFound = true;
      diag.trailerOffset = blockStartPos;
      if (f.position() != fileSize) {
        Serial.printf("[GIF_DIAG] ERROR: bytes after trailer=%u\n",
                      (unsigned)(fileSize - f.position()));
        diag.structureOk = false;
        diag.errorReason = "BYTES_AFTER_TRAILER";
        diag.errorOffset = (uint32_t)f.position();
      } else {
        Serial.printf("[GIF_DIAG] trailer=0x3B PASS at offset=%u (EOF)\n", diag.trailerOffset);
      }
      break;
    }

    if (blockByte == 0x21) {
      // Extension: skip label + sub-blocks with explicit terminator check
      uint8_t extLabel;
      if (f.read(&extLabel, 1) != 1) {
        diag.structureOk = false;
        diag.errorReason = "TRUNCATED_EXT";
        diag.errorOffset = blockStartPos;
        break;
      }

      uint8_t subLen;
      bool extTerminated = false;
      while (f.read(&subLen, 1) == 1) {
        if (subLen == 0) {
          extTerminated = true;
          break;
        }
        size_t newPos = f.position() + subLen;
        if (newPos > fileSize || !f.seek(newPos)) {
          Serial.println("[GIF_DIAG] ERROR: extension sub-block exceeds EOF");
          diag.structureOk = false;
          diag.errorReason = "EXT_NO_TERMINATOR";
          diag.errorOffset = blockStartPos;
          break;
        }
      }

      if (!extTerminated) {
        Serial.println("[GIF_DIAG] ERROR: extension missing 0x00 terminator");
        diag.structureOk = false;
        diag.errorReason = "EXT_NO_TERMINATOR";
        diag.errorOffset = blockStartPos;
        break;
      }
      continue;
    }

    if (blockByte == 0x2C) {
      // --- Image Descriptor (9 bytes after separator) ---
      diag.lastFrameOffset = blockStartPos;
      uint8_t id[9];
      if (f.read(id, 9) != 9) {
        Serial.printf("[GIF_DIAG] frame=%d ERROR: truncated Image Descriptor\n", diag.frames);
        diag.structureOk = false;
        diag.errorReason = "TRUNCATED_ID";
        diag.errorOffset = blockStartPos;
        break;
      }

      // Dimensions from Image Descriptor
      uint16_t frmW = id[4] | (id[5] << 8);
      uint16_t frmH = id[6] | (id[7] << 8);
      diag.lastFrameWidth = frmW;
      diag.lastFrameHeight = frmH;
      bool dimOk = (frmW == 320 && frmH == 172);

      uint8_t idPacked     = id[8];
      bool    hasLCT       = (idPacked >> 7) & 1;
      uint8_t lctSizeBits  = idPacked & 0x07;
      int     lctEntries   = hasLCT ? (2 << lctSizeBits) : 0;
      size_t  lctBytes     = hasLCT ? 3 * (size_t)lctEntries : 0;

      int effectiveEntries = hasLCT ? lctEntries : diag.gctColors;

      Serial.printf("[GIF_DIAG] frame=%d width=%u height=%u %s\n",
                    diag.frames, frmW, frmH, dimOk ? "DIMS_OK" : "DIMS_FAIL");
      Serial.printf("[GIF_DIAG] frame=%d hasLCT=%d colorTableEntries=%d\n",
                    diag.frames, (int)hasLCT, effectiveEntries);

      // Skip LCT
      if (hasLCT) {
        size_t newPos = f.position() + lctBytes;
        if (newPos > fileSize || !f.seek(newPos)) {
          Serial.printf("[GIF_DIAG] frame=%d ERROR: LCT exceeds EOF\n", diag.frames);
          diag.structureOk = false;
          diag.errorReason = "LCT_OVERRUN";
          diag.errorOffset = (uint32_t)f.position();
          break;
        }
      }

      // --- LZW Minimum Code Size byte ---
      uint8_t lzwMinCodeSize;
      if (f.read(&lzwMinCodeSize, 1) != 1) {
        Serial.printf("[GIF_DIAG] frame=%d ERROR: missing LZW byte\n", diag.frames);
        diag.structureOk = false;
        diag.errorReason = "MISSING_LZW_BYTE";
        diag.errorOffset = (uint32_t)f.position();
        break;
      }

      bool lzwRangeOk = (lzwMinCodeSize >= 2 && lzwMinCodeSize <= 8);
      if (!lzwRangeOk) {
        Serial.printf("[GIF_DIAG] frame=%d ERROR: invalid LZW min code size=%d\n",
                      diag.frames, (int)lzwMinCodeSize);
        diag.structureOk = false;
        diag.errorReason = "LZW_RANGE";
        diag.errorOffset = (uint32_t)f.position() - 1;
      }

      // Informational check only — does not mark structureOk = false
      int expectedLZW = 2;
      if (effectiveEntries > 2) {
        int bits = 0, v = effectiveEntries - 1;
        while (v > 0) { bits++; v >>= 1; }
        expectedLZW = (bits < 2) ? 2 : bits;
      }
      bool lzwMatch = lzwRangeOk && ((int)lzwMinCodeSize == expectedLZW);
      Serial.printf("[GIF_DIAG] frame=%d colorTableEntries=%d expectedLZW=%d actualLZW=%d %s (INFO)\n",
                    diag.frames, effectiveEntries, expectedLZW,
                    (int)lzwMinCodeSize, lzwMatch ? "MATCH" : "DIFF");

      if (!dimOk) {
        diag.structureOk = false;
        if (diag.errorReason[0] == '\0' || strcmp(diag.errorReason, "NONE") == 0) {
          diag.errorReason = "FRAME_DIMS_FAIL";
          diag.errorOffset = blockStartPos;
        }
      }

      // --- Sub-block chain verification ---
      size_t imageDataStart = f.position();
      size_t imageDataBytes = 0;
      int    subBlockCount  = 0;
      bool   eofError       = false;
      bool   imgTerminated  = false;
      uint8_t subLen;

      while (f.read(&subLen, 1) == 1) {
        if (subLen == 0) {
          imgTerminated = true;
          break;  // Block terminator 0x00
        }
        size_t newPos = f.position() + subLen;
        if (newPos > fileSize) {
          Serial.printf("[GIF_DIAG] frame=%d ERROR: sub-block exceeds EOF (pos=%u subLen=%u fileSize=%u)\n",
                        diag.frames, (unsigned)f.position(), subLen, (unsigned)fileSize);
          eofError = true;
          diag.structureOk = false;
          diag.errorReason = "SUBBLOCK_OVERRUN";
          diag.errorOffset = (uint32_t)f.position();
          break;
        }
        if (!f.seek(newPos)) {
          Serial.printf("[GIF_DIAG] frame=%d ERROR: sub-block seek failed\n", diag.frames);
          eofError = true;
          diag.structureOk = false;
          diag.errorReason = "SUBBLOCK_SEEK_FAIL";
          diag.errorOffset = (uint32_t)f.position();
          break;
        }
        imageDataBytes += subLen;
        subBlockCount++;
      }

      if (!imgTerminated) {
        Serial.printf("[GIF_DIAG] frame=%d ERROR: image data missing 0x00 terminator\n", diag.frames);
        diag.structureOk = false;
        diag.errorReason = "IMG_NO_TERMINATOR";
        diag.errorOffset = (uint32_t)f.position();
      } else if (!eofError) {
        Serial.printf("[GIF_DIAG] frame=%d imageDataStart=%u subBlocks=%d imageDataBytes=%u terminator=0x00 PASS\n",
                      diag.frames, (unsigned)imageDataStart,
                      subBlockCount, (unsigned)imageDataBytes);
      }

      diag.frames++;
      continue;
    }

    // --- Unknown Block Byte Handler ---
    // Check if this byte 'G' at block position starts a duplicate header "GIF87a" or "GIF89a"
    if (blockByte == 'G') {
      uint8_t peek[5];
      size_t peekPos = f.position(); // Position right after 'G'
      if (f.read(peek, 5) == 5) {
        if (peek[0] == 'I' && peek[1] == 'F' && peek[2] == '8' &&
            (peek[3] == '7' || peek[3] == '9') && peek[4] == 'a') {
          diag.duplicateHeaderFound = true;
          diag.duplicateHeaderOffset = blockStartPos;
          Serial.printf("[GIF_DIAG] DUPLICATE_HEADER_FOUND at duplicateHeaderOffset=%u (GIF8%ca)\n",
                        diag.duplicateHeaderOffset, peek[3]);
        }
        f.seek(peekPos); // Restore file position
      }
    }

    Serial.printf("[GIF_DIAG] ERROR: unknown block 0x%02X at pos=%u\n",
                  blockByte, blockStartPos);
    diag.structureOk = false;
    diag.errorByte = blockByte;
    diag.errorOffset = blockStartPos;
    diag.errorReason = diag.duplicateHeaderFound ? "DUPLICATE_HEADER" : "INVALID_BLOCK";
    break;
  }

  // Final structural checks
  if (!diag.trailerFound) {
    Serial.println("[GIF_DIAG] ERROR: missing GIF trailer 0x3B");
    diag.structureOk = false;
    if (strcmp(diag.errorReason, "NONE") == 0) {
      diag.errorReason = "MISSING_TRAILER";
      diag.errorOffset = (uint32_t)fileSize;
    }
  }

  if (diag.frames == 0) {
    Serial.println("[GIF_DIAG] ERROR: no image frames found");
    diag.structureOk = false;
    if (strcmp(diag.errorReason, "NONE") == 0) {
      diag.errorReason = "NO_FRAMES";
    }
  }

  f.close();
  Serial.printf("[GIF_DIAG] totalFrames=%d trailerFound=%s structureOk=%s errorReason=%s\n",
                diag.frames, diag.trailerFound ? "YES" : "NO",
                diag.structureOk ? "YES" : "NO", diag.errorReason);
  Serial.printf("[GIF_DIAG] freeHeap_after=%u\n", (unsigned)ESP.getFreeHeap());

  String resp = "activeFile=/loop.gif\n"
                "fileSize=" + String(fileSize) + "\n"
                "header=" + String(diag.headerStr) + "\n"
                "canvas=" + String(diag.canvasW) + "x" + String(diag.canvasH) + "\n"
                "gctPresent=" + String(diag.gctPresent ? "YES" : "NO") + "\n"
                "gctColors=" + String(diag.gctColors) + "\n"
                "gctBytes=" + String(diag.gctBytes) + "\n"
                "duplicateHeaderFound=" + String(diag.duplicateHeaderFound ? "YES" : "NO") + "\n"
                "duplicateHeaderOffset=" + (diag.duplicateHeaderFound ? String(diag.duplicateHeaderOffset) : "NONE") + "\n"
                "frames=" + String(diag.frames) + "\n"
                "lastFrameOffset=" + (diag.frames > 0 ? String(diag.lastFrameOffset) : "NONE") + "\n"
                "lastFrameSize=" + (diag.frames > 0 ? (String(diag.lastFrameWidth) + "x" + String(diag.lastFrameHeight)) : "NONE") + "\n"
                "trailerFound=" + String(diag.trailerFound ? "YES" : "NO") + "\n"
                "trailerOffset=" + (diag.trailerFound ? String(diag.trailerOffset) : "NONE") + "\n"
                "structureOk=" + String(diag.structureOk ? "YES" : "NO") + "\n"
                "errorOffset=" + (diag.structureOk ? "NONE" : String(diag.errorOffset)) + "\n"
                "errorByte=" + (diag.structureOk ? "NONE" : ("0x" + String(diag.errorByte, HEX))) + "\n"
                "errorReason=" + String(diag.errorReason);

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/plain", resp);
}
