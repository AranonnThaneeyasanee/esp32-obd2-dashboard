#pragma once

// =============================================================================
// Vehicle Calibration Profile  —  EXAMPLE / PLACEHOLDER VALUES
// =============================================================================
//
//  ⚠️  ค่าทุกตัวในไฟล์นี้เป็น "ค่าตัวอย่าง" ที่ใส่ไว้ให้โปรเจคคอมไพล์ผ่านเท่านั้น
//      ไม่ใช่ค่าที่ใช้งานจริง และจะไม่สามารถอ่านข้อมูลจากรถคันจริงได้
//
//  ค่าจริงประกอบด้วย manufacturer-specific UDS PID, ECU request header
//  และสัมประสิทธิ์การแปลงหน่วย ซึ่งได้มาจากการเก็บ log ขณะขับทดสอบบนถนนจริง
//  แล้วนำมาวิเคราะห์หาความสัมพันธ์ระหว่างค่าดิบกับค่าที่วัดได้จริง
//  ถือเป็นทรัพย์สินทางปัญญาของผู้พัฒนา จึงไม่ได้เผยแพร่พร้อมซอร์สโค้ดชุดนี้
//
// -----------------------------------------------------------------------------
//  วิธีใช้งานกับรถของคุณเอง
// -----------------------------------------------------------------------------
//  1. คัดลอกไฟล์นี้เป็น  src/vehicle_calibration.h
//  2. แทนค่าด้านล่างด้วยค่าที่ได้จากการวิเคราะห์รถของคุณ
//  3. คอมไพล์ตามปกติ — dashboard_config.h จะเลือกใช้ไฟล์ของคุณโดยอัตโนมัติ
//
//  (ไฟล์ vehicle_calibration.h ถูกกำหนดไว้ใน .gitignore แล้ว จะไม่ถูก commit)
//
//  โหมด "EV Generic" และ "ICE Generic" ในตัวโปรแกรมใช้ PID มาตรฐาน OBD-II
//  (SAE J1979 — 010C, 010D, 0105, 012F) ซึ่งเป็นสเปคสาธารณะ จึงทำงานได้ทันที
//  กับรถทั่วไปโดยไม่ต้องตั้งค่าไฟล์นี้
// =============================================================================


// ── 1. ECU Request Headers (ใช้กับคำสั่ง ATSH) ────────────────────────────────
// กำหนดว่าจะยิงคำถามไปหา ECU ตัวไหนบนบัส CAN
// Response frame จะกลับมาด้วย header = request header + 8
#define VP_HDR_BMS              "700"   // ← placeholder (Battery Management System)
#define VP_HDR_MCU              "701"   // ← placeholder (Motor Control Unit)
#define VP_HDR_SPEED            "702"   // ← placeholder (ECU ที่ถือค่าความเร็ว)


// ── 2. Manufacturer-Specific PID Suffixes (UDS Service 0x22) ─────────────────
// คำสั่งที่ส่งออกไปคือ "22" + suffix   ส่วน response ที่ตอบกลับคือ "62" + suffix
#define VP_PID_SPEED            "0001"  // ← placeholder
#define VP_PID_SOC              "0002"  // ← placeholder
#define VP_PID_HV_VOLTAGE       "0003"  // ← placeholder
#define VP_PID_HV_CURRENT       "0004"  // ← placeholder
#define VP_PID_MOTOR_RPM        "0005"  // ← placeholder
#define VP_PID_ENERGY           "0006"  // ← placeholder
#define VP_PID_MOTOR_TEMP       "0007"  // ← placeholder
#define VP_PID_ACC_CHARGE       "0008"  // ← placeholder
#define VP_PID_ACC_DISCHARGE    "0009"  // ← placeholder
#define VP_PID_BATT_TEMP        "000A"  // ← placeholder
#define VP_PID_SOH              "000B"  // ← placeholder
#define VP_PID_CHARGE_COUNT     "000C"  // ← placeholder
#define VP_PID_AUX              "000D"  // ← placeholder (ช่องทดลอง)


// ── 3. Scaling Coefficients ──────────────────────────────────────────────────
// สัมประสิทธิ์แปลงค่าดิบจาก CAN frame เป็นหน่วยที่ใช้งานจริง
// ทั้งหมดนี้ได้จากการ fit ค่าดิบกับค่าอ้างอิงที่วัดได้ขณะขับทดสอบ

// ความเร็ว: speedKmh = rawValue / VP_SPEED_DIVISOR
#define VP_SPEED_DIVISOR        1.0f    // ← placeholder (ค่าจริงไม่ใช่ 1.0)

// รอบมอเตอร์โดยประมาณ: rpm = speedKmh * VP_RPM_FACTOR
// (ค่าจริงสะท้อนอัตราทดเกียร์ + เส้นรอบวงล้อของรถรุ่นที่รองรับ)
#define VP_RPM_FACTOR           1.0f    // ← placeholder (ค่าจริงไม่ใช่ 1.0)

// สุขภาพแบตเตอรี่: SOH% = (ความจุที่วัดได้ / VP_SOH_NOMINAL_AH) * 100
#define VP_SOH_NOMINAL_AH       100.0f  // ← placeholder (Ah ตามสเปคโรงงาน)

// กระแส HV เป็นค่าแบบมี offset: ampere = (raw - VP_HV_CURRENT_OFFSET) / 10
// ค่าที่ต่ำกว่า offset = กำลังชาร์จ, สูงกว่า = กำลังจ่ายไฟ
#define VP_HV_CURRENT_OFFSET    0.0f    // ← placeholder

// แรงดัน HV: volt = rawLittleEndian / VP_HV_VOLTAGE_DIVISOR
#define VP_HV_VOLTAGE_DIVISOR   1.0f    // ← placeholder


// =============================================================================
//  ส่วนล่างนี้เป็นค่าที่ประกอบขึ้นอัตโนมัติ — ไม่ต้องแก้
// =============================================================================

// คำสั่งที่ส่งออก (UDS Service 0x22 = ReadDataByIdentifier)
#define VP_REQ_SPEED            ("22" VP_PID_SPEED)
#define VP_REQ_SOC              ("22" VP_PID_SOC)
#define VP_REQ_HV_VOLTAGE       ("22" VP_PID_HV_VOLTAGE)
#define VP_REQ_HV_CURRENT       ("22" VP_PID_HV_CURRENT)
#define VP_REQ_MOTOR_RPM        ("22" VP_PID_MOTOR_RPM)
#define VP_REQ_ENERGY           ("22" VP_PID_ENERGY)
#define VP_REQ_MOTOR_TEMP       ("22" VP_PID_MOTOR_TEMP)
#define VP_REQ_ACC_CHARGE       ("22" VP_PID_ACC_CHARGE)
#define VP_REQ_ACC_DISCHARGE    ("22" VP_PID_ACC_DISCHARGE)
#define VP_REQ_BATT_TEMP        ("22" VP_PID_BATT_TEMP)
#define VP_REQ_SOH              ("22" VP_PID_SOH)
#define VP_REQ_CHARGE_COUNT     ("22" VP_PID_CHARGE_COUNT)

// Prefix ของ response ที่ parser ใช้ค้นหาในสตริง (0x22 + 0x40 = 0x62)
#define VP_RESP_SPEED           ("62" VP_PID_SPEED)
#define VP_RESP_SOC             ("62" VP_PID_SOC)
#define VP_RESP_HV_VOLTAGE      ("62" VP_PID_HV_VOLTAGE)
#define VP_RESP_HV_CURRENT      ("62" VP_PID_HV_CURRENT)
#define VP_RESP_MOTOR_RPM       ("62" VP_PID_MOTOR_RPM)
#define VP_RESP_ENERGY          ("62" VP_PID_ENERGY)
#define VP_RESP_MOTOR_TEMP      ("62" VP_PID_MOTOR_TEMP)
#define VP_RESP_ACC_CHARGE      ("62" VP_PID_ACC_CHARGE)
#define VP_RESP_ACC_DISCHARGE   ("62" VP_PID_ACC_DISCHARGE)
#define VP_RESP_BATT_TEMP       ("62" VP_PID_BATT_TEMP)
#define VP_RESP_SOH             ("62" VP_PID_SOH)
#define VP_RESP_CHARGE_COUNT    ("62" VP_PID_CHARGE_COUNT)
#define VP_RESP_AUX             ("62" VP_PID_AUX)

// คำสั่งตั้ง header
#define VP_ATSH_BMS             ("ATSH" VP_HDR_BMS)
#define VP_ATSH_MCU             ("ATSH" VP_HDR_MCU)
#define VP_ATSH_SPEED           ("ATSH" VP_HDR_SPEED)
