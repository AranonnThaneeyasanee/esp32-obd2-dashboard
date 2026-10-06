#pragma once

#include <cmath>
#include <cctype>

// ============================================================
// Energy Source Selection and Trip Calculation Helpers
// ============================================================
#define ENERGY_MAX_INTEGRATION_GAP_MS 3500
#define HV_TELEMETRY_FRESH_TIMEOUT_MS 3500
#define HV_PAIR_MAX_SKEW_MS           1500

static uint32_t hvCurrentSampleSeq = 0; // Sequence ID: increments ONLY on valid VP_PID_HV_CURRENT decode
static bool hasIntegratedCurrentSample = false;
static uint32_t lastIntegratedCurrentSeq = 0;

inline bool isHvVoltageFresh(uint32_t now) {
  return hvVoltageValid && (hvVoltageLastUpdateMs > 0) &&
         ((uint32_t)(now - hvVoltageLastUpdateMs) <= HV_TELEMETRY_FRESH_TIMEOUT_MS);
}

inline bool isHvCurrentFresh(uint32_t now) {
  return hvCurrentValid && (hvCurrentLastUpdateMs > 0) &&
         ((uint32_t)(now - hvCurrentLastUpdateMs) <= HV_TELEMETRY_FRESH_TIMEOUT_MS);
}

// Unified HV Energy Readiness: single source of truth for both AUTO status & Integration
inline bool isHvEnergyReady(uint32_t now, String &outReason) {
  if (!isHvVoltageFresh(now) && !isHvCurrentFresh(now)) {
    outReason = "hv_voltage_and_current_stale";
    return false;
  }
  if (!isHvVoltageFresh(now)) {
    outReason = "hv_voltage_stale";
    return false;
  }
  if (!isHvCurrentFresh(now)) {
    outReason = "hv_current_stale";
    return false;
  }

  uint32_t pairSkewMs = (hvVoltageLastUpdateMs > hvCurrentLastUpdateMs)
                        ? (hvVoltageLastUpdateMs - hvCurrentLastUpdateMs)
                        : (hvCurrentLastUpdateMs - hvVoltageLastUpdateMs);
  if (pairSkewMs > HV_PAIR_MAX_SKEW_MS) {
    outReason = "hv_pair_skew";
    return false;
  }

  if (!std::isfinite(hvBatteryVoltage) || !std::isfinite(hvBatteryCurrent) ||
      (hvBatteryVoltage < 200.0f || hvBatteryVoltage > 500.0f) ||
      (hvBatteryCurrent < -400.0f || hvBatteryCurrent > 500.0f)) {
    outReason = "input_sanity_failed";
    return false;
  }

  float pKw = (hvBatteryVoltage * hvBatteryCurrent) / 1000.0f;
  if (!std::isfinite(pKw) || std::abs(pKw) > 250.0f) {
    outReason = "power_sanity_failed";
    return false;
  }

  outReason = "ok";
  return true;
}

inline bool isHvEnergyReady(uint32_t now) {
  String dummy;
  return isHvEnergyReady(now, dummy);
}

inline bool isHvPowerReady() {
  return isHvEnergyReady(millis());
}

// ── Telemetry Freshness Helpers ──────────────────────────────────────────────
// Explicitly bound to OBD_HEALTH_FRESH_TIMEOUT_MS (3000ms).
// Pure telemetry evaluation: decoupled from UI currentState to prevent page switches
// from corrupting transport health.
inline bool isObdDataFreshAt(uint32_t now) {
  if (obdLastGoodFrameMs == 0) return false;
  return (uint32_t)(now - obdLastGoodFrameMs) < OBD_HEALTH_FRESH_TIMEOUT_MS;
}

inline bool isObdDataFresh() {
  return isObdDataFreshAt(millis());
}

// Tracks whether TCP/ELM recovery is awaiting genuinely new post-recovery telemetry
static bool obdAwaitingFreshTelemetryAfterRecovery = false;

// Snapshot of obdLastGoodFrameMs at recovery entry; used to detect new frames from ANY source (including VP_PID_SOC)
static uint32_t lastKnownTelemetryTimestampAtRecovery = 0;

// Ensure valid telemetry timestamp is non-zero (sentinel 0 is reserved for 'no valid frame ever')
inline void recordValidObdFrame() {
  uint32_t frameNow = millis();
  obdLastGoodFrameMs = (frameNow == 0) ? 1 : frameNow;
  obdAwaitingFreshTelemetryAfterRecovery = false; // Fresh post-recovery frame confirmed
}

// ── OBD Health State Computation ─────────────────────────────────────────────
// Called ONLY from handleConnection(). Single writer of obdHealthState.
// Pure evaluator: captures now = millis() once, modifies no state, runs no mutations.
// Precedence ensures stale data > 5000ms strictly yields DISCONNECTED.
//
// EXACT MATHEMATICAL BOUNDARY CONTRACT:
// • Age < 3000ms:          FRESH (CONNECTED if full stack healthy)
// • 3000ms <= Age <= 5000ms: WARNING (telemetry stale within grace)
// • Age > 5000ms:          DISCONNECTED (grace window expired) [STRICT: > 5000, never >=]
// • obdLastGoodFrameMs == 0: DISCONNECTED (no valid telemetry ever received)
inline ObdHealthState getObdHealthState() {
  const uint32_t now = millis();

  bool wifiOk    = (WiFi.status() == WL_CONNECTED);
  bool tcpOk     = (currentTcpState == TCP_STATE_ACTIVE) && obdClient.connected();
  bool elmOk     = (currentElmState == ELM_INIT_CONFIRMED);
  bool dataFresh = isObdDataFreshAt(now); // Exact single timestamp snapshot

  // The timestamp-change detector may clear the gate only when obdLastGoodFrameMs has advanced
  // from the recovery snapshot due to an already-validated telemetry frame.
  // It must never clear the gate merely because TCP/ELM state changed.
  if (obdAwaitingFreshTelemetryAfterRecovery && (obdLastGoodFrameMs != lastKnownTelemetryTimestampAtRecovery)) {
    obdAwaitingFreshTelemetryAfterRecovery = false;
  }

  // 1. CONNECTED: all 5 conditions met (age < 3000ms) AND not awaiting fresh telemetry post-recovery
  if (wifiOk && tcpOk && elmOk && obdConnected && dataFresh && !obdAwaitingFreshTelemetryAfterRecovery) {
    return OBD_HEALTH_CONNECTED;
  }

  // 2. No Wi-Fi path
  if (!wifiOk) {
    // CONNECTING is reserved STRICTLY for initial boot bring-up before first connection.
    // Once wifiEverConnected == true, any Wi-Fi drop is immediately DISCONNECTED (RED).
    if (!wifiEverConnected &&
        (currentStaState == STA_STATE_CONNECTING || currentStaState == STA_STATE_IDLE)) {
      return OBD_HEALTH_CONNECTING;
    }
    // Post-connection drop, BACKOFF, WAIT_FOR_ADAPTER, SCANNING -> DISCONNECTED (RED fast blink)
    return OBD_HEALTH_DISCONNECTED;
  }

  // Paths below: wifiOk == true

  // 3. Never received a valid frame during current runtime -> DISCONNECTED
  // (Contract: obdLastGoodFrameMs == 0 means no frame ever received, cannot qualify for grace)
  // Evaluated BEFORE TCP/ELM recovery checks so lack of valid telemetry is never masked.
  if (obdLastGoodFrameMs == 0) {
    return OBD_HEALTH_DISCONNECTED;
  }

  const uint32_t dataAge = (uint32_t)(now - obdLastGoodFrameMs);

  // 4. Stale data > grace window (>5000ms) -> DISCONNECTED
  // MANDATORY CONTRACT: Strictly > OBD_HEALTH_WARNING_DATA_GRACE_MS, NEVER >=
  // Evaluated BEFORE TCP/ELM recovery checks so stale data > 5s is never masked as WARNING.
  if (dataAge > OBD_HEALTH_WARNING_DATA_GRACE_MS) {
    return OBD_HEALTH_DISCONNECTED;
  }

  // 5. Initial bring-up only: Wi-Fi up, TCP not yet opened -> CONNECTING
  // Strictly reserved for the pre-first-GOT_IP boot phase (!wifiEverConnected).
  // Post-GOT_IP reassociations transition through WARNING or DISCONNECTED.
  if (!wifiEverConnected &&
      currentTcpState == TCP_STATE_IDLE &&
      currentElmState == ELM_INIT_IDLE) {
    return OBD_HEALTH_CONNECTING;
  }

  // 6a. Explicit Telemetry Grace Window (3000ms <= age <= 5000ms) OR awaiting fresh post-recovery telemetry -> WARNING
  if ((dataAge >= OBD_HEALTH_FRESH_TIMEOUT_MS && dataAge <= OBD_HEALTH_WARNING_DATA_GRACE_MS) ||
      (obdAwaitingFreshTelemetryAfterRecovery && dataAge <= OBD_HEALTH_WARNING_DATA_GRACE_MS)) {
    return OBD_HEALTH_WARNING;
  }

  // 6b. Explicit TCP/ELM Handshake Recovery -> WARNING
  if (currentTcpState == TCP_STATE_CONNECTING     ||
      currentTcpState == TCP_STATE_RECONNECT_WAIT ||
      currentElmState != ELM_INIT_CONFIRMED       ||
      obdConnecting) {
    return OBD_HEALTH_WARNING;
  }

  // 7. Fallback -> DISCONNECTED
  return OBD_HEALTH_DISCONNECTED;
}

// ── Channel Sync Health Gate ──────────────────────────────────────────────────
// Used only by checkChannelHarmonization() to determine if AP sync is safe.
// All 5 conditions required, including isObdDataFresh(), so a link that just
// became stale is still protected from AP-side channel disruption.
bool staHealthyForChannelSync() {
  return (WiFi.status() == WL_CONNECTED)         &&
         (currentTcpState == TCP_STATE_ACTIVE)   &&
         (currentElmState == ELM_INIT_CONFIRMED) &&
         obdConnected                            &&
         isObdDataFresh();
}

// ============================================================
// BYD Dolphin SOC Resolver — SOC V1.0
//
// Single responsibility:
//   validate source → check freshness → resolve → write batt_soc
//
// [R12] V1 HARD LOCK: VP_PID_SOH/detailed source is NOT active.
//   This resolver has NO executable path for SOC_SOURCE_BMS_DETAILED.
//   The detailedFresh branch has been removed. It will be added back
//   only after VP_PID_SOH byte mapping is confirmed from real vehicle logs.
//
// [R13] Resolver Execution Guarantee:
//   Called unconditionally as the FIRST statement of updateObdPollingEngine()
//   (before upload / state guards) AND immediately after VP_PID_SOC parser block.
//   Guarantees freshness expiration is evaluated even if step 14 is not reached.
//
// [R14.1] Final SOC Write Ownership:
//   batt_soc, socFinalValid, socSource, socFinalLastUpdateMs, socSourceDifference
//   are written EXCLUSIVELY by this function in SOC V1.0.
//
// [R15] Resolver Idempotency:
//   This function is safe to call multiple times per cycle. It does NOT mutate
//   socCoarseLastUpdateMs, socCoarseValid, or socBmsCoarse.
//
// [R16] No Source Timestamp Mutation:
//   This function MUST NEVER assign to socCoarseLastUpdateMs,
//   socDetailedLastUpdateMs, or obdLastGoodFrameMs.
//
// NO SILENT FALLBACK RULE (R4):
//   If VP_PID_SOC is unavailable or expired:
//     → NO voltage estimation, NO current estimation, NO energy estimation
//     → NO previous-value hold beyond timeout, NO averaging/interpolation
//     → SOC = INVALID, batt_soc = -1.0f
//
// MUST NOT: touch Wi-Fi, Energy, RPM, Speed, Charging, NVS,
//           OBD polling sequence, or any other subsystem.
// ============================================================
inline void resolveBydSoc() {
    uint32_t now = millis();

    // --- Freshness check ---
    // [R7]  socCoarseValid = "last parsed sample was valid" — NOT "currently fresh".
    //       Do NOT mutate socCoarseValid to false here.
    // [R11] (uint32_t) subtraction is rollover-safe for millis() (~49.7-day wrap)
    // Boundary: <= 15000ms is fresh, > 15000ms is stale
    bool coarseFresh = socCoarseValid
                    && ((uint32_t)(now - socCoarseLastUpdateMs) <= SOC_DATA_TIMEOUT_MS);

    // [R12] No detailedFresh check in V1. socDetailedValid = false always.

    // --- Cross-check diagnostic: reset to -1.0f in V1 ---
    socSourceDifference = -1.0f; // not applicable in V1 (no detailed source)

    // --- Resolution priority — V1 uses only coarse source ---
    // [R3] Defensive range invariant: re-validates even if parser already did.
    //      Catches: bit corruption, NaN from future float arithmetic, etc.
    auto isValidSocValue = [](float v) -> bool {
        return std::isfinite(v) && v >= 0.0f && v <= 100.0f;
    };

    if (coarseFresh && isValidSocValue(socBmsCoarse)) {
        batt_soc             = socBmsCoarse;
        socSource            = SOC_SOURCE_BMS_COARSE;
        socFinalValid        = true;
        socFinalLastUpdateMs = socCoarseLastUpdateMs; // source timestamp, not now (R2/R8/R16)
    } else {
        // No valid, fresh, in-range BMS SOC — NO SILENT FALLBACK (R4)
        batt_soc      = -1.0f;
        socSource     = SOC_SOURCE_NONE;
        socFinalValid = false;
        // [R8] socFinalLastUpdateMs: retain last known value — do NOT reset to 0.
        //      It records when the last valid SOC was received.
        //      soc_valid=false is the authority for current freshness.
    }

    // --- Rate-limited diagnostic log (~2 seconds) ---
    static uint32_t lastSocLogMs = 0;
    if ((uint32_t)(now - lastSocLogMs) >= 2000) {
        lastSocLogMs = now;
        if (socFinalValid) {
            Serial.printf(
                "[SOC] coarse=%.2f detailed=INVALID diff=N/A source=VP_PID_SOC valid=1 final=%.2f\n",
                socBmsCoarse, batt_soc);
        } else {
            Serial.printf(
                "[SOC] coarse=%s detailed=INVALID source=NONE valid=0 final=INVALID\n",
                (socCoarseValid && !coarseFresh) ? "STALE" : "INVALID");
        }
    }
}

inline void updateEnergySourceAUTO() {
  uint32_t now = millis();

  // Mode-Aware Status Isolation: evaluate authority strictly according to active mode
  if (energySourceMode == 0) {
    // Mode 0: AUTO (HV V×I Integration is sole authority)
    String readinessReason;
    bool hvReady = isHvEnergyReady(now, readinessReason);
    if (hvReady) {
      energySourceLabel = "HV_POWER_INTEGRATION";
      energyValid = true;
      energyError = "ok";
    } else {
      energySourceLabel = "NO_DATA";
      energyValid = false;
      energyError = readinessReason;
    }
  } else if (energySourceMode == 1) {
    // Mode 1: Manual BMS VP_PID_ACC_DISCHARGE Accumulated Discharge
    if (accumulatedDischargeValidated && accumulatedDischargeNow > 0.0f && accumulatedDischargeStart >= 0.0f) {
      energySourceLabel = "BMS_ACCUMULATED";
      energyValid = true;
      energyError = "ok";
    } else {
      energySourceLabel = "BMS_ACCUMULATED_CANDIDATE";
      energyValid = false;
      energyError = "not_validated_yet";
    }
  } else if (energySourceMode == 2) {
    // Mode 2: Manual HV V×I Integration
    String readinessReason;
    bool hvReady = isHvEnergyReady(now, readinessReason);
    if (hvReady) {
      energySourceLabel = "HV_POWER_INTEGRATION";
      energyValid = true;
      energyError = "ok";
    } else {
      energySourceLabel = "NO_DATA";
      energyValid = false;
      energyError = readinessReason;
    }
  } else if (energySourceMode == 3) {
    // Mode 3: Manual VP_PID_ENERGY Fallback (Raw counter source without timestamp in existing firmware)
    energySourceLabel = "22000E_FALLBACK";
    energyValid = (energy22000Eraw > 0.0f);
    energyError = energyValid ? "ok" : "no_data_22000e";
  } else {
    energySourceLabel = "OFF_DEBUG";
    energyValid = false;
    energyError = "mode_off";
  }
}

inline void updateHvPowerAndTripEnergy() {
  // 1. Mode Authority Guard: Mode 0 (AUTO) and Mode 2 (Manual HV) ONLY
  // Mode 1 and Mode 3 strictly abort without touching HV readiness or baseline
  if (!(energySourceMode == 0 || energySourceMode == 2)) {
    integrationAllowed = false;
    integrationSkipReason = "wrong_mode";
    lastEnergyUpdateMs = 0;
    return;
  }

  // 2. Unconditional Charging Active Guard: Type B Baseline Invalidation
  // Must NOT be masked or overridden by readinessReason!
  if (chargeAutoBackActive) {
    integrationAllowed = false;
    integrationSkipReason = "charging_active";
    lastEnergyUpdateMs = 0;
    return;
  }

  uint32_t nowMs = millis();

  // Update data ages for debug visibility
  hvVoltageAgeMs = (hvVoltageLastUpdateMs > 0) ? (nowMs - hvVoltageLastUpdateMs) : 999999;
  hvCurrentAgeMs = (hvCurrentLastUpdateMs > 0) ? (nowMs - hvCurrentLastUpdateMs) : 999999;

  // 3. Unified HV Readiness Evaluation
  String readinessReason;
  bool ready = isHvEnergyReady(nowMs, readinessReason);
  if (!ready) {
    integrationAllowed = false;
    integrationSkipReason = readinessReason;
    // Invariant 4: Failure path must NEVER establish a baseline timestamp
    lastEnergyUpdateMs = 0;
    return;
  }

  // 4. Internal Counter State Validity Guard (Type B Reset)
  if (!std::isfinite(tripKwhUsed) || tripKwhUsed < 0.0f) {
    integrationAllowed = false;
    integrationSkipReason = "trip_used_state_invalid";
    lastEnergyUpdateMs = 0;
    return;
  }
  if (!std::isfinite(tripRegenKwh) || tripRegenKwh < 0.0f) {
    integrationAllowed = false;
    integrationSkipReason = "trip_regen_state_invalid";
    lastEnergyUpdateMs = 0;
    return;
  }

  // 5. Power Calculation & Direction Split
  rawPowerKw = (hvBatteryVoltage * hvBatteryCurrent) / 1000.0f;
  dischargePowerKw = (rawPowerKw > 0.0f) ? rawPowerKw : 0.0f;
  regenPowerKw = (rawPowerKw < 0.0f) ? -rawPowerKw : 0.0f;
  batteryPowerKw = dischargePowerKw - regenPowerKw;
  instantPowerKw = batteryPowerKw;

  // 6. Sequence Duplicate Guard: Strictly precedes Session Start & Baseline Init!
  if (hasIntegratedCurrentSample && hvCurrentSampleSeq == lastIntegratedCurrentSeq) {
    integrationAllowed = false;
    integrationSkipReason = "duplicate_current_sample";
    return;
  }
  lastIntegratedCurrentSeq = hvCurrentSampleSeq;
  hasIntegratedCurrentSample = true;

  // 7. Baseline Initialization (First unique sample establishes time baseline without integrating)
  if (lastEnergyUpdateMs == 0) {
    lastEnergyUpdateMs = nowMs;
    integrationAllowed = false;
    integrationSkipReason = "initialize_baseline";
    lastEnergyIntegrationDtMs = 0;
    if (!energySessionStarted) {
      energySessionStarted = true;
      Serial.printf("[ENERGY] Trip energy session started from valid HV telemetry: V=%.1fV I=%.1fA P=%.3fkW\n",
                    hvBatteryVoltage, hvBatteryCurrent, rawPowerKw);
    }
    return;
  }

  // 8. Delta-t Validation (Explicit dt_zero vs dt_gap_too_large)
  uint32_t dtMs = nowMs - lastEnergyUpdateMs;
  lastEnergyIntegrationDtMs = dtMs;
  float dtHours = dtMs / 3600000.0f;
  lastEnergyUpdateMs = nowMs;

  if (dtMs == 0) {
    integrationAllowed = false;
    integrationSkipReason = "dt_zero";
    return;
  }
  if (dtMs > ENERGY_MAX_INTEGRATION_GAP_MS) {
    integrationAllowed = false;
    integrationSkipReason = "dt_gap_too_large";
    lastEnergyUpdateMs = 0; // Invalidate baseline on gap: require new clean baseline
    return;
  }

  // 9. Delta Calculation (Discrete Rectangular Approximation)
  float deltaUsed = dischargePowerKw * dtHours;
  float deltaRegen = regenPowerKw * dtHours;

  // 10. Overflow-Safe Atomic Counter Commit Guard
  float nextTripKwhUsed = tripKwhUsed + ((deltaUsed > 0.0f && std::isfinite(deltaUsed)) ? deltaUsed : 0.0f);
  float nextTripRegenKwh = tripRegenKwh + ((deltaRegen > 0.0f && std::isfinite(deltaRegen)) ? deltaRegen : 0.0f);

  if (!std::isfinite(nextTripKwhUsed) || nextTripKwhUsed < tripKwhUsed) {
    integrationAllowed = false;
    integrationSkipReason = "trip_used_overflow";
    lastEnergyUpdateMs = 0;
    return;
  }

  if (!std::isfinite(nextTripRegenKwh) || nextTripRegenKwh < tripRegenKwh) {
    integrationAllowed = false;
    integrationSkipReason = "trip_regen_overflow";
    lastEnergyUpdateMs = 0;
    return;
  }

  // Atomic Commit: Both counters commit together; never a half-update
  tripKwhUsed = nextTripKwhUsed;
  tripRegenKwh = nextTripRegenKwh;

  if (!energySessionStarted) {
    energySessionStarted = true;
  }

  integrationAllowed = true;
  integrationSkipReason = "none";
  energySourceLabel = "HV_POWER_INTEGRATION";
  energyValid = true;
  energyError = "ok";
}

inline void updateTripEnergyValues() {
  updateEnergySourceAUTO();

  if (!energyValid) {
    tripCostBaht = tripKwhUsed * electricityRateBahtPerKwh;
    energy_kwh = tripKwhUsed;
    return;
  }

  // Authority strictly isolated by mode:
  if (energySourceMode == 0) {
    // AUTO (0): tripKwhUsed is driven SOLELY by updateHvPowerAndTripEnergy()
    // Strictly NO assignment to tripKwhUsed from VP_PID_ACC_DISCHARGE or VP_PID_ENERGY!
  } else if (energySourceMode == 1) { // Manual BMS
    if (accumulatedDischargeValidated && accumulatedDischargeNow > 0.0f && accumulatedDischargeStart >= 0.0f) {
      float delta = accumulatedDischargeNow - accumulatedDischargeStart;
      if (delta >= 0.0f && delta < 100.0f) {
        tripKwhUsed = delta;
      }
    }
  } else if (energySourceMode == 2) {
    // Manual HV (2): driven solely by updateHvPowerAndTripEnergy()
  } else if (energySourceMode == 3) { // Manual VP_PID_ENERGY
    tripKwhUsed = discharge_kwh;
  }

  energy_kwh = tripKwhUsed;
  tripCostBaht = tripKwhUsed * electricityRateBahtPerKwh;

  static unsigned long lastComparisonLogMs = 0;
  if (millis() - lastComparisonLogMs > 5000 || lastComparisonLogMs == 0) {
    lastComparisonLogMs = millis();
    Serial.printf("[ENERGY] DEVICE_TRIP_KWH=%.3f REGEN_KWH=%.3f POWER=%.3fkW\n", tripKwhUsed, tripRegenKwh, batteryPowerKw);
    Serial.printf("[ENERGY] ENERGY_SOURCE=%s ERROR=%s SESSION_STARTED=%d\n", energySourceLabel.c_str(), energyError.c_str(), energySessionStarted ? 1 : 0);
    Serial.printf("  -> Telemetry: V=%.1fV(age=%lu), I=%.1fA(age=%lu), dt=%lums, skip=%s\n",
                  hvBatteryVoltage, hvVoltageAgeMs, hvBatteryCurrent, hvCurrentAgeMs,
                  lastEnergyIntegrationDtMs, integrationSkipReason.c_str());
  }
}


// ============================================================
// Timeout-Guarded ELM327 Response Reader
// ============================================================
String readELMResponse(unsigned long timeout = 1000) {
  String response = "";
  unsigned long startTime = millis();
  while (millis() - startTime < timeout) {
    if (obdClient.available()) {
      char c = (char)obdClient.read();
      if (c == '>') {
        return response;
      }
      response += c;
    }
  }
  Serial.println("[WARN] ELM327 response timeout!");
  return response;
}

inline IPAddress parseIP(const String& ipStr) {
  int parts[4] = {0, 0, 0, 0};
  int partIdx = 0;
  String currentPart = "";
  for (int i = 0; i < ipStr.length(); i++) {
    char c = ipStr.charAt(i);
    if (c == '.') {
      parts[partIdx++] = currentPart.toInt();
      currentPart = "";
      if (partIdx >= 4) break;
    } else if (c >= '0' && c <= '9') {
      currentPart += c;
    }
  }
  if (partIdx < 4) {
    parts[partIdx] = currentPart.toInt();
  }
  return IPAddress(parts[0], parts[1], parts[2], parts[3]);
}

// ============================================================
// State Variables for STA, TCP, and ELM Protocol Managers
// ============================================================
static uint8_t staConnectAttempt = 0;
static uint32_t staLastAttemptMs = 0;
static uint32_t staBackoffMs = 0;
static uint32_t staFirstFailureMs = 0;
static bool scanRunning = false;
static uint32_t lastScanTriggerMs = 0;
static uint32_t lastRelaxedScanMs = 0;
static uint32_t scanStartMs = 0;
static uint32_t scanRecoveryCount = 0;
static uint32_t scanTimeoutCount = 0;
static uint32_t scanFailCount = 0;
static uint32_t wifiDisconnectRecoveryCount = 0;
static std::vector<ScanCandidate> candidateQueue;
static size_t currentCandidateIdx = 0;

static constexpr uint8_t MAX_DIRECT_CONNECT_ATTEMPTS = 2;
static uint8_t directConnectAttempts = 0;
static bool directConnectFastPathExhausted = false;

static uint8_t tcpConnectAttempt = 0;
static uint32_t tcpLastAttemptMs = 0;
static uint32_t tcpBackoffMs = 0;

static String elmResponseBuffer = "";
static uint32_t elmStepStartMs = 0;

static String currentResponseBuffer = "";
static bool isWaitingForResponse = false;
static int consecutiveTimeouts = 0;
static unsigned long commandSentTime = 0;
static String lastSentCommand = "";
static unsigned long nextCommandSendTime = 0;
static String currentHeaderState = "";
static int obdStep = 0;

// Reset OBD Transaction State on Candidate Switch / Disconnect
// Purified: resets transaction variables and buffers ONLY.
// Does NOT touch currentTcpState or currentElmState (managed by TCP manager).
void resetObdTransactionState() {
  elmResponseBuffer = "";
  elmStepStartMs = 0;
  currentResponseBuffer = "";
  isWaitingForResponse = false;
  obdConnected = false;
  obdConnecting = false;
  obdStep = 0;
  currentHeaderState = "";
  consecutiveTimeouts = 0;
  commandSentTime = 0;
  nextCommandSendTime = 0;
  lastSentCommand = "";

  // Trip Energy V1 Baseline & Invalidation Guard on transport recovery/disconnect
  // Prevents integrating across disconnect gaps (even fast 1-2s reconnects).
  // Strictly preserves accumulated tripKwhUsed and tripRegenKwh (NO reset of trip counters!).
  lastEnergyUpdateMs = 0;
  lastEnergyIntegrationDtMs = 0;
  integrationAllowed = false;
  integrationSkipReason = "recovery_baseline_reset";
  hvVoltageValid = false;
  hvCurrentValid = false;

  // Recovery Status Consistency: immediately clear active energy claim for live-HV modes
  if (energySourceMode == 0 || energySourceMode == 2) {
    energyValid = false;
    energySourceLabel = "NO_DATA";
    energyError = "recovery_baseline_reset";
  }
}

// ============================================================
// 1. STA Link Manager
// ============================================================
void updateStaManager() {
  // Consume STA Events from callback under Critical Section
  WiFiEventRecord localStaEvent = {0, 0, (WiFiEvent_t)0, 0, false};
  bool hasStaEvent = false;
  portENTER_CRITICAL(&wifiEventMux);
  if (latestStaEvent.pending && isSequenceNewer(latestStaEvent.sequence, lastProcessedStaSequence)) {
    localStaEvent.sequence = latestStaEvent.sequence;
    localStaEvent.timestampMs = latestStaEvent.timestampMs;
    localStaEvent.event = latestStaEvent.event;
    localStaEvent.disconnectReason = latestStaEvent.disconnectReason;
    localStaEvent.pending = true;
    latestStaEvent.pending = false;
    lastProcessedStaSequence = localStaEvent.sequence;
    hasStaEvent = true;
  }
  portEXIT_CRITICAL(&wifiEventMux);

  if (hasStaEvent) {
    if (localStaEvent.event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      wifiDisconnectReason = localStaEvent.disconnectReason;
      lastWiFiDisconnectMs = localStaEvent.timestampMs;

      // INVARIANT: wifiLastDropMs updated UNCONDITIONALLY from canonical event timestamp
      wifiLastDropMs       = localStaEvent.timestampMs;

      // Increment dropCount FIRST so DROP #1 correctly logs dropCount=1
      wifiDropCount++;

      // Only start a recovery cycle if STA had an established connection previously.
      // Preserves first disconnect timestamp across multiple re-drops in the same cycle.
      if (wifiEverConnected) {
        if (!wifiDropRecoveryActive) {
          wifiDropRecoveryStartMs = localStaEvent.timestampMs;
        }
        wifiDropRecoveryActive = true;
      }

      if (wifiEverConnected && obdLastGoodFrameMs != 0) {
        obdAwaitingFreshTelemetryAfterRecovery = true;
        lastKnownTelemetryTimestampAtRecovery = obdLastGoodFrameMs;
      }

      currentTcpState = TCP_STATE_IDLE;
      currentElmState = ELM_INIT_IDLE;
      obdClient.stop();
      resetObdTransactionState();

      // ── Mandatory drop classification snapshot ───────────────────────────────────
      // All fields required. reason=XX is raw wifi_err_reason_t integer — do not
      // interpret it here. Root cause analysis is post-test from collected logs.
      Serial.printf(
        "[WIFI][STA][DROP] reason=%d rssi=%d staChan=%u apChan=%u targetApChan=%u\n"
        "  staState=%d tcpState=%d elmState=%d channelSync=%d portalState=%d\n"
        "  localIP=%s wifiStatus=%d upload=%d\n"
        "  obdLastGood=%lu dropCount=%u healthBeforeRecompute=%d\n",
        wifiDisconnectReason,
        WiFi.RSSI(),
        (unsigned)WiFi.channel(),
        (unsigned)currentApChannel,
        (unsigned)targetApChannel,
        (int)currentStaState,
        (int)currentTcpState,
        (int)currentElmState,
        (int)channelSyncState,
        (int)currentPortalState,
        WiFi.localIP().toString().c_str(),
        (int)WiFi.status(),
        (int)uploadInProgress,
        (unsigned long)obdLastGoodFrameMs,
        (unsigned)wifiDropCount,
        (int)obdHealthState
      );

      if (currentStaState == STA_STATE_SCANNING) {
        // INV-SCAN-01: STA_DISCONNECTED must NEVER cancel an already-active discovery
        // scan merely because the STA is in STA_STATE_SCANNING. In SCANNING there is no
        // association to recover, and scanNetworks(true) itself emits a disassociation
        // event from the previous network — cancelling here creates a self-sustaining
        // START -> RECOVER -> START loop in which the scan can never complete.
        // Sole owners of active-scan cancellation: the STA_SCAN_TIMEOUT_MS watchdog and
        // the scan-completion block below. This handler is observational only.
        Serial.printf("[WIFI][STATE] SCANNING preserved; STA_DISCONNECTED ignored scanRunning=%d scanAge=%lu\n",
                      (int)scanRunning,
                      (unsigned long)(scanRunning && scanStartMs > 0 ? (millis() - scanStartMs) : 0));
      } else if (currentStaState != STA_STATE_WAIT_FOR_ADAPTER) {
        candidateQueue.clear();
        currentCandidateIdx = 0;
        currentStaState = STA_STATE_BACKOFF;
        staBackoffMs = calculateBackoffMs(staConnectAttempt++);
        staLastAttemptMs = millis();
        if (staFirstFailureMs == 0) staFirstFailureMs = millis();
        wifiDisconnectRecoveryCount++;
        Serial.printf("[WIFI][STA][DISCONNECT] reason=%d, backoff=%u ms (attempt %u, recoveryCount=%u)\n",
                      wifiDisconnectReason, staBackoffMs, staConnectAttempt, (unsigned)wifiDisconnectRecoveryCount);
      }
    } else if (localStaEvent.event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
      currentStaState = STA_STATE_CONNECTED;
      staConnectAttempt = 0;
      staFirstFailureMs = 0;
      directConnectAttempts = 0;
      directConnectFastPathExhausted = false;
      currentState = STATE_TCP_CONNECTING;
      currentTcpState = TCP_STATE_CONNECTING;
      currentElmState = ELM_INIT_IDLE;
      Serial.printf("[WIFI][STA] Connected! SSID=%s Ch=%u IP=%s RSSI=%d\n",
                    WiFi.SSID().c_str(), WiFi.channel(), WiFi.localIP().toString().c_str(), WiFi.RSSI());

      // Latch that STA has successfully connected at least once (strictly monotonic)
      wifiEverConnected = true;

      // ── STA association restored (only after active drop, not on boot) ───────────
      // RECONNECTED = STA layer association restored. Does NOT imply full stack health.
      // RECONNECTED.health is a pre-health-recompute snapshot; authoritative health is assigned only by handleConnection().
      // NOTE: Multiple [RECONNECTED] events may occur in one cycle if RF re-drops before TCP/ELM.
      if (wifiDropRecoveryActive) {
        uint32_t staDowntime = (uint32_t)(millis() - wifiLastDropMs);
        Serial.printf(
          "[WIFI][STA][RECONNECTED] staDowntime=%lums dropCount=%u newChan=%u rssi=%d\n"
          "  tcp=%d elm=%d dataFresh=%d health=%d\n",
          (unsigned long)staDowntime,
          (unsigned)wifiDropCount,
          (unsigned)WiFi.channel(),
          WiFi.RSSI(),
          (int)currentTcpState,
          (int)currentElmState,
          (int)isObdDataFresh(),
          (int)obdHealthState
        );
      }

      uint8_t staChan = WiFi.channel();
      if (staChan != currentApChannel && staChan >= 1 && staChan <= 14) {
        targetApChannel = staChan;
        channelSyncState = CHANNEL_SYNC_PENDING;
      }
    }
  }

  // If already connected, nothing further needed in STA link manager
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  currentState = STATE_WIFI_CONNECTING;
  speed_raw_smooth = -1.0;
  consecutiveCanFailures = 0;
  motor_rpm = 0;
  vehicle_speed = 0;
  vehicleSpeedKmh = 0.0f;

  // Circuit Breaker: if adapter absent > 60s, transition to relaxed scanning.
  // In continuous scan mode, do NOT downgrade STA_STATE_SCANNING to WAIT_FOR_ADAPTER (INV-CS-03).
  if (staFirstFailureMs > 0 &&
      (millis() - staFirstFailureMs > CIRCUIT_BREAKER_TIMEOUT_MS) &&
      currentStaState != STA_STATE_SCANNING) {
    if (currentStaState != STA_STATE_WAIT_FOR_ADAPTER) {
      currentStaState = STA_STATE_WAIT_FOR_ADAPTER;
      Serial.println("[WIFI][STA] Circuit breaker active: adapter absent > 60s -> STA_STATE_WAIT_FOR_ADAPTER (relaxed 30s scan)");
    }
  }

  if (currentStaState == STA_STATE_IDLE || currentStaState == STA_STATE_BACKOFF) {
    if (currentStaState == STA_STATE_BACKOFF && (millis() - staLastAttemptMs < staBackoffMs)) {
      return; // Non-blocking backoff wait - NO Wi-Fi transaction allowed while waiting
    }

    // Check if we have candidates in the queue to try
    if (!candidateQueue.empty() && currentCandidateIdx < candidateQueue.size()) {
      ScanCandidate& cand = candidateQueue[currentCandidateIdx];
      obd_ssid_active = cand.ssid;
      obd_ip_active = "192.168.0.10";

      // Step 1: Abort pending driver transition before new candidate attempt
      WiFi.disconnect(false, false);

      // Step 2: Apply IP configuration
      if (isValidObdIp(obd_ip_active)) {
        connectionMode = "STATIC";
        IPAddress gateway = parseIP(obd_ip_active);
        IPAddress local_IP = gateway;
        local_IP[3] = 20;
        IPAddress subnet(255, 255, 255, 0);
        WiFi.config(local_IP, gateway, subnet);
      } else {
        connectionMode = "DHCP";
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
      }

      // Step 3: Hardware association begin
      uint8_t bssidBytes[6] = {0};
      if (parseMacAddress(cand.bssid, bssidBytes)) {
        WiFi.begin(cand.ssid.c_str(), nullptr, cand.channel, bssidBytes);
        Serial.printf("[WIFI][STA] BSSID-directed begin to Candidate #%u: SSID=%s, BSSID=%s, Ch=%u, Prio=%d\n",
                      (unsigned)(currentCandidateIdx + 1), cand.ssid.c_str(), cand.bssid.c_str(), cand.channel, cand.priority);
      } else {
        WiFi.begin(cand.ssid.c_str());
        Serial.printf("[WIFI][STA] SSID-only begin to Candidate #%u: SSID=%s, Prio=%d\n",
                      (unsigned)(currentCandidateIdx + 1), cand.ssid.c_str(), cand.priority);
      }

      // Step 4: Advance state and record attempt timestamp
      currentStaState = STA_STATE_CONNECTING;
      staLastAttemptMs = millis();
    } else if (obd_ssid_active.length() > 0 && !directConnectFastPathExhausted && directConnectAttempts < MAX_DIRECT_CONNECT_ATTEMPTS) {
      directConnectAttempts++;
      Serial.printf("[WIFI][STA] Direct connecting with saved BSSID (attempt %u/%u): SSID=%s, BSSID=%s, Ch=%u\n",
                    directConnectAttempts, MAX_DIRECT_CONNECT_ATTEMPTS,
                    obd_ssid_active.c_str(), savedObdBssid.c_str(), savedObdChannel);

      // Step 1: Abort pending driver transition before direct connect attempt
      WiFi.disconnect(false, false);

      // Step 2: Apply IP configuration
      if (isValidObdIp(obd_ip_active)) {
        connectionMode = "STATIC";
        IPAddress gateway = parseIP(obd_ip_active);
        IPAddress local_IP = gateway;
        local_IP[3] = 20;
        IPAddress subnet(255, 255, 255, 0);
        WiFi.config(local_IP, gateway, subnet);
      } else {
        connectionMode = "DHCP";
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
      }

      // Step 3: Hardware association begin
      uint8_t savedBssidBytes[6] = {0};
      if (parseMacAddress(savedObdBssid, savedBssidBytes)) {
        WiFi.begin(obd_ssid_active.c_str(), nullptr, savedObdChannel, savedBssidBytes);
      } else {
        WiFi.begin(obd_ssid_active.c_str());
      }

      // Step 4: Advance state and record attempt timestamp
      currentStaState = STA_STATE_CONNECTING;
      staLastAttemptMs = millis();
    } else {
      if (obd_ssid_active.length() > 0 && !directConnectFastPathExhausted) {
        directConnectFastPathExhausted = true;
        Serial.printf("[WIFI][STA][FALLBACK_SCAN] Direct-connect budget exhausted (%u/%u attempts) -> STA_STATE_SCANNING\n",
                      directConnectAttempts, MAX_DIRECT_CONNECT_ATTEMPTS);
      }
      currentStaState = STA_STATE_SCANNING;
      scanRunning = false;
      scanStartMs = 0;
    }
  }

  if (currentStaState == STA_STATE_WAIT_FOR_ADAPTER) {
    if (lastRelaxedScanMs == 0 || millis() - lastRelaxedScanMs >= WAIT_FOR_ADAPTER_SCAN_INTERVAL_MS) {
      lastRelaxedScanMs = millis();
      currentStaState = STA_STATE_SCANNING;
      scanRunning = false;
    }
  }

  if (currentStaState == STA_STATE_CONNECTING) {
    if ((uint32_t)(millis() - staLastAttemptMs) >= STA_CONNECT_TIMEOUT_MS) {
      Serial.printf("[WIFI][STA] Connect timeout (%u ms) in mode %s -> trying next candidate or scan\n",
                    STA_CONNECT_TIMEOUT_MS, connectionMode.c_str());
      currentTcpState = TCP_STATE_IDLE;
      currentElmState = ELM_INIT_IDLE;
      obdClient.stop();
      resetObdTransactionState();
      currentCandidateIdx++;
      staLastAttemptMs = millis();
      tcpLastAttemptMs = millis();
      elmStepStartMs = 0;
      commandSentTime = 0;
      nextCommandSendTime = 0;
      consecutiveTimeouts = 0;
      lastSentCommand = "";

      if (!candidateQueue.empty() && currentCandidateIdx < candidateQueue.size()) {
        currentStaState = STA_STATE_IDLE; // Try next candidate immediately with clean re-baselined timers
      } else {
        // Candidate queue exhausted: check if queue was actually populated before resetting
        if (!candidateQueue.empty() && currentCandidateIdx >= candidateQueue.size()) {
          Serial.println("[WIFI][STA] All scan candidates exhausted -> resetting fast-path lifecycle for next cycle");
          directConnectAttempts = 0;
          directConnectFastPathExhausted = false;
          candidateQueue.clear();
          currentCandidateIdx = 0;
        }
        currentStaState = STA_STATE_SCANNING;
        scanRunning = false;
      }
    }
  }

  if (currentStaState == STA_STATE_SCANNING) {
    // ── Defensive invariant — should not normally be reachable ───────────────────
    // updateStaManager() already has: if (WiFi.status() == WL_CONNECTED) { return; }
    // at the top of the function, which prevents reaching SCANNING state
    // while STA is associated. This check is a belt-and-suspenders invariant only.
    if (WiFi.status() == WL_CONNECTED && isObdDataFresh()) {
      return;
    }

    // INVARIANT 14: UPLOAD_ACTIVE MUST NOT trigger active Wi-Fi scan (Absolute Zero Exception)
    if (uploadInProgress || currentPortalState == PORTAL_STATE_UPLOAD) {
      return; // Defer scan until upload completes
    }

    // Policy: Defer scan while Client Device is actively using Web Portal (unless Circuit Breaker fires)
    bool apClientActive = setupApClientConnected || (WiFi.softAPgetStationNum() > 0);
    if (apClientActive && currentStaState != STA_STATE_WAIT_FOR_ADAPTER) {
      return; // Defer scan to preserve web portal responsiveness
    }

    if (!scanRunning) {
      if (lastScanTriggerMs == 0 || millis() - lastScanTriggerMs > 5000) {
        int16_t triggerResult = WiFi.scanNetworks(true); // Non-blocking asynchronous Wi-Fi scan
        lastScanTriggerMs = millis();
        if (triggerResult == WIFI_SCAN_RUNNING) {
          scanRunning = true;
          scanStartMs = millis();
          Serial.println("[WIFI][SCAN][START] Async scan triggered");
        } else {
          WiFi.scanDelete();
          scanFailCount++;
          Serial.printf("[WIFI][SCAN][FAIL] Trigger rejected (result=%d, failCount=%u)\n",
                        (int)triggerResult, (unsigned)scanFailCount);
        }
      }
    } else {
      int16_t scanResult = WiFi.scanComplete();
      if (scanResult >= 0) {
        scanRunning = false;
        scanStartMs = 0;
        Serial.printf("[WIFI][SCAN][DONE] Found %d networks\n", scanResult);

        std::vector<ScanCandidate> candidates;

        for (int i = 0; i < scanResult; ++i) {
          String s = WiFi.SSID(i);
          s.trim();
          String b = WiFi.BSSIDstr(i);
          int32_t r = WiFi.RSSI(i);
          uint8_t ch = WiFi.channel(i);
          String s_lower = s;
          s_lower.toLowerCase();

          int prio = 99; // Ambient non-OBD default
          if (s.length() > 0 && s == savedObdSsid && b.length() > 0 && b == savedObdBssid) {
            prio = 1; // Saved SSID + Saved BSSID
          } else if (s.length() > 0 && s == savedObdSsid) {
            prio = 2; // Saved SSID
          } else if (s_lower.indexOf("obd") != -1 || s_lower.indexOf("elm") != -1 ||
                     s_lower.indexOf("vgate") != -1 || s_lower.indexOf("vlink") != -1 ||
                     s_lower.indexOf("aermotor") != -1 || s_lower.indexOf("wifi_obdii") != -1 ||
                     s_lower.indexOf("icar") != -1 || s_lower.startsWith("clk")) {
            prio = 3; // Known OBD signature keyword
          }

          // Candidate Filtering: discard non-OBD ambient networks
          if (prio <= 3) {
            // Deduplication by (SSID, BSSID)
            bool duplicateFound = false;
            for (auto& existing : candidates) {
              if (existing.ssid == s && (s.length() == 0 || existing.bssid == b)) {
                duplicateFound = true;
                if (r > existing.rssi) {
                  existing.rssi = r;
                  existing.channel = ch;
                  existing.priority = prio;
                }
                break;
              }
            }
            if (!duplicateFound) {
              candidates.push_back({s, b, r, ch, prio});
            }
          }
        }

        WiFi.scanDelete(); // Free scan results memory immediately

        // Deterministic Sort: Priority (ascending) -> RSSI (descending) -> BSSID
        std::sort(candidates.begin(), candidates.end(), [](const ScanCandidate& a, const ScanCandidate& b) {
          if (a.priority != b.priority) return a.priority < b.priority;
          if (a.rssi != b.rssi) return a.rssi > b.rssi;
          return a.bssid < b.bssid;
        });

        candidateQueue = candidates;
        currentCandidateIdx = 0;

        if (!candidateQueue.empty() && candidateQueue[0].ssid.length() > 0) {
          currentStaState = STA_STATE_IDLE; // Trigger connection to candidate #0
        } else {
          // Continuous Scan: No suitable candidate found in this scan pass.
          // Stay in STA_STATE_SCANNING and continue scanning every 5s minimum interval.
          // Do NOT increment staConnectAttempt (scan absence is NOT a connection failure).
          currentStaState = STA_STATE_SCANNING;
          Serial.println("[WIFI][STA] Scan completed — no suitable OBD candidate found. Continuing SCANNING cycle.");
        }
      } else if (scanResult == WIFI_SCAN_FAILED) {
        WiFi.scanDelete();
        scanRunning = false;
        scanStartMs = 0;
        scanFailCount++;
        currentStaState = STA_STATE_SCANNING;
        Serial.printf("[WIFI][SCAN][FAIL] Completion failed — driver state cleared (failCount=%u)\n",
                      (unsigned)scanFailCount);
      } else if (scanStartMs > 0 && (millis() - scanStartMs > STA_SCAN_TIMEOUT_MS)) {
        // Scan timeout
        uint32_t scanAgeMs = millis() - scanStartMs;
        WiFi.scanDelete();
        scanRunning = false;
        scanStartMs = 0;
        scanTimeoutCount++;
        currentStaState = STA_STATE_SCANNING;
        Serial.printf("[WIFI][SCAN][TIMEOUT] Scan timed out after %lums — cleared for recovery (timeoutCount=%u)\n",
                      (unsigned long)scanAgeMs, (unsigned)scanTimeoutCount);
      }
    }
  }
}

// Helper function for entering TCP_STATE_RECONNECT_WAIT
// Strictly edge-triggered; guarantees clean transaction reset and non-blocking backoff.
// State assignment strictly precedes resetObdTransactionState() and obdClient.stop().
inline void transitionTcpToReconnectWait(const char* reason) {
  const uint32_t now = millis();

  // Flag that TCP/ELM recovery cycle is in progress and capture pre-recovery telemetry timestamp
  obdAwaitingFreshTelemetryAfterRecovery = true;
  lastKnownTelemetryTimestampAtRecovery = obdLastGoodFrameMs;

  // 1. Logical state transitions FIRST (eliminates race conditions & callback reentrancy)
  currentTcpState = TCP_STATE_RECONNECT_WAIT;
  currentElmState = ELM_INIT_IDLE;

  // 2. Purified transaction cleanup (does NOT touch TCP/ELM states)
  resetObdTransactionState();

  // 3. Deterministic single-timestamp updates & counter tracking
  tcpLastAttemptMs = now;
  lastTCPDisconnectMs = now;
  tcpConnectAttempt = 0;   // Reset attempt sequence for fresh reconnect cycle
  tcpReconnectCount++;     // Discrete TCP loss event counter

  // 4. Socket teardown LAST
  obdClient.stop();

  Serial.printf("[WIFI][TCP] Active socket lost -> TCP_STATE_RECONNECT_WAIT (%s, count %u)\n",
                reason, (unsigned)tcpReconnectCount);
}

// ============================================================
// 2. TCP Socket & Non-Blocking ELM327 Handshake Manager
// ============================================================
void updateTcpAndElmManager() {
  if (WiFi.status() != WL_CONNECTED) {
    if (currentTcpState != TCP_STATE_IDLE) {
      currentTcpState = TCP_STATE_IDLE;
      currentElmState = ELM_INIT_IDLE;
      obdClient.stop();
      obdConnected = false;
      obdConnecting = false;
    }
    return;
  }

  if (currentTcpState == TCP_STATE_ACTIVE && obdClient.connected()) {
    return; // TCP and ELM are active and confirmed
  }

  // ── Critical Edge Transition: Active socket lost -> Transition to RECONNECT_WAIT ──
  // Strictly edge-triggered: subsequent loops see currentTcpState == TCP_STATE_RECONNECT_WAIT
  // and cannot re-enter this block until active socket is restored.
  if (currentTcpState == TCP_STATE_ACTIVE && !obdClient.connected()) {
    currentState = STATE_TCP_CONNECTING;
    obdConnected = false;
    obdConnecting = false;
    transitionTcpToReconnectWait("socket closed while ACTIVE");
    return;
  }

  currentState = STATE_TCP_CONNECTING;
  obdConnected = false;
  obdConnecting = false;

  if (currentTcpState == TCP_STATE_RECONNECT_WAIT) {
    tcpBackoffMs = calculateBackoffMs(tcpConnectAttempt);
    if (millis() - tcpLastAttemptMs < tcpBackoffMs) {
      return; // Non-blocking TCP backoff
    }
    currentTcpState = TCP_STATE_CONNECTING;
    currentElmState = ELM_INIT_IDLE;
  }

  // ── Edge transition: Idle socket on connected STA -> start TCP connection ──
  // TCP IDLE Recovery Gate: only when WiFi.status() == WL_CONNECTED,
  // and does NOT override an existing CONNECTING, ACTIVE, or RECONNECT_WAIT state.
  if (WiFi.status() == WL_CONNECTED && currentTcpState == TCP_STATE_IDLE) {
    currentTcpState = TCP_STATE_CONNECTING;
    currentElmState = ELM_INIT_IDLE;
  }

  if (currentTcpState == TCP_STATE_CONNECTING && currentElmState == ELM_INIT_IDLE) {
    tcpLastAttemptMs = millis();
    Serial.printf("[WIFI][TCP] Connecting to OBD2 socket %s:%u (attempt %u)...\n",
                  obd_ip_active.c_str(), tcp_port, tcpConnectAttempt);

    obdClient.stop();
    if (obdClient.connect(obd_ip_active.c_str(), tcp_port, TCP_CONNECT_TIMEOUT_MS)) {
      Serial.println("[WIFI][TCP] Socket connected! Starting non-blocking ELM327 handshake...");
      currentElmState = ELM_INIT_SEND_ATZ;
      elmResponseBuffer = "";
    } else {
      tcpConnectAttempt++;
      lastTCPDisconnectMs = millis();
      currentTcpState = TCP_STATE_RECONNECT_WAIT;
      Serial.printf("[WIFI][TCP] Socket connect failed (attempt %u)\n", tcpConnectAttempt);
      return;
    }
  }

  // Step-by-step Non-Blocking ELM327 Handshake Engine
  if (currentElmState != ELM_INIT_IDLE && currentElmState != ELM_INIT_CONFIRMED && currentElmState != ELM_INIT_FAILED) {
    uint32_t now = millis();

    // Read available data from socket
    while (obdClient.available()) {
      char c = (char)obdClient.read();
      elmResponseBuffer += c;
    }

    switch (currentElmState) {
      case ELM_INIT_SEND_ATZ:
        elmResponseBuffer = "";
        obdClient.print("ATZ\r");
        elmStepStartMs = now;
        currentElmState = ELM_INIT_WAIT_ATZ;
        break;

      case ELM_INIT_WAIT_ATZ:
        if (elmResponseBuffer.indexOf("ELM327") != -1 || elmResponseBuffer.indexOf(">") != -1) {
          currentElmState = ELM_INIT_SEND_ATD;
        } else if (now - elmStepStartMs > ELM_STEP_TIMEOUT_MS) {
          Serial.println("[WIFI][ELM] ATZ validation failed / timed out -> ELM_INIT_FAILED");
          currentElmState = ELM_INIT_FAILED;
        }
        break;

      case ELM_INIT_SEND_ATD:
        elmResponseBuffer = "";
        obdClient.print("ATD\r");
        elmStepStartMs = now;
        currentElmState = ELM_INIT_WAIT_ATD;
        break;

      case ELM_INIT_WAIT_ATD:
        if (elmResponseBuffer.indexOf("OK") != -1 || elmResponseBuffer.indexOf(">") != -1) {
          currentElmState = ELM_INIT_SEND_ATE0;
        } else if (now - elmStepStartMs > ELM_STEP_TIMEOUT_MS) {
          Serial.println("[WIFI][ELM] ATD validation failed / timed out -> ELM_INIT_FAILED");
          currentElmState = ELM_INIT_FAILED;
        }
        break;

      case ELM_INIT_SEND_ATE0:
        elmResponseBuffer = "";
        obdClient.print("ATE0\r");
        elmStepStartMs = now;
        currentElmState = ELM_INIT_WAIT_ATE0;
        break;

      case ELM_INIT_WAIT_ATE0:
        if (elmResponseBuffer.indexOf("OK") != -1 || elmResponseBuffer.indexOf(">") != -1) {
          currentElmState = ELM_INIT_SEND_ATH1;
        } else if (now - elmStepStartMs > ELM_STEP_TIMEOUT_MS) {
          Serial.println("[WIFI][ELM] ATE0 validation failed / timed out -> ELM_INIT_FAILED");
          currentElmState = ELM_INIT_FAILED;
        }
        break;

      case ELM_INIT_SEND_ATH1:
        elmResponseBuffer = "";
        obdClient.print("ATH1\r");
        elmStepStartMs = now;
        currentElmState = ELM_INIT_WAIT_ATH1;
        break;

      case ELM_INIT_WAIT_ATH1:
        if (elmResponseBuffer.indexOf("OK") != -1 || elmResponseBuffer.indexOf(">") != -1) {
          currentElmState = ELM_INIT_SEND_ATSP0;
        } else if (now - elmStepStartMs > ELM_STEP_TIMEOUT_MS) {
          Serial.println("[WIFI][ELM] ATH1 validation failed / timed out -> ELM_INIT_FAILED");
          currentElmState = ELM_INIT_FAILED;
        }
        break;

      case ELM_INIT_SEND_ATSP0:
        elmResponseBuffer = "";
        obdClient.print("ATSP0\r");
        elmStepStartMs = now;
        currentElmState = ELM_INIT_WAIT_ATSP0;
        break;

      case ELM_INIT_WAIT_ATSP0:
        if (elmResponseBuffer.indexOf("OK") != -1 || elmResponseBuffer.indexOf(">") != -1) {
          currentElmState = ELM_INIT_SEND_ATAL;
        } else if (now - elmStepStartMs > ELM_STEP_TIMEOUT_MS) {
          Serial.println("[WIFI][ELM] ATSP0 validation failed / timed out -> ELM_INIT_FAILED");
          currentElmState = ELM_INIT_FAILED;
        }
        break;

      case ELM_INIT_SEND_ATAL:
        elmResponseBuffer = "";
        obdClient.print("ATAL\r");
        elmStepStartMs = now;
        currentElmState = ELM_INIT_WAIT_ATAL;
        break;

      case ELM_INIT_WAIT_ATAL:
        if (elmResponseBuffer.indexOf("OK") != -1 || elmResponseBuffer.indexOf(">") != -1) {
          currentElmState = ELM_INIT_SEND_ATST64;
        } else if (now - elmStepStartMs > ELM_STEP_TIMEOUT_MS) {
          Serial.println("[WIFI][ELM] ATAL validation failed / timed out -> ELM_INIT_FAILED");
          currentElmState = ELM_INIT_FAILED;
        }
        break;

      case ELM_INIT_SEND_ATST64:
        elmResponseBuffer = "";
        obdClient.print("ATST64\r");
        elmStepStartMs = now;
        currentElmState = ELM_INIT_WAIT_ATST64;
        break;

      case ELM_INIT_WAIT_ATST64:
        if (elmResponseBuffer.indexOf("OK") != -1 || elmResponseBuffer.indexOf(">") != -1) {
          currentElmState = ELM_INIT_CONFIRMED;
        } else if (now - elmStepStartMs > ELM_STEP_TIMEOUT_MS) {
          Serial.println("[WIFI][ELM] ATST64 validation failed / timed out -> ELM_INIT_FAILED");
          currentElmState = ELM_INIT_FAILED;
        }
        break;

      default:
        break;
    }

    if (currentElmState == ELM_INIT_CONFIRMED) {
      Serial.println("[WIFI][TCP] ELM327 Handshake Complete — OBD_CONFIRMED!");
      tcpConnectAttempt = 0;
      currentTcpState = TCP_STATE_ACTIVE;
      currentState = STATE_ACTIVE;
      obdConnected = true;
      obdConnecting = false;

      // Crash-safe NVS persistence with validity guard and read-back verification
      commitConfirmedObdConfig();

      isWaitingForResponse = false;
      currentResponseBuffer = "";
      obdStep = 0;
      currentHeaderState = "";
      nextCommandSendTime = millis() + 100;
    } else if (currentElmState == ELM_INIT_FAILED) {
      Serial.println("[WIFI][TCP] ELM Handshake FAILED — cleaning state and advancing candidate queue");
      if (wifiEverConnected && obdLastGoodFrameMs != 0) {
        obdAwaitingFreshTelemetryAfterRecovery = true;
        lastKnownTelemetryTimestampAtRecovery = obdLastGoodFrameMs;
      }
      obdClient.stop();
      currentElmState = ELM_INIT_IDLE;
      resetObdTransactionState();
      currentCandidateIdx++;
      staLastAttemptMs = millis();
      tcpLastAttemptMs = millis();
      elmStepStartMs = 0;
      commandSentTime = 0;
      nextCommandSendTime = 0;
      consecutiveTimeouts = 0;
      lastSentCommand = "";

      if (!candidateQueue.empty() && currentCandidateIdx < candidateQueue.size()) {
        currentStaState = STA_STATE_IDLE; // Try next candidate cleanly with re-baselined timers
        currentTcpState = TCP_STATE_IDLE;
      } else {
        currentTcpState = TCP_STATE_RECONNECT_WAIT;
        tcpConnectAttempt++;
        lastTCPDisconnectMs = millis();
      }
    }
  }
}

// ============================================================
// 3. OBD Polling Engine (Active Normal Operation)
// ============================================================
void updateObdPollingEngine() {
  // [R13] MUST execute before ANY early return.
  // Guarantees SOC staleness is detected even during phone uploads (Test S)
  // or when car/TCP is disconnected (Test T).
  resolveBydSoc();

  if (uploadInProgress) {
    return; // Pause PID queries during phone upload
  }

  if (currentState != STATE_ACTIVE || currentTcpState != TCP_STATE_ACTIVE) {
    return;
  }

  wifiRssi = WiFi.RSSI();

  // 1. Send command non-blockingly
  if (!isWaitingForResponse && millis() >= nextCommandSendTime) {
    String cmd = "";
    if (car_profile == 0) { // EV -> BYD -> Dolphin
      switch (obdStep) {
      case 0:
        cmd = "ATRV";
        obdStep = 1;
        break;
      case 1:
        if (currentHeaderState != VP_HDR_BMS) {
          cmd = VP_ATSH_BMS;
          currentHeaderState = VP_HDR_BMS;
          obdStep = 2;
        } else {
          cmd = VP_REQ_BATT_TEMP;   // Battery temp
          obdStep = 3;
        }
        break;
      case 2:
        cmd = VP_REQ_BATT_TEMP;     // Battery temp
        obdStep = 3;
        break;
      case 3:
        cmd = VP_REQ_HV_VOLTAGE;     // HV Battery Voltage (BMS header confirmed)
        obdStep = 4;
        break;
      case 4:
        cmd = VP_REQ_HV_CURRENT;     // HV Battery Current (BMS header confirmed, sole integration trigger)
        obdStep = 5;
        break;
      case 5:
        if (currentHeaderState != VP_HDR_MCU) {
          cmd = VP_ATSH_MCU;
          currentHeaderState = VP_HDR_MCU;
          obdStep = 6;
        } else {
          cmd = VP_REQ_MOTOR_TEMP;   // Motor temp only; do NOT use VP_PID_MOTOR_RPM for RPM in this test
          obdStep = 7;
        }
        break;
      case 6:
        cmd = VP_REQ_MOTOR_TEMP;     // Motor temp
        obdStep = 7;
        break;
      case 7:
        if (currentHeaderState != VP_HDR_SPEED) {
          cmd = VP_ATSH_SPEED;
          currentHeaderState = VP_HDR_SPEED;
          obdStep = 8;
        } else {
          cmd = TEST1_PID;   // TEST1 speed candidate (dynamic based on build env)
          obdStep = 9;
        }
        break;
      case 8:
        cmd = TEST1_PID;     // TEST1 speed candidate (dynamic based on build env)
        obdStep = 9;
        break;
      case 9:
        if (lastSlowQueryTime > 0 && (millis() - lastSlowQueryTime < SLOW_QUERY_INTERVAL_MS)) {
          obdStep = 0;
          break;
        }
        if (currentHeaderState != VP_HDR_BMS) {
          cmd = VP_ATSH_BMS;
          currentHeaderState = VP_HDR_BMS;
          obdStep = 10;
        } else {
          cmd = VP_REQ_SOH;   // SOH/capacity existing parser
          obdStep = 11;
        }
        break;
      case 10:
        cmd = VP_REQ_SOH;
        obdStep = 11;
        break;
      case 11:
        cmd = VP_REQ_ENERGY;     // Energy existing parser
        obdStep = 12;
        break;
      case 12:
        cmd = VP_REQ_ACC_DISCHARGE;     // BMS Accumulated Discharge Energy
        obdStep = 13;
        break;
      case 13:
        cmd = VP_REQ_ACC_CHARGE;     // BMS Accumulated Charge Energy
        obdStep = 14;
        break;
      case 14:
        cmd = VP_REQ_SOC;     // SOC existing parser
        obdStep = 15;
        break;
      case 15:
        cmd = VP_REQ_CHARGE_COUNT;     // BASU Charge times (Candidate)
        obdStep = 0;
        lastSlowQueryTime = millis();
        break;
      }
    } else if (car_profile == 1) { // EV -> Generic
      switch (obdStep) {
      case 0:
        cmd = "ATRV";
        obdStep = 1;
        break;
      case 1:
        if (currentHeaderState != "7DF") {
          cmd = "ATSH7DF";
          currentHeaderState = "7DF";
          obdStep = 2;
        } else {
          cmd = "010C"; // RPM
          obdStep = 3;
        }
        break;
      case 2:
        cmd = "010C";
        obdStep = 3;
        break;
      case 3:
        cmd = "010D"; // Speed
        obdStep = 4;
        break;
      case 4:
        cmd = "012F"; // Fuel level/SOC representation
        obdStep = 5;
        break;
      case 5:
        cmd = "0105"; // Coolant Temp -> Temp
        obdStep = 0;  // Loop back
        break;
      }
    } else { // ICE -> Generic
      switch (obdStep) {
      case 0:
        cmd = "ATRV";
        obdStep = 1;
        break;
      case 1:
        if (currentHeaderState != "7DF") {
          cmd = "ATSH7DF";
          currentHeaderState = "7DF";
          obdStep = 2;
        } else {
          cmd = "010C"; // RPM
          obdStep = 3;
        }
        break;
      case 2:
        cmd = "010C";
        obdStep = 3;
        break;
      case 3:
        cmd = "010D"; // Speed
        obdStep = 4;
        break;
      case 4:
        cmd = "0105"; // Coolant Temp
        obdStep = 0;  // Loop back
        break;
      }
    }

    if (cmd.length() > 0) {
      obdClient.print(cmd + "\r");
      lastSentCommand = cmd;
      isWaitingForResponse = true;
      commandSentTime = millis();
      currentResponseBuffer = "";
    }
  }

  // 2. Read response non-blockingly
  if (isWaitingForResponse) {
    while (obdClient.available()) {
      char c = (char)obdClient.read();
      if (c == '>') {
        // Complete response!
        isWaitingForResponse = false;
        consecutiveTimeouts = 0; // Reset timeouts on successful response
        nextCommandSendTime = millis() + 50; // 50ms breathing room delay

        String resp = currentResponseBuffer;
        resp.trim();
        if (resp.length() > 0) {
          rxBlinkState = !rxBlinkState;

          Serial.print("OBD RX for [");
          Serial.print(lastSentCommand);
          Serial.print("]: [");
          Serial.print(resp);
          Serial.println("]");

          bool isError =
              (resp.indexOf("NODATA") != -1 || resp.indexOf("NO DATA") != -1 ||
               resp.indexOf("SEARCHING") != -1 ||
               resp.indexOf("UNABLE") != -1 || resp.indexOf("ERROR") != -1 ||
               resp.indexOf("CANERROR") != -1 || resp.indexOf("?") != -1);

          if (!lastSentCommand.startsWith("AT")) {
            if (isError) {
              consecutiveCanFailures++;
              Serial.print(
                  "[INFO] CAN Query Failure. Consecutive CAN Failures: ");
              Serial.println(consecutiveCanFailures);
              if (consecutiveCanFailures >= 30) {
                motor_rpm = 0;
              }
            } else {
              consecutiveCanFailures = 0;
            }
          }

          if (isError) {
            Serial.println("[WARN] ELM327 error/no-data response, skipping.");
            break;
          }

          int idxV = resp.indexOf('V');
          if (idxV != -1 && resp.indexOf("41") == -1 &&
              resp.indexOf("62") == -1) {
            int startIdx = idxV - 1;
            while (startIdx >= 0) {
              char cChar = resp.charAt(startIdx);
              if ((cChar >= '0' && cChar <= '9') || cChar == '.')
                startIdx--;
              else
                break;
            }
            startIdx++;
            if (startIdx < idxV) {
              String valStr = resp.substring(startIdx, idxV);
              float val = valStr.toFloat();
              if (val > 5.0 && val < 20.0) {
                batt12v = val;
                Serial.print("  -> 12V Battery: ");
                Serial.println(batt12v);
              }
            }
          }

          String cleanResp = resp;
          cleanResp.replace(" ", "");
          cleanResp.replace("\r", "");
          cleanResp.replace("\n", "");
          cleanResp.toUpperCase();

          int idxSpeed = cleanResp.indexOf(VP_RESP_SPEED);
          const int offset = 6; // ข้าม "62" + PID 4 หลัก ไปยัง data byte ตัวแรก
          if (idxSpeed != -1 && cleanResp.length() >= (uint)(idxSpeed + offset + 4)) {
            String hexHi = cleanResp.substring(idxSpeed + offset, idxSpeed + offset + 2);
            String hexLo = cleanResp.substring(idxSpeed + offset + 2, idxSpeed + offset + 4);

            int hi = strtol(hexHi.c_str(), NULL, 16);
            int lo = strtol(hexLo.c_str(), NULL, 16);
            int raw = (hi << 8) | lo;

            // ตัวหารได้จากการ fit ค่าดิบกับความเร็วจริงขณะขับทดสอบ
            // ค่าที่ใช้งานจริงอยู่ใน vehicle_calibration.h (ไม่ได้เผยแพร่)
            float speedVal = raw / VP_SPEED_DIVISOR;

            if (speedVal >= 0.0f && speedVal <= 250.0f) {
              vehicle_speed = speedVal;  // raw target — NOT smoothed
              vehicleSpeedKmh = speedVal;
              lastRpmUpdateTime = millis();
              recordValidObdFrame();
              displayTargetSpeed = speedVal;

              // RPM Source Logic: estimated RPM from speed
              float estRpm = speedVal * VP_RPM_FACTOR;

              if (rpmSourceMode == 1) {
                // EST mode: always use estimated
                motor_rpm = (int)estRpm;
                displayTargetRpm = estRpm;
                rpmSourceLabel = "EST";
              } else if (rpmSourceMode == 0) {
                // AUTO mode: use MMCU if valid and recent, else estimated
                bool mmcuRecent = (mmcuLastUpdateMs > 0 && millis() - mmcuLastUpdateMs < 3000);
                if (mmcuRpmValid && mmcuRecent) {
                  motor_rpm = mmcuRpmDisplayAbs;
                  displayTargetRpm = (float)mmcuRpmDisplayAbs;
                  rpmSourceLabel = "REAL_MMCU";
                } else {
                  motor_rpm = (int)estRpm;
                  displayTargetRpm = estRpm;
                  rpmSourceLabel = "AUTO_FALLBACK_EST";
                }
              }
              // rpmSourceMode == 2 (MMCU): do NOT set motor_rpm here;
              // it's set exclusively in VP_PID_MOTOR_RPM parser

              Serial.printf("  -> %s: raw=%d speed=%.2f km/h rpm=%d src=%s\n",
                            TEST1_BUILD_NAME, raw, speedVal, motor_rpm,
                            rpmSourceLabel.c_str());
            } else {
              Serial.printf("  -> %s rejected: raw=%d speed=%.2f\n",
                            TEST1_BUILD_NAME, raw, speedVal);
            }
          }

          Serial.print("  Clean: ");
          Serial.println(cleanResp);

          int idx620032 = cleanResp.indexOf(VP_RESP_BATT_TEMP);
          if (idx620032 != -1) {
            if (cleanResp.length() >= (uint)(idx620032 + 8)) {
              String hexStr = cleanResp.substring(idx620032 + 6, idx620032 + 8);
              int rawVal = strtol(hexStr.c_str(), NULL, 16);
              float tempVal = rawVal - 40.0;
              if (tempVal > -40.0 && tempVal < 120.0) {
                batt_temp = tempVal;
                recordValidObdFrame();
                Serial.print("  -> Battery Temp: ");
                Serial.print(tempVal);
                Serial.println(" C");
              }
            }
          }

          int idx62000F = cleanResp.indexOf(VP_RESP_MOTOR_TEMP);
          if (idx62000F != -1) {
            if (cleanResp.length() >= (uint)(idx62000F + 8)) {
              String hexA = cleanResp.substring(idx62000F + 6, idx62000F + 8);
              int rawA = strtol(hexA.c_str(), NULL, 16);
              float tempVal = rawA - 40.0;
              if (tempVal > -40.0 && tempVal < 150.0) {
                motor_temp = tempVal;
                recordValidObdFrame();
                Serial.print("  -> Motor Temp: ");
                Serial.print(tempVal);
                Serial.println(" C");
              }
            }
          }

          int idx621FFC = cleanResp.indexOf(VP_RESP_SOH);
          if (idx621FFC != -1) {
            if (cleanResp.length() >= (uint)(idx621FFC + 14)) {
              String hexC = cleanResp.substring(idx621FFC + 10, idx621FFC + 12);
              String hexD = cleanResp.substring(idx621FFC + 12, idx621FFC + 14);
              int byteC = strtol(hexC.c_str(), NULL, 16);
              int byteD = strtol(hexD.c_str(), NULL, 16);
              float actualAh = ((byteD * 256.0) + byteC) / 100.0;
              float sohVal = (actualAh / VP_SOH_NOMINAL_AH) * 100.0;
              if (sohVal >= 0.0 && sohVal <= 110.0) {
                hv_soh = sohVal;
                if (hv_soh > 100.0)
                  hv_soh = 100.0;
                recordValidObdFrame();
                Serial.print("  -> SOH: ");
                Serial.print(sohVal);
                Serial.println("%");
              }
            }
          }

        // ── PID VP_PID_ENERGY: HV Battery Energy Counter (kWh) ──
          // ECU: BMS (response header = request + 8)
          // Response: <BMS resp hdr> 05 62 <PID> [byteA_low] [byteB_high]
          // Encoding: little-endian 16-bit, unit = 0.01 kWh
          int idx62000E = cleanResp.indexOf(VP_RESP_ENERGY);
          if (idx62000E != -1 && cleanResp.length() >= (uint)(idx62000E + 10)) {
            String hexA = cleanResp.substring(idx62000E + 6, idx62000E + 8); // low byte
            String hexB = cleanResp.substring(idx62000E + 8, idx62000E + 10); // high byte
            int byteA = strtol(hexA.c_str(), NULL, 16);
            int byteB = strtol(hexB.c_str(), NULL, 16);
            float current_kwh = ((byteB * 256.0f) + byteA) / 100.0f;
            energy22000Eraw = current_kwh;
            recordValidObdFrame();

            if (last_energy_raw < 0.0f) {
              // First reading this session — just capture, don't count
              last_energy_raw = current_kwh;
              Serial.printf("  -> Energy baseline captured: %.2f kWh (raw16=%d)\n",
                            current_kwh, byteB * 256 + byteA);
            } else {
              float delta = current_kwh - last_energy_raw;
              energy22000Edelta = delta;

              if (delta < -0.005f && delta > -1.0f) {
                // Negative delta = remaining energy decreased = energy consumed
                float consumed = -delta;
                discharge_kwh += consumed;
                last_energy_raw = current_kwh; // advance baseline
                Serial.printf("  -> VP_PID_ENERGY negative delta consumed +%.3f kWh (total_fallback=%.3f)\n",
                              consumed, discharge_kwh);
              } else if (delta < -1.0f) {
                // Huge negative jump = counter glitch/reset
                last_energy_raw = current_kwh;
                Serial.printf("  -> VP_PID_ENERGY large drop ignored: %.3f kWh (baseline advanced)\n", delta);
              } else if (delta > 0.005f) {
                // Positive delta = BMS recalculation (ignored for consumption, tracked for debug)
                energy22000EAdjustKwh += delta;
                Serial.printf("  -> VP_PID_ENERGY positive delta BMS recalculation +%.3f kWh, ignored for trip/regen\n", delta);
              }
            }
            updateTripEnergyValues();
          }

          // ── PID VP_PID_HV_VOLTAGE: HV Battery Voltage (V) ──
          int idx620008 = cleanResp.indexOf(VP_RESP_HV_VOLTAGE);
          if (idx620008 != -1 && cleanResp.length() >= (uint)(idx620008 + 10)) {
            String hexA = cleanResp.substring(idx620008 + 6, idx620008 + 8);
            String hexB = cleanResp.substring(idx620008 + 8, idx620008 + 10);
            int byteA = strtol(hexA.c_str(), NULL, 16);
            int byteB = strtol(hexB.c_str(), NULL, 16);

            // BYD Dolphin dataset/hardware format: 16-bit little-endian integer volts (byteA + byteB*256)
            float validVolt = (float)(byteA + byteB * 256);
            bool voltOk = (validVolt >= 200.0f && validVolt <= 500.0f);

            if (voltOk) {
              hvBatteryVoltage = validVolt;
              hvVoltageValid = true;
              hvVoltageLastUpdateMs = millis();
              recordValidObdFrame();
            } else {
              // Explicit Invalid Parser Contract:
              hvVoltageValid = false;
              lastEnergyUpdateMs = 0; // Invalidate baseline on corrupted voltage
              // Mode-Aware Status Refresh:
              if (energySourceMode == 0 || energySourceMode == 2) {
                energyValid = false;
                energySourceLabel = "NO_DATA";
                energyError = "hv_voltage_invalid";
              }
            }

            // Invariant 2: VP_PID_HV_VOLTAGE is V state only (NO side effect on Energy Engine).
            // Do NOT call updateTripEnergyValues() here to prevent premature skew drop.
            Serial.printf("  -> [VP_PID_HV_VOLTAGE] hvV=%.1f validV=%d\n",
                          hvBatteryVoltage, hvVoltageValid ? 1 : 0);
          }

          // ── PID VP_PID_HV_CURRENT: HV Battery Current (A) ──
          int idx620009 = cleanResp.indexOf(VP_RESP_HV_CURRENT);
          if (idx620009 != -1 && cleanResp.length() >= (uint)(idx620009 + 10)) {
            String hexA = cleanResp.substring(idx620009 + 6, idx620009 + 8);
            String hexB = cleanResp.substring(idx620009 + 8, idx620009 + 10);
            int byteA = strtol(hexA.c_str(), NULL, 16);
            int byteB = strtol(hexB.c_str(), NULL, 16);

            float currentCandidate = ((byteA + byteB * 256.0f) - VP_HV_CURRENT_OFFSET) / 10.0f;
            if (currentCandidate >= -400.0f && currentCandidate <= 500.0f) {
              hvBatteryCurrent = currentCandidate;
              hvCurrentValid = true;
              hvCurrentLastUpdateMs = millis(); // Written strictly at decode time T1
              recordValidObdFrame();

              // Increment sequence ID with wrap-around protection
              if (hvCurrentSampleSeq == UINT32_MAX) {
                hvCurrentSampleSeq = 1;
                hasIntegratedCurrentSample = false; // Reset wrap state to prevent collision
                lastIntegratedCurrentSeq = 0;
              } else {
                hvCurrentSampleSeq++;
              }

              // Sole trigger point for HV integration
              updateHvPowerAndTripEnergy();
              updateTripEnergyValues();
            } else {
              // Explicit Invalid Parser Contract:
              hvCurrentValid = false;
              lastEnergyUpdateMs = 0; // Invalidate baseline on corrupted current; DO NOT call integration engine
              // Mode-Aware Status Refresh:
              if (energySourceMode == 0 || energySourceMode == 2) {
                energyValid = false;
                energySourceLabel = "NO_DATA";
                energyError = "hv_current_invalid";
              }
            }

            bool ready = isHvPowerReady();
            Serial.printf("  -> [VP_PID_HV_CURRENT] ENERGY_SRC=%s hvV=%.1f validV=%d hvA=%.1f validA=%d ready=%d power=%.3fkW trip=%.3fkWh regen=%.3fkWh err=%s\n",
                          energySourceLabel.c_str(), hvBatteryVoltage, hvVoltageValid ? 1 : 0,
                          hvBatteryCurrent, hvCurrentValid ? 1 : 0, ready ? 1 : 0,
                          batteryPowerKw, tripKwhUsed, tripRegenKwh, energyError.c_str());
          }

          // ── PID VP_PID_ACC_DISCHARGE: BMS Accumulated Discharge Energy ──
          int idx620012 = cleanResp.indexOf(VP_RESP_ACC_DISCHARGE);
          if (idx620012 != -1 && cleanResp.length() >= (uint)(idx620012 + 10)) {
            String hexA = cleanResp.substring(idx620012 + 6, idx620012 + 8);
            String hexB = cleanResp.substring(idx620012 + 8, idx620012 + 10);
            int byteA = strtol(hexA.c_str(), NULL, 16);
            int byteB = strtol(hexB.c_str(), NULL, 16);

            float val_le16 = (byteA + byteB * 256.0f);
            float val_be16 = (byteB + byteA * 256.0f);
            float val_le32 = 0.0f;
            float val_be32 = 0.0f;

            if (cleanResp.length() >= (uint)(idx620012 + 14)) {
              String hexC = cleanResp.substring(idx620012 + 10, idx620012 + 12);
              String hexD = cleanResp.substring(idx620012 + 12, idx620012 + 14);
              int byteC = strtol(hexC.c_str(), NULL, 16);
              int byteD = strtol(hexD.c_str(), NULL, 16);
              val_le32 = (byteA + byteB * 256.0f + byteC * 65536.0f + byteD * 16777216.0f);
              val_be32 = (byteD + byteC * 256.0f + byteB * 65536.0f + byteA * 16777216.0f);
            }

            float candidate = 0.0f;
            if (val_le32 > 10000.0f) {
              candidate = val_le32 / 1000.0f; // Wh -> kWh
            } else if (val_le32 > 0.0f) {
              candidate = val_le32;
            } else {
              candidate = val_le16;
            }

            // Candidate Validation Check
            if (candidate > 0.0f && candidate < 100000.0f) {
              recordValidObdFrame();
              if (lastAccumulatedDischargeVal < 0.0f) {
                lastAccumulatedDischargeVal = candidate;
                monotonicCount = 0;
                Serial.printf("  -> [VP_PID_ACC_DISCHARGE] Validation initialized: baseline=%.3f kWh\n", candidate);
              } else {
                float diff = candidate - lastAccumulatedDischargeVal;
                // Monotonic check: diff >= 0, and reasonable delta (< 10 kWh)
                if (diff >= 0.0f && diff < 10.0f) {
                  monotonicCount++;
                  if (monotonicCount >= 3) {
                    if (!accumulatedDischargeValidated || accumulatedDischargeStart < 0.0f) {
                      // First promotion to a validated BMS counter: establish a real
                      // baseline atomically. Never subtract the initial sentinel (-1).
                      accumulatedDischargeStart = candidate;
                      Serial.printf("  -> [VP_PID_ACC_DISCHARGE] Validation PROMOTED: baseline established at %.3f kWh\n", candidate);
                    }
                    accumulatedDischargeValidated = true;
                  }
                  lastAccumulatedDischargeVal = candidate;
                  Serial.printf("  -> [VP_PID_ACC_DISCHARGE] Monotonic check OK: diff=%.3f, count=%d, validated=%d\n",
                                diff, monotonicCount, accumulatedDischargeValidated);
                } else {
                  accumulatedDischargeValidated = false;
                  monotonicCount = 0;
                  accumulatedDischargeStart = -1.0f;
                  lastAccumulatedDischargeVal = candidate;
                  Serial.printf("  -> [VP_PID_ACC_DISCHARGE] Validation FAILED (glitch or non-monotonic): diff=%.3f, resetting baseline to %.3f kWh\n",
                                diff, candidate);
                }
              }
            } else {
              accumulatedDischargeValidated = false;
              monotonicCount = 0;
              accumulatedDischargeStart = -1.0f;
              Serial.printf("  -> [VP_PID_ACC_DISCHARGE] Validation FAILED (candidate out of bounds): candidate=%.3f kWh\n", candidate);
            }

            accumulatedDischargeNow = candidate;
            updateTripEnergyValues();
            Serial.printf("  -> [VP_PID_ACC_DISCHARGE] le16=%.1f be16=%.1f le32=%.1f be32=%.1f cand=%.3f validated=%d\n",
                          val_le16, val_be16, val_le32, val_be32, candidate, accumulatedDischargeValidated);
          }

          // ── PID VP_PID_SOC: Battery SOC (BMS Coarse) ──
          // Protocol: UDS Service 22 (ReadDataByIdentifier)
          //
          // [FIX 1] Response Transaction Isolation:
          //   Execute SOC parser ONLY when the currently processed response
          //   belongs to command VP_REQ_SOC. Responses from other PIDs containing
          //   coincidental VP_RESP_SOC in payload MUST NOT enter this parser.
          if (lastSentCommand == VP_REQ_SOC) {
            int idx620005 = cleanResp.indexOf(VP_RESP_SOC);
            bool socParsed = false;

            if (idx620005 != -1 && cleanResp.length() >= (uint)(idx620005 + 8)) {
              String hexA = cleanResp.substring(idx620005 + 6, idx620005 + 8);

              // [R9] Validate hex string before conversion
              bool hexValid = (hexA.length() == 2)
                           && isxdigit((unsigned char)hexA[0])
                           && isxdigit((unsigned char)hexA[1]);

              if (hexValid) {
                int byteA = strtol(hexA.c_str(), NULL, 16);

                if (byteA >= 0 && byteA <= 100) {
                  // [R10] Frame verified: VP_RESP_SOC present + valid SOC byte (0x00..0x64)
                  // [R2]  Single shared timestamp — one millis() call for this frame
                  uint32_t socRxMs      = millis();
                  socBmsCoarse          = (float)byteA;
                  socCoarseValid        = true;        // [R7] parser-pass flag only
                  socCoarseLastUpdateMs = socRxMs;     // source timestamp
                  socCandidate          = socBmsCoarse; // backward-compat legacy only
                  obdLastGoodFrameMs    = socRxMs;     // same frame, same timestamp

                  Serial.printf("  -> [VP_PID_SOC] SOC Coarse: %.2f%%\n", socBmsCoarse);
                  socParsed = true;
                } else {
                  Serial.printf("  -> [VP_PID_SOC] byteA out of range: %d (not updating state)\n", byteA);
                }
              } else {
                Serial.printf("  -> [VP_PID_SOC] Invalid hex string '%s' (not updating state)\n", hexA.c_str());
              }
            }

            if (!socParsed) {
              // Malformed / negative response / hex error / out-of-range:
              // [R7]  socCoarseValid NOT set to false — it retains its last state
              // [R8]  socCoarseLastUpdateMs NOT refreshed — SOC will expire naturally
              // [P]   obdLastGoodFrameMs NOT refreshed
              if (idx620005 == -1) {
                Serial.printf("  -> [VP_PID_SOC] No positive response in frame (not updating state)\n");
              }
            }

            // [FIX 1.1 / R13 / R15] Immediate promotion call:
            // resolveBydSoc() runs immediately after VP_PID_SOC parser ONLY when
            // lastSentCommand == VP_REQ_SOC, so newly received SOC becomes visible
            // in batt_soc within the exact same cycle.
            resolveBydSoc();
          }

          // ── PID VP_PID_ACC_CHARGE: BMS Accumulated Charge Energy ──
          int idx620011 = cleanResp.indexOf(VP_RESP_ACC_CHARGE);
          if (idx620011 != -1) {
            Serial.printf("[VP_PID_ACC_CHARGE] Raw CAN: %s\n", cleanResp.c_str());
            if (cleanResp.length() >= (uint)(idx620011 + 10)) {
              String hexA = cleanResp.substring(idx620011 + 6, idx620011 + 8);
              String hexB = cleanResp.substring(idx620011 + 8, idx620011 + 10);
              int byteA = strtol(hexA.c_str(), NULL, 16);
              int byteB = strtol(hexB.c_str(), NULL, 16);

              float val_le16 = (byteA + byteB * 256.0f);
              float val_be16 = (byteB + byteA * 256.0f);
              float val_le32 = 0.0f;
              float val_be32 = 0.0f;

              if (cleanResp.length() >= (uint)(idx620011 + 14)) {
                String hexC = cleanResp.substring(idx620011 + 10, idx620011 + 12);
                String hexD = cleanResp.substring(idx620011 + 12, idx620011 + 14);
                int byteC = strtol(hexC.c_str(), NULL, 16);
                int byteD = strtol(hexD.c_str(), NULL, 16);
                val_le32 = (byteA + byteB * 256.0f + byteC * 65536.0f + byteD * 16777216.0f);
                val_be32 = (byteD + byteC * 256.0f + byteB * 65536.0f + byteA * 16777216.0f);
              }

              float candidate = 0.0f;
              if (val_le32 > 10000.0f) {
                candidate = val_le32 / 1000.0f; // Wh -> kWh
              } else if (val_le32 > 0.0f) {
                candidate = val_le32;
              } else {
                candidate = val_le16;
              }

              if (candidate > 0.0f && candidate < 100000.0f) {
                recordValidObdFrame();
                if (lastAccumulatedChargeVal < 0.0f) {
                  lastAccumulatedChargeVal = candidate;
                  monotonicChargeCount = 0;
                  Serial.printf("  -> [VP_PID_ACC_CHARGE] Validation initialized: baseline=%.3f kWh\n", candidate);
                } else {
                  float diff = candidate - lastAccumulatedChargeVal;
                  if (diff >= 0.0f && diff < 10.0f) {
                    monotonicChargeCount++;
                    if (monotonicChargeCount >= 3) {
                      accumulatedChargeValidated = true;
                    }
                    lastAccumulatedChargeVal = candidate;
                    
                    if (accumulatedChargeValidated) {
                      accumulatedChargeNow = candidate;
                      if (accumulatedChargeStart < 0.0f) {
                        accumulatedChargeStart = candidate;
                      }
                      if (lastChargeEnergySampleKwh > 0.0f) {
                        float delta = candidate - lastChargeEnergySampleKwh;
                        if (delta >= 0.01f && delta < 10.0f) {
                          lastChargeEnergyIncreaseMs = millis();
                        }
                      }
                      lastChargeEnergySampleKwh = candidate;
                    }
                    Serial.printf("  -> [VP_PID_ACC_CHARGE] Monotonic check OK: diff=%.3f, count=%d, validated=%d, chargeNow=%.3f\n",
                                  diff, monotonicChargeCount, accumulatedChargeValidated, accumulatedChargeNow);
                  } else {
                    monotonicChargeCount = 0;
                    accumulatedChargeValidated = false;
                    lastAccumulatedChargeVal = candidate;
                    Serial.printf("  -> [VP_PID_ACC_CHARGE] Large change/drop ignored: diff=%.3f\n", diff);
                  }
                }
              }
            }
          }

          // ── PID VP_PID_CHARGE_COUNT: [BASU] Charge times () ──
          int idx62010A = cleanResp.indexOf(VP_RESP_CHARGE_COUNT);
          if (idx62010A != -1) {
            Serial.printf("[VP_PID_CHARGE_COUNT] Raw CAN: %s\n", cleanResp.c_str());
            if (cleanResp.length() >= (uint)(idx62010A + 10)) {
              String hexA = cleanResp.substring(idx62010A + 6, idx62010A + 8);
              String hexB = cleanResp.substring(idx62010A + 8, idx62010A + 10);
              int byteA = strtol(hexA.c_str(), NULL, 16);
              int byteB = strtol(hexB.c_str(), NULL, 16);
              int countVal = (byteA * 256) + byteB;
              if (countVal >= 0 && countVal < 65535) {
                basuChargeCount = countVal;
                recordValidObdFrame();
                Serial.printf("  -> [VP_PID_CHARGE_COUNT] Candidate parsed: %d\n", basuChargeCount);
                
                if (lastSavedChargeCount <= 0) {
                  lastSavedChargeCount = basuChargeCount;
                  preferences.begin("obd2_dash", false);
                  preferences.putInt("chg_count", lastSavedChargeCount);
                  preferences.end();
                  Serial.printf("  -> Initialized lastSavedChargeCount to: %d\n", lastSavedChargeCount);
                }
              }
            }
          }

          // ── PID VP_PID_MOTOR_RPM: Motor RPM (MMCU Debug Candidate) ──
          // ECU: MCU (response header = request + 8)
          // Response: <MCU resp hdr> 05 62 <PID> [byteA] [byteB]
          //
          // ⚠ STATUS: DEBUG CANDIDATE — NOT yet confirmed as
          // [MMCU] Main Drive motor RPM. Formula is approximate.
          //
          // Formula: use ONLY byteA, ignore byteB.
          // Motor RPM = (byteA - 136) * 100
          // Stationary zero point: byteA = 0x88 (136)
          int idx62000B = cleanResp.indexOf(VP_RESP_MOTOR_RPM);
          if (idx62000B != -1 && cleanResp.length() >= (uint)(idx62000B + 10)) {
            String hexA = cleanResp.substring(idx62000B + 6, idx62000B + 8);
            String hexB = cleanResp.substring(idx62000B + 8, idx62000B + 10);
            int byteA = strtol(hexA.c_str(), NULL, 16);
            int byteB = strtol(hexB.c_str(), NULL, 16);

            // MMCU debug: compute raw signed RPM
            int rpmValSigned = (byteA - 136) * 100;
            mmcuRpmRaw = rpmValSigned;
            mmcuRpmDisplayAbs = abs(rpmValSigned);
            if (mmcuRpmDisplayAbs > 16000) mmcuRpmDisplayAbs = 16000;
            mmcuResponseCount++;
            mmcuLastUpdateMs = millis();
            recordValidObdFrame();

            // Validate: check if value is plausible
            bool isMoving = (vehicle_speed > 3);
            if (mmcuRpmDisplayAbs == 0 && isMoving) {
              mmcuRpmValid = false;
              mmcuLastError = "zero_while_moving";
            } else if (mmcuRpmDisplayAbs > 16000) {
              mmcuRpmValid = false;
              mmcuLastError = "invalid_scale";
            } else {
              mmcuRpmValid = true;
              mmcuLastError = "ok";
            }

            // Apply MMCU RPM based on source mode
            if (rpmSourceMode == 2) {
              // MMCU mode: use actual MMCU only, NO fallback
              if (mmcuRpmValid) {
                motor_rpm = mmcuRpmDisplayAbs;
                displayTargetRpm = (float)mmcuRpmDisplayAbs;
                rpmSourceLabel = "REAL_MMCU";
              } else {
                motor_rpm = 0; // show 0 / invalid — no fallback
                displayTargetRpm = 0.0f;
                rpmSourceLabel = "MMCU_NO_DATA";
              }
              lastRpmUpdateTime = millis();
            } else if (rpmSourceMode == 0 && mmcuRpmValid) {
              // AUTO mode: MMCU is valid, use it
              motor_rpm = mmcuRpmDisplayAbs;
              displayTargetRpm = (float)mmcuRpmDisplayAbs;
              rpmSourceLabel = "REAL_MMCU";
              lastRpmUpdateTime = millis();
            }
            // EST mode (1): ignore MMCU data entirely for motor_rpm

            lastRpmActualTime = millis(); // Always track MMCU response time

            Serial.printf("  -> RPM (VP_PID_MOTOR_RPM): byteA=0x%s(%d) byteB=0x%s(%d) "
                          "rawSigned=%d absDisplay=%d valid=%s err=%s "
                          "respCount=%d src=%s\n",
                          hexA.c_str(), byteA, hexB.c_str(), byteB,
                          mmcuRpmRaw, mmcuRpmDisplayAbs,
                          mmcuRpmValid ? "true" : "false",
                          mmcuLastError.c_str(),
                          mmcuResponseCount, rpmSourceLabel.c_str());
          } else if (lastSentCommand == VP_REQ_MOTOR_RPM) {
            // VP_PID_MOTOR_RPM was sent but no valid response in this data
            mmcuLastError = "timeout";
            mmcuRpmValid = false;
            if (rpmSourceMode == 2) {
              motor_rpm = 0;
              rpmSourceLabel = "MMCU_NO_DATA";
            }
          }

          // ── PID VP_PID_AUX: Vehicle Speed (km/h) ──
          // ECU: MCU (response header = request + 8)
          // Response: <MCU resp hdr> 05 62 <PID> [byteA] [byteB]
          //
          // ⚠ LOG ANALYSIS NOTE (from internal road-test log, not published):
          // The log was recorded while the car was PARKED.
          // byteB was observed to be a slowly-increasing counter (0x4A -> 0x5C)
          // during the 2-minute parked session. It is NOT part of the speed.
          //
          // Currently using formula: speed = (byteB*256 + byteA - 20000) / 100
          // This gives -8 to +5 km/h while stationary (noise), clamped to 0.
          // SHOWING 0 WHILE PARKED IS CORRECT.
          //
          // To fix for actual driving: need a log taken while driving at
          // known speeds (e.g. 40 km/h, 80 km/h) to reverse-engineer formula.
          // A stability/smoothing filter is applied here to reduce jitter.
          int idx62000A = cleanResp.indexOf(VP_RESP_AUX);
          if (idx62000A != -1 && cleanResp.length() >= (uint)(idx62000A + 10)) {
            String hexA = cleanResp.substring(idx62000A + 6, idx62000A + 8);
            String hexB = cleanResp.substring(idx62000A + 8, idx62000A + 10);
            int byteA = strtol(hexA.c_str(), NULL, 16);
            int byteB = strtol(hexB.c_str(), NULL, 16);

            // 16-bit little-endian raw value
            int rawVal = (byteB * 256) + byteA;

            // Exponential moving average to smooth out the byteB counter jitter
            if (speed_raw_smooth < 0.0f) {
              speed_raw_smooth = (float)rawVal; // first reading
            } else {
              speed_raw_smooth = speed_raw_smooth * 0.7f + (float)rawVal * 0.3f;
            }

            // Apply formula — subtract observed stationary center (~19800)
            // and scale by 100. This is approximate until confirmed with
            // a driving log.
            int speedVal = (int)((speed_raw_smooth - 19800.0f) / 100.0f);
            if (speedVal < 0) speedVal = 0;
            if (speedVal > 250) speedVal = 250;

            // vehicle_speed = speedVal; // Disabled in TEST1
            lastRpmUpdateTime = millis();

            Serial.printf("  -> Speed (VP_PID_AUX): byteA=%s byteB=%s raw=%d smooth=%.0f speed=%d km/h\n",
                          hexA.c_str(), hexB.c_str(), rawVal,
                          speed_raw_smooth, speedVal);
          }

          // Standard OBD-II PID 010C response: Engine/Motor RPM
          // Response format: 41 0C AA BB -> RPM = (AA*256 + BB) / 4
          int idx410C = cleanResp.indexOf("410C");
          if (idx410C != -1) {
            if (cleanResp.length() >= (uint)(idx410C + 8)) {
              String hexA = cleanResp.substring(idx410C + 4, idx410C + 6);
              String hexB = cleanResp.substring(idx410C + 6, idx410C + 8);
              int byteA = strtol(hexA.c_str(), NULL, 16);
              int byteB = strtol(hexB.c_str(), NULL, 16);
              motor_rpm = ((byteA * 256) + byteB) / 4;
              displayTargetRpm = (float)motor_rpm;
              lastRpmUpdateTime = millis();
              recordValidObdFrame();
              Serial.print("  -> RPM (OBD 010C): ");
              Serial.println(motor_rpm);
            }
          }

          // Standard OBD-II PID 010D response: Vehicle Speed
          // Response format: 41 0D AA -> Speed = AA km/h
          int idx410D = cleanResp.indexOf("410D");
          if (idx410D != -1) {
            if (cleanResp.length() >= (uint)(idx410D + 6)) {
              String hexStr = cleanResp.substring(idx410D + 4, idx410D + 6);
              int speedVal = strtol(hexStr.c_str(), NULL, 16);
              vehicle_speed = speedVal;
              vehicleSpeedKmh = (float)speedVal;
              displayTargetSpeed = (float)speedVal;
              lastRpmUpdateTime = millis();
              recordValidObdFrame();
              Serial.print("  -> Vehicle Speed (OBD 010D): ");
              Serial.print(speedVal);
              Serial.println(" km/h");
            }
          }
        }
        break; // Break since we processed a full command response
      } else {
        currentResponseBuffer += c;
      }
    }

    // Handle non-blocking timeout
    if (isWaitingForResponse && (millis() - commandSentTime > 1000)) {
      Serial.print("[WARN] Non-blocking timeout for command: ");
      Serial.println(lastSentCommand);

      consecutiveTimeouts++;
      if (consecutiveTimeouts >= 10) {
        Serial.println(
            "[WARN] Too many consecutive timeouts. Reconnecting socket...");
        consecutiveTimeouts = 0;
        obdClient.stop();
      }

      if (!lastSentCommand.startsWith("AT")) {
        consecutiveCanFailures++;
        Serial.print("[INFO] CAN Timeout. Consecutive CAN Failures: ");
        Serial.println(consecutiveCanFailures);
        if (consecutiveCanFailures >= 30) {
          motor_rpm = 0;
        }
      }

      isWaitingForResponse = false;
      nextCommandSendTime = millis() + 100;
    }
  }
}

// ============================================================
// Top-Level Connection Handler (Wi-Fi Supervisor Orchestrator)
// ============================================================
void handleConnection() {
  // Zero-speed display smoothing updates (contain NO early return statements)
  // IMPORTANT:
  // motor_rpm / vehicle_speed zeroing follows telemetry freshness (3000ms).
  // displayTargetSpeed / displayTargetRpm smoothing follows the independent
  // display timeout (5000ms).
  // These two behaviors are intentionally different.
  if (!isObdDataFresh()) {
    motor_rpm = 0;
    vehicle_speed = 0;
    vehicleSpeedKmh = 0.0f;
  }

  if (obdLastGoodFrameMs == 0 || ((uint32_t)(millis() - obdLastGoodFrameMs) > DISPLAY_ZERO_SPEED_TIMEOUT_MS)) {
    displayTargetSpeed = 0.0f;
    displayTargetRpm = 0.0f;
  }

  // 1. Run STA Link Manager (Always active to maintain connection & consume events)
  updateStaManager();

  // 2. Run TCP Socket & ELM Protocol Handshake Manager
  updateTcpAndElmManager();

  // 3. Run OBD PID Polling Engine (Active when confirmed)
  updateObdPollingEngine();

  // ── Single-writer: compute and assign obdHealthState once per loop ────────
  // INVARIANT: Unconditionally reached on every call — no early return may bypass this block.
  ObdHealthState newHealth = getObdHealthState();

  // Transition counters (serial / /api/status debug only - strictly on rising edges)
  if (newHealth == OBD_HEALTH_WARNING      && obdHealthState != OBD_HEALTH_WARNING) {
    obdHealthWarningCount++;
  }
  if (newHealth == OBD_HEALTH_DISCONNECTED && obdHealthState != OBD_HEALTH_DISCONNECTED) {
    obdHealthDisconnectCount++;
  }

  // ── Full recovery confirmed: active drop cycle reaches CONNECTED ─────────
  // RECOVERED = Full OBD health recovered (WiFi + TCP + ELM + fresh telemetry).
  // Decoupled from health rising edge: fires reliably even if reconnection completed same-loop.
  // wifiDropRecoveryActive is the sole lifecycle flag.
  // At most 1 [RECOVERED] emitted per drop cycle; exactly 1 only when full recovery reached.
  if (newHealth == OBD_HEALTH_CONNECTED && wifiDropRecoveryActive) {
    uint32_t totalDowntime = (uint32_t)(millis() - wifiDropRecoveryStartMs);
    Serial.printf(
      "[WIFI][STA][RECOVERED] totalDowntime=%lums dropCount=%u recoveryStartMs=%lu lastDropMs=%lu\n"
      "  newChan=%u rssi=%d tcp=%d elm=%d dataFresh=%d health=CONNECTED\n",
      (unsigned long)totalDowntime,
      (unsigned)wifiDropCount,
      (unsigned long)wifiDropRecoveryStartMs,
      (unsigned long)wifiLastDropMs,
      (unsigned)WiFi.channel(),
      WiFi.RSSI(),
      (int)currentTcpState,
      (int)currentElmState,
      (int)isObdDataFresh()
    );
    obdHealthRecoverySuccessCount++;
    wifiDropRecoveryActive = false; // Cycle complete: unlatch
    wifiDropRecoveryStartMs = 0;
  }

  obdHealthState = newHealth;
}
