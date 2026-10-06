/*
  ===========================================================================
  Project OBD2: ESP32-S3 Zero - Vgate OBD2 Wi-Fi Simulator
  ===========================================================================
  This board simulates an OBD2 Wi-Fi Adapter (like vGate iCar Pro WiFi)
  It broadcasts a Wi-Fi Hotspot and runs a TCP Server on port 35000.
  Applications or client boards (like ESP32-S3 Dashboard) can connect to query 12V 
  battery voltage and other data.
  ===========================================================================
*/

#include <WiFi.h>

const char* ap_ssid = "OBD2_Simulator";
const char* ap_password = ""; // Open network (easy connection, no password)
const uint16_t tcp_port = 35000;

WiFiServer server(tcp_port);
WiFiClient client;

// ============================================================
// Simulated vehicle data variables (accurate to 2 decimal places)
// ============================================================
float batt12v = 13.85;
float soc = 85.0;
float soh = 98.4;
float hv_volt = 380.0;
int speed_kmh = 0;
int rpm = 0;
bool dc_dc_active = true;

// Added simulated EV Temperature values
float pack_temp = 32.4;
float motor_temp = 68.5;
int energy_total = 2337; // Cumulative energy in 0.01 kWh units (23.37 kWh starting point)

String inputBuffer = "";
unsigned long lastUpdate = 0;
unsigned long lastStateChange = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("=========================================");
  Serial.println("ESP32-S3 Zero: OBD2 Wi-Fi Simulator Init!");
  Serial.println("=========================================");

  // Initialize Wi-Fi Access Point safely
  WiFi.disconnect();
  delay(100);
  WiFi.mode(WIFI_AP);
  
  // Keep the simulator on the same non-overlapping channel used by the
  // dashboard setup AP. ESP32 AP+STA coexistence requires both interfaces
  // to share a channel, so matching the test AP prevents channel migration.
  constexpr uint8_t SIMULATOR_AP_CHANNEL = 11;
  if (ap_password == NULL || strlen(ap_password) == 0) {
    WiFi.softAP(ap_ssid, nullptr, SIMULATOR_AP_CHANNEL);
  } else {
    WiFi.softAP(ap_ssid, ap_password, SIMULATOR_AP_CHANNEL);
  }
  
  server.begin();

  Serial.print("Access Point Created! SSID: ");
  Serial.println(ap_ssid);
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("TCP Port: ");
  Serial.println(tcp_port);
  Serial.println("Waiting for connections...");
}

// ============================================================
// Update simulated car status (driving and charging cycles)
// ============================================================
void updateSimulation() {
  unsigned long now = millis();
  if (now - lastUpdate > 100) { // Update every 100ms
    lastUpdate = now;

    // Toggle Charging state every 30 seconds
    if (now - lastStateChange > 30000) {
      lastStateChange = now;
      dc_dc_active = !dc_dc_active;
      Serial.print("DC-DC Converter Status Changed to: ");
      Serial.println(dc_dc_active ? "ACTIVE (Charging)" : "STANDBY (Discharging)");
    }

    // Simulate 12V battery voltage fluctuations with minor noise
    float noise = (random(-15, 15) / 100.0); // ±0.15V fluctuation
    if (dc_dc_active) {
      batt12v = 13.92 + (sin(now / 5000.0) * 0.1) + noise; // Charging wave
      if (batt12v > 14.3) batt12v = 14.25;
      if (batt12v < 13.6) batt12v = 13.72;
    } else {
      batt12v = 12.75 + (sin(now / 5000.0) * 0.05) + noise; // Standby depletion
      if (batt12v > 13.1) batt12v = 13.05;
      if (batt12v < 12.3) batt12v = 12.45;
    }

    // Simulate speed cycle
    static int dir = 1;
    if (dir == 1) {
      speed_kmh += random(0, 3);
      if (speed_kmh >= 85) dir = -1;
    } else {
      speed_kmh -= random(0, 3);
      if (speed_kmh <= 0) {
        speed_kmh = 0;
        dir = 1;
      }
    }
    
    rpm = speed_kmh * 42;
    soc = 85.0 - (now / 400000.0);
    if (soc < 5.0) soc = 95.0; // Reset SoC simulation loop
    
    soh = 98.4 - (now / 1200000.0);
    if (soh < 75.0) soh = 98.4;
    
    hv_volt = 380.0 - (speed_kmh * 0.15);

    // Simulate Battery Pack Temperature (slow sinusoidal drift: 30C to 40C)
    pack_temp = 34.0 + (sin(now / 15000.0) * 3.5) + (random(-10, 10) / 100.0);

    // Simulate Electric Motor Temperature (scales with Speed: 55C to 95C)
    float baseTemp = 60.0 + (speed_kmh * 0.32);
    motor_temp = baseTemp + (sin(now / 8000.0) * 2.5) + (random(-20, 20) / 100.0);

    // Simulate cumulative energy use (increment 1 unit = 0.01 kWh every ~6 seconds)
    static unsigned long lastEnergyInc = 0;
    if (now - lastEnergyInc > 6000) {
      lastEnergyInc = now;
      energy_total++;
      if (energy_total > 65535) energy_total = 2337; // Wrap around for simulation
    }
  }
}

// ============================================================
// ELM327 OBD2 Command Parser & Response Handler
// ============================================================
String handleCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return ">";

  Serial.print("--> Received Command: [");
  Serial.print(cmd);
  Serial.println("]");

  // 1. ELM327 Basic AT Commands
  if (cmd == "ATZ") {
    return "ELM327 v1.5\r\r>";
  }
  if (cmd.startsWith("ATE") || cmd.startsWith("ATL") || cmd.startsWith("ATH") || cmd.startsWith("ATS") || cmd.startsWith("ATSP")) {
    return "OK\r\r>";
  }
  if (cmd == "ATRV") {
    char buf[16];
    sprintf(buf, "%.2fV", batt12v); // Response matching decimal format
    return String(buf) + "\r\r>";
  }
  if (cmd.startsWith("AT")) {
    return "OK\r\r>";
  }

  // 2. Standard OBD2 PIDs
  if (cmd == "0100") {
    return "41 00 BE 3E 30 10\r\r>";
  }
  if (cmd == "010C") { // RPM
    int val = rpm * 4;
    byte a = (val >> 8) & 0xFF;
    byte b = val & 0xFF;
    char buf[32];
    sprintf(buf, "41 0C %02X %02X\r\r>", a, b);
    return String(buf);
  }
  if (cmd == "010D") { // Vehicle Speed
    char buf[32];
    sprintf(buf, "41 0D %02X\r\r>", speed_kmh & 0xFF);
    return String(buf);
  }

  // 3. BYD Custom UDS Commands (HV Battery & MCU details)
  if (cmd.startsWith("22") && cmd.length() == 6) {
    String sub = cmd.substring(2, 6);
    if (sub == "000A") { // Battery Temperature (example PID)
      // Formula: ByteA - 40 = Temperature in Celsius
      int val = (int)(pack_temp + 40.0);
      char buf[32];
      sprintf(buf, "62 00 32 %02X\r\r>", val & 0xFF);
      return String(buf);
    }
    if (sub == "0007") { // Motor Temperature (example PID)
      // Formula: ByteA - 40 = Motor Temp
      int val = (int)(motor_temp + 40.0);
      char buf[32];
      sprintf(buf, "62 00 0F %02X\r\r>", val & 0xFF);
      return String(buf);
    }
    if (sub == "0005") { // Motor RPM (example PID)
      // New correct formula: motor_rpm = ((ByteB * 256 + ByteA) - 5000) * 10 (Little-Endian)
      // So Little_Endian_Val = (motor_rpm / 10) + 5000
      int rawVal = (rpm / 10) + 5000;
      byte a = rawVal & 0xFF;        // Low byte (first byte sent)
      byte b = (rawVal >> 8) & 0xFF; // High byte (second byte sent)
      char buf[32];
      sprintf(buf, "62 00 0B %02X %02X\r\r>", a, b);
      return String(buf);
    }
    if (sub == "0001") { // Vehicle Speed (example PID)
      // New correct formula: vehicle_speed = ((ByteB * 256 + ByteA) - 20000) / 100.0 (Little-Endian)
      // So Little_Endian_Val = (vehicle_speed * 100) + 20000
      int rawVal = (speed_kmh * 100) + 20000;
      byte a = rawVal & 0xFF;        // Low byte (first byte sent)
      byte b = (rawVal >> 8) & 0xFF; // High byte (second byte sent)
      char buf[32];
      sprintf(buf, "62 00 0A %02X %02X\r\r>", a, b);
      return String(buf);
    }
    if (sub == "000B") { // SOH (State of Health) — example PID — Actual Capacity
      // Formula: ((ByteD*256)+ByteC)/100 = Actual Capacity Ah. SOH = (Ah/100.0)*100
      float currentAh = (soh / 100.0) * 100.0;
      int val = (int)(currentAh * 100.0);
      byte c = val & 0xFF;
      byte d = (val >> 8) & 0xFF;
      char buf[32];
      // Byte A and B are usually SOC, we'll just simulate 76.00% (1D B0)
      sprintf(buf, "62 1F FC B0 1D %02X %02X\r\r>", c, d);
      return String(buf);
    }
    if (sub == "0002") { // SOC (State of Charge)
      // Formula: ByteA = SOC%
      int val = (int)soc; 
      char buf[32];
      sprintf(buf, "62 00 05 %02X\r\r>", val & 0xFF);
      return String(buf);
    }
    if (sub == "0006") { // Accumulated Energy Use
      // Formula: ((ByteB*256)+ByteA) / 100.0 = cumulative kWh
      // Dashboard computes trip kWh = current - baseline (first reading)
      int val = energy_total;
      byte a = val & 0xFF;
      byte b = (val >> 8) & 0xFF;
      char buf[32];
      sprintf(buf, "62 00 0E %02X %02X\r\r>", a, b);
      return String(buf);
    }
  }

  return "NO DATA\r\r>";
}

void loop() {
  // Continuous simulation update
  updateSimulation();

  // Manage incoming client connections (always accept new connections to kick out zombie ones)
  WiFiClient newClient = server.available();
  if (newClient) {
    if (client) {
      Serial.println("\n>>> Old S3 Dashboard Connection Terminated.");
      client.stop(); // Kick out the old connection
    }
    client = newClient;
    Serial.println("\n>>> S3 Dashboard Connected!");
    client.print(">\r"); // Send prompt to start communication
  }

  if (client && client.connected()) {
    while (client.available()) {
      char c = client.read();
      if (c == '\r' || c == '\n') {
        inputBuffer.trim();
        if (inputBuffer.length() > 0) {
          String response = handleCommand(inputBuffer);
          client.print(response);
          inputBuffer = "";
        }
      } else {
        if (c != ' ') {
          inputBuffer += (char)toupper(c);
        }
      }
    }
  } else if (client) {
    // If client disconnected abruptly
    Serial.println("\n>>> S3 Dashboard Disconnected.");
    client.stop();
  }
}
