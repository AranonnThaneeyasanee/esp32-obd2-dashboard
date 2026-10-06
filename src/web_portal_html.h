#pragma once

// ============================================================
// HTML Landing Page (Captive Portal Lightweight Page)
// ============================================================
const char LANDING_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>OBD2 Dashboard</title>
  <style>
    body {
      background-color: #0b0f19;
      color: #f3f4f6;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      margin: 0;
      padding: 20px;
      display: flex;
      flex-direction: column;
      align-items: center;
      min-height: 100vh;
      justify-content: center;
      box-sizing: border-box;
    }
    .container {
      max-width: 450px;
      width: 100%;
      background: #111827;
      border: 1px solid #1f2937;
      border-radius: 16px;
      padding: 32px 24px;
      box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.6), 0 10px 10px -5px rgba(0, 0, 0, 0.6);
      text-align: center;
      box-sizing: border-box;
      position: relative;
      overflow: hidden;
    }
    .container::before {
      content: "";
      position: absolute;
      top: 0;
      left: 0;
      right: 0;
      height: 4px;
      background: linear-gradient(90deg, #06b6d4, #3b82f6);
    }
    .logo-container {
      margin-bottom: 24px;
    }
    .logo-icon {
      font-size: 48px;
      animation: pulse 2s infinite ease-in-out;
      display: inline-block;
    }
    @keyframes pulse {
      0%, 100% { transform: scale(1); filter: drop-shadow(0 0 2px rgba(6,182,212,0.3)); }
      50% { transform: scale(1.05); filter: drop-shadow(0 0 12px rgba(6,182,212,0.8)); }
    }
    h1 {
      font-size: 22px;
      font-weight: 700;
      color: #f3f4f6;
      margin: 0 0 8px 0;
      text-transform: uppercase;
      letter-spacing: 1.5px;
    }
    h2 {
      font-size: 14px;
      font-weight: 400;
      color: #06b6d4;
      margin: 0 0 24px 0;
      letter-spacing: 1px;
    }
    .desc {
      font-size: 14px;
      color: #9ca3af;
      line-height: 1.6;
      margin-bottom: 32px;
    }
    .btn-link {
      display: block;
      background: linear-gradient(90deg, #06b6d4, #0891b2);
      color: #111827;
      text-decoration: none;
      padding: 14px 20px;
      font-weight: bold;
      border-radius: 8px;
      font-size: 15px;
      transition: all 0.3s ease;
      box-shadow: 0 4px 6px -1px rgba(6,182,212,0.2);
    }
    .btn-link:hover {
      background: linear-gradient(90deg, #22d3ee, #06b6d4);
      transform: translateY(-2px);
      box-shadow: 0 10px 15px -3px rgba(6,182,212,0.4);
    }
    .btn-link:active {
      transform: translateY(0);
    }
    .footer {
      margin-top: 32px;
      font-size: 11px;
      color: #4b5563;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="logo-container">
      <span class="logo-icon">🔌</span>
    </div>
    <h1>OBD2 Smart Dashboard</h1>
    <h2>WIFI SETUP PORTAL</h2>
    <p class="desc">
      ยินดีต้อนรับเข้าสู่ระบบตั้งค่า Smart Dashboard<br>
      กรุณาคลิกที่ปุ่มด้านล่างเพื่อดำเนินการตั้งค่า WiFi, เปลี่ยนภาพ GIF หรือตั้งค่าการดึงข้อมูล OBD2
    </p>
    <a href="http://192.168.4.1/setup" class="btn-link">เข้าสู่หน้าตั้งค่า (Go to Setup)</a>
    <div class="footer">
      Device Portal &copy; 2026 Smart OBD2 Dash
    </div>
  </div>
</body>
</html>
)rawliteral";

// ============================================================
// HTML Upload Portal Page
// ============================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>OBD2 Dashboard - Setup Portal</title>
  <style>
    body {
      background-color: #0b0f19;
      color: #f3f4f6;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      margin: 0;
      padding: 20px;
      display: flex;
      flex-direction: column;
      align-items: center;
      min-height: 100vh;
      justify-content: center;
      box-sizing: border-box;
    }
    .container {
      max-width: 500px;
      width: 100%;
      background: #111827;
      border: 1px solid #1f2937;
      border-radius: 12px;
      padding: 24px;
      box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.5);
      box-sizing: border-box;
    }
    h1 {
      font-size: 20px;
      color: #06b6d4;
      text-align: center;
      margin-top: 0;
      text-transform: uppercase;
      letter-spacing: 1.5px;
    }
    .section {
      background: #1f2937;
      border-radius: 8px;
      padding: 16px;
      margin-bottom: 16px;
      border-left: 4px solid #06b6d4;
    }
    .section.boot {
      border-left-color: #ef4444;
    }
    h2 {
      font-size: 15px;
      margin-top: 0;
      margin-bottom: 8px;
      color: #f3f4f6;
    }
    p {
      font-size: 12px;
      color: #9ca3af;
      margin: 0 0 12px 0;
    }
    .file-input {
      display: block;
      width: 100%;
      font-size: 13px;
      color: #9ca3af;
      margin-bottom: 12px;
    }
    .file-input::-webkit-file-upload-button {
      background: #374151;
      color: #fff;
      border: none;
      padding: 6px 12px;
      border-radius: 4px;
      cursor: pointer;
      margin-right: 8px;
    }
    .btn {
      background: #06b6d4;
      color: #111827;
      border: none;
      padding: 10px 16px;
      font-weight: bold;
      border-radius: 6px;
      cursor: pointer;
      font-size: 13px;
      width: 100%;
      transition: background 0.2s, transform 0.1s;
    }
    .btn:hover {
      background: #22d3ee;
    }
    .btn:active {
      transform: scale(0.98);
    }
    .btn-boot {
      background: #ef4444;
      color: #fff;
    }
    .btn-boot:hover {
      background: #f87171;
    }
    .btn-revert {
      background: #4b5563;
      color: #fff;
      margin-top: 8px;
    }
    .btn-revert:hover {
      background: #6b7280;
    }
    .save-btn {
      background: linear-gradient(135deg, #3b82f6, #2563eb);
      color: #fff;
      border: none;
      padding: 12px 20px;
      font-weight: bold;
      border-radius: 8px;
      cursor: pointer;
      font-size: 15px;
      width: 100%;
      transition: background 0.2s, transform 0.1s;
      box-shadow: 0 4px 6px -1px rgba(37, 99, 235, 0.2);
      margin-top: 16px;
    }
    .save-btn:hover {
      background: linear-gradient(135deg, #60a5fa, #2563eb);
    }
    .save-btn:active {
      transform: scale(0.98);
    }
    .save-btn:disabled {
      background: #4b5563;
      color: #9ca3af;
      cursor: not-allowed;
      box-shadow: none;
      transform: none;
    }
    .soc-slider {
      -webkit-appearance: none;
      appearance: none;
      width: 100%;
      height: 8px;
      border-radius: 4px;
      background: linear-gradient(to right, #ef4444 0%, #f97316 35%, #eab308 70%, #10b981 100%);
      outline: none;
      margin: 8px 0;
    }
    .soc-slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 22px;
      height: 22px;
      border-radius: 50%;
      background: #ffffff;
      border: 2px solid #10b981;
      cursor: pointer;
      box-shadow: 0 2px 4px rgba(0,0,0,0.3);
    }
    .soc-slider::-moz-range-thumb {
      width: 22px;
      height: 22px;
      border-radius: 50%;
      background: #ffffff;
      border: 2px solid #10b981;
      cursor: pointer;
      box-shadow: 0 2px 4px rgba(0,0,0,0.3);
    }
    .info {
      font-size: 11px;
      color: #6b7280;
      text-align: center;
      margin-top: 16px;
      line-height: 1.4;
    }
    
    /* Custom SweetAlert-Style Modals */
    .modal-overlay {
      position: fixed;
      inset: 0;
      width: 100%;
      height: 100vh;
      height: 100dvh;
      min-height: 100svh;
      background: rgba(11, 15, 25, 0.85);
      backdrop-filter: blur(8px);
      -webkit-backdrop-filter: blur(8px);
      display: flex;
      align-items: center;
      justify-content: center;
      z-index: 2000;
      overflow: hidden;
      overscroll-behavior: none;
      box-sizing: border-box;
      opacity: 0;
      visibility: hidden;
      transition: opacity 0.25s ease, visibility 0.25s ease;
    }
    .modal-overlay.active {
      opacity: 1;
      visibility: visible;
    }
    .modal-card {
      background: #111827;
      border: 1px solid #1f2937;
      border-radius: 16px;
      padding: 32px;
      max-width: 380px;
      width: 85%;
      max-height: calc(100vh - 40px);
      max-height: calc(100dvh - 40px);
      overflow-y: auto;
      overscroll-behavior: contain;
      -webkit-overflow-scrolling: touch;
      touch-action: pan-y;
      text-align: center;
      box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.6);
      transform: scale(0.8);
      transition: transform 0.25s cubic-bezier(0.34, 1.56, 0.64, 1);
      box-sizing: border-box;
    }
    .modal-overlay.active .modal-card {
      transform: scale(1);
    }
    .modal-icon {
      width: 64px;
      height: 64px;
      border-radius: 50%;
      display: flex;
      align-items: center;
      justify-content: center;
      margin: 0 auto 20px auto;
      font-size: 32px;
      font-weight: bold;
      box-sizing: border-box;
    }
    .modal-icon.success {
      background: rgba(16, 185, 129, 0.1);
      color: #10b981;
      border: 3px solid #10b981;
      animation: pulse-green 2s infinite;
    }
    .modal-icon.error {
      background: rgba(239, 68, 68, 0.1);
      color: #ef4444;
      border: 3px solid #ef4444;
      animation: pulse-red 2s infinite;
    }
    .modal-icon.loading {
      border: 4px solid #1f2937;
      border-top: 4px solid #06b6d4;
      border-radius: 50%;
      animation: spin 1s linear infinite;
    }
    .modal-title {
      font-size: 18px;
      font-weight: bold;
      margin-bottom: 8px;
      color: #f3f4f6;
    }
    .modal-desc {
      font-size: 13px;
      color: #9ca3af;
      margin-bottom: 24px;
      line-height: 1.5;
    }
    
    /* Circular loading animation spinner */
    @keyframes spin {
      0% { transform: rotate(0deg); }
      100% { transform: rotate(360deg); }
    }
    @keyframes pulse-green {
      0% { box-shadow: 0 0 0 0 rgba(16, 185, 129, 0.4); }
      70% { box-shadow: 0 0 0 10px rgba(16, 185, 129, 0); }
      100% { box-shadow: 0 0 0 0 rgba(16, 185, 129, 0); }
    }
    @keyframes pulse-red {
      0% { box-shadow: 0 0 0 0 rgba(239, 68, 68, 0.4); }
      70% { box-shadow: 0 0 0 10px rgba(239, 68, 68, 0); }
      100% { box-shadow: 0 0 0 0 rgba(239, 68, 68, 0); }
    }
    
    /* HTML5 upload progress bar */
    .progress-container {
      width: 100%;
      height: 6px;
      background: #1f2937;
      border-radius: 3px;
      overflow: hidden;
      margin: 16px 0;
      display: none;
    }
    .progress-bar {
      width: 0%;
      height: 100%;
      background: linear-gradient(90deg, #06b6d4, #3b82f6);
      transition: width 0.1s ease-out;
    }
    /* Auto-resize badge */
    .badge {
      display: inline-block;
      background: linear-gradient(135deg, #06b6d4, #3b82f6);
      color: #fff;
      font-size: 10px;
      font-weight: bold;
      padding: 2px 8px;
      border-radius: 10px;
      vertical-align: middle;
      margin-left: 4px;
      letter-spacing: 0.5px;
    }
    /* GIF Preview Box */
    .preview-box {
      background: #111827;
      border: 1px dashed #374151;
      border-radius: 8px;
      padding: 10px;
      margin-bottom: 12px;
      text-align: center;
    }
    .preview-box img {
      max-width: 320px;
      max-height: 172px;
      border-radius: 4px;
      border: 1px solid #374151;
      image-rendering: pixelated;
    }
    .preview-info {
      font-size: 11px;
      color: #9ca3af;
      margin-top: 6px;
      line-height: 1.4;
    }
    .preview-info .dim {
      color: #06b6d4;
      font-weight: bold;
    }
    .preview-info .arrow {
      color: #10b981;
    }
    .preview-info .size-warn {
      color: #f59e0b;
    }
    .preview-info .size-ok {
      color: #10b981;
    }
    
    /* Custom Styling for Grid and Select controls */
    .select-control {
      width: 100%;
      background: #1f2937;
      color: #fff;
      border: 1px solid #374151;
      padding: 10px;
      border-radius: 6px;
      font-size: 13px;
      box-sizing: border-box;
      outline: none;
      transition: border-color 0.2s;
      margin-bottom: 8px;
    }
    .select-control:focus {
      border-color: #06b6d4;
    }
    .slots-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
      margin-bottom: 16px;
    }
    @media (max-width: 480px) {
      .slots-grid {
        grid-template-columns: 1fr;
      }
    }
    .slot-card {
      background: #1f2937;
      border-radius: 8px;
      padding: 12px;
      display: flex;
      flex-direction: column;
      border-left: 4px solid #3b82f6;
      box-sizing: border-box;
    }
    .slot-num {
      font-size: 11px;
      font-weight: bold;
      color: #3b82f6;
      margin-bottom: 6px;
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }
    
    @keyframes fadeIn {
      from { opacity: 0; transform: translateY(10px); }
      to { opacity: 1; transform: translateY(0); }
    }
    .fade-in {
      animation: fadeIn 0.4s ease-out forwards;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>OBD2 Dashboard Setup</h1>
    <p style="text-align: center; font-size: 13px; margin-bottom: 20px;">อัปโหลดไฟล์ GIF ไร้สายผ่าน WiFi</p>
    
    <div id="js-error-banner" style="display: none; background: rgba(239, 68, 68, 0.2); border: 2px solid #ef4444; border-radius: 8px; padding: 16px; margin-bottom: 20px; font-size: 12px; color: #f87171; line-height: 1.5; text-align: center; font-weight: bold;">
      ⚠️ หน้าเว็บโหลดไม่สมบูรณ์เนื่องจากข้อผิดพลาดของสคริปต์ (JavaScript Error) หรือการเชื่อมต่อขาดหาย แต่คุณยังคงสามารถเปลี่ยนไฟล์รูปภาพ GIF ด้านล่างนี้ได้
    </div>
    
    <div style="background: rgba(239, 68, 68, 0.15); border: 1px solid rgba(239, 68, 68, 0.4); border-radius: 8px; padding: 10px; margin-bottom: 12px; font-size: 11px; color: #f87171; line-height: 1.45;">
      ⚠️ <strong>สำหรับผู้ใช้ iPhone:</strong> หากเลือกรูปแล้วโหลดค้าง ให้เปิดรูปนั้นให้เต็มจอในแอป Photos ก่อน (ขณะใช้เน็ตมือถือ) เพื่อดึงรูปจาก iCloud ลงเครื่อง แล้วค่อยต่อ WiFi บอร์ดเพื่ออัปโหลด หรือใช้วิธีอัปโหลดผ่านแอป <strong>"ไฟล์" (Files)</strong> แทน<br>
      💡 <strong>ถ้าอัปโหลดไม่ผ่าน:</strong> ให้เข้าเว็บ <strong>192.168.4.1</strong> โดยตรงบนเบราว์เซอร์
    </div>

    <div style="font-size: 11px; color: #9ca3af; line-height: 1.45; background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.05); padding: 10px; border-radius: 8px; margin-bottom: 20px;">
      <strong>💡 คำแนะนำสำหรับรูปภาพและ GIF:</strong><br>
      • <strong>ขนาดไฟล์:</strong> ไม่เกิน 200 KB (หากเกิน หน้าจอจะเล่นภาพเริ่มต้นแทน เพื่อความปลอดภัยของระบบ)<br>
      • <strong>ขนาดพิกเซล:</strong> แนะนำที่ 320 x 172 พิกเซล (ระบบมีย่อส่วนภาพให้อัตโนมัติ)<br>
      • <strong>จำนวนเฟรม:</strong> แนะนำไม่เกิน 60 เฟรม (ยาวประมาณ 5 วินาที) เพื่อเฟรมเรตที่สมูทที่สุด<br>
      • <strong>⚙️ หมายเหตุ:</strong> หากไฟล์ต้นฉบับใหญ่หรือยาวมากเกินไป ระบบอาจย่อลงมาต่ำกว่า 200 KB ไม่สำเร็จ แนะนำให้ตัดช่วงเฟรมหรือย่อขนาดไฟล์เบื้องต้นก่อนนำมาอัปโหลดครับ
    </div>
    <div class="section boot">
      <h2>1. ภาพเปิดเครื่อง (Boot Animation)</h2>
      <p>ไฟล์ GIF สำหรับเล่นตอนเปิดหน้าจอ <span class="badge">🔄 ปรับขนาดอัตโนมัติ</span></p>

      <form onsubmit="event.preventDefault(); handleFormSubmit(this, 'boot')">
        <input type="file" name="file" accept="image/gif,.gif" class="file-input" required onchange="previewGIF(this, 'preview-boot')">
        <div id="preview-boot" class="preview-box" style="display:none;"></div>
        <button type="submit" class="btn btn-boot">อัปโหลดภาพเปิดเครื่อง</button>
      </form>
      <button type="button" class="btn btn-revert" onclick="restoreDefaultGIF('boot')" style="margin-top: 10px; margin-bottom: 4px; display: block; width: 100%;">🔄 คืนค่าภาพเปิดเครื่องเริ่มต้น (Restore Default Boot GIF)</button>
    </div>
    
    <div class="section">
      <h2>2. ภาพลูปหน้าหลัก (Theme 2 Loop)</h2>
      <p>ไฟล์ GIF สำหรับเล่นลูปบนหน้าหลักของธีม 2 <span class="badge">🔄 ปรับขนาดอัตโนมัติ</span></p>

      <form onsubmit="event.preventDefault(); handleFormSubmit(this, 'loop')">
        <input type="file" name="file" accept="image/gif,.gif" class="file-input" required onchange="previewGIF(this, 'preview-loop')">
        <div id="preview-loop" class="preview-box" style="display:none;"></div>
        <button type="submit" class="btn">อัปโหลดภาพลูปธีม 2</button>
      </form>
      <button type="button" class="btn btn-revert" onclick="restoreDefaultGIF('loop')" style="margin-top: 10px; margin-bottom: 4px; display: block; width: 100%;">🔄 คืนค่ารูปภาพลูปเริ่มต้น (Restore Default Loop GIF)</button>
    </div>
    
    <button type="button" class="btn btn-revert" onclick="restoreDefaultGIF('all')" style="margin-bottom: 16px; display: block; width: 100%;">🔄 คืนค่ารูปภาพเริ่มต้นทั้งหมด (Restore All Default GIFs)</button>

    <div id="layout-section" style="display: none;">
      <form action="/save_config" method="POST" id="main-config-form" onsubmit="event.preventDefault();">
        <!-- Hidden inputs for NVS config persistence (populated from AJAX) -->
        <input type="hidden" name="selected_vehicle_profile" id="car-profile-id" value="BYD_DOLPHIN_TH_EV">
        <input type="hidden" name="energy_source" id="energy-source-mode" value="0">
        <input type="hidden" name="elec_rate" id="elec-rate" value="4.50">
        <input type="hidden" name="car_eng_val" id="car-eng-val" value="0.00">
        <input type="hidden" name="car_eng_unit" id="car-eng-unit" value="kWh">
        <input type="hidden" name="car_eng_lbl" id="car-eng-lbl" value="">

        <!-- EV Settings Wrapper -->
        <div id="ev-settings-wrapper">


            <!-- 3.3 Screen Layout -->
            <h3 style="color: #3b82f6; margin-top: 16px; margin-bottom: 8px; font-size: 14px;">📺 การจัดเรียงหน้าจอแสดงผล (Screen Layout)</h3>
            <p style="font-size: 11px; color: #9ca3af; margin-top: 0; margin-bottom: 12px;">จัดลำดับและเลือกฟังก์ชันที่จะโชว์ในหน้าเกจทั้ง 6 หน้าจอหลัก</p>
            <div class="slots-grid" style="margin-bottom: 16px;">
              <div class="slot-card">
                <div class="slot-num">หน้าจอที่ 1</div>
                <select class="select-control" name="slot0">
                </select>
              </div>
              <div class="slot-card" style="border-left-color: #10b981;">
                <div class="slot-num" style="color: #10b981;">หน้าจอที่ 2</div>
                <select class="select-control" name="slot1">
                </select>
              </div>
              <div class="slot-card" style="border-left-color: #f59e0b;">
                <div class="slot-num" style="color: #f59e0b;">หน้าจอที่ 3</div>
                <select class="select-control" name="slot2">
                </select>
              </div>
              <div class="slot-card" style="border-left-color: #a855f7;">
                <div class="slot-num" style="color: #a855f7;">หน้าจอที่ 4</div>
                <select class="select-control" name="slot3">
                </select>
              </div>
              <div class="slot-card" style="border-left-color: #ec4899;">
                <div class="slot-num" style="color: #ec4899;">หน้าจอที่ 5</div>
                <select class="select-control" name="slot4">
                </select>
              </div>
              <div class="slot-card" style="border-left-color: #06b6d4;">
                <div class="slot-num" style="color: #06b6d4;">หน้าจอที่ 6</div>
                <select class="select-control" name="slot5">
                </select>
              </div>
            </div>


          </div>

          <!-- Section 3 Save Button -->
          <button type="submit" id="save-btn-sec3" class="save-btn">💾 บันทึกการตั้งค่า</button>
        </div>

        <!-- Section 4: AutoBack Settings -->
        <div id="section-4-container" class="section" style="border-left-color: #3b82f6; margin-top: 16px;">
          <h2>4. AutoBack Settings</h2>
          <p>ตั้งค่าการสลับหน้าจออัตโนมัติเมื่อหยุดรถ</p>

          <!-- 4.2 AutoBack Settings Section -->
          <div id="autoback-settings-section" style="margin-bottom: 16px; padding: 12px; border: 1px solid #3b82f6; border-radius: 8px; background-color: rgba(59, 130, 246, 0.05);">
            <h3 style="color: #3b82f6; margin-top: 0; margin-bottom: 8px; font-size: 14px;">🔄 ตั้งค่าสลับหน้าจออัตโนมัติ (AutoBack Settings)</h3>
            <p style="font-size: 11px; color: #9ca3af; margin-top: 0; margin-bottom: 12px;">สลับหน้าจออัตโนมัติเมื่อรถจอดหยุดนิ่งเป็นเวลา 3 วินาที</p>
            
            <div style="margin-bottom: 12px;">
              <label style="font-size: 10px; color: #3b82f6; display: block; margin-bottom: 4px; text-transform: uppercase;">สถานะ AutoBack (AutoBack Status)</label>
              <select class="select-control" name="autoback_enabled" id="autoback-enabled" style="border-color: #3b82f6;" onchange="toggleAutoBackType()">
                <option value="1">เปิดใช้งาน (Enabled)</option>
                <option value="0">ปิดใช้งาน (Disabled)</option>
              </select>
            </div>
            
            <div style="margin-bottom: 12px;">
              <label style="font-size: 10px; color: #3b82f6; display: block; margin-bottom: 4px; text-transform: uppercase;">หน้าจอเมื่อหยุดรถ (Stop Screen Type)</label>
              <select class="select-control" name="autoback_type" id="autoback-type" style="border-color: #3b82f6;" onchange="toggleAutoBackType()">
                <option value="0">GIF Loop</option>
                <option value="1">Robo Eye</option>
                <option value="2">Stopwatch (นาฬิกาจับเวลา)</option>
                <option value="3">Charge Estimate (ประมาณเวลาชาร์จ)</option>
              </select>
            </div>

            <!-- Dynamic Target SOC Setting for Charge Estimate -->
            <div id="target-soc-container" style="margin-bottom: 12px; display: none;">
              <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 4px;">
                <label style="font-size: 10px; color: #10b981; text-transform: uppercase;">เป้าหมายปริมาณแบตเตอรี่ (Target SOC)</label>
                <span id="target-soc-val" style="font-size: 14px; font-weight: bold; color: #10b981;">80%</span>
              </div>
              <input type="range" class="soc-slider" name="charge_target_soc" id="charge-target-soc" min="0" max="100" step="10" value="80" oninput="updateTargetSocDisplay(this.value);">
              <div style="display: flex; justify-content: space-between; font-size: 9px; color: #9ca3af; margin-top: 2px;">
                <span>0%</span><span>50%</span><span>100%</span>
              </div>
              <p style="font-size: 11px; color: #10b981; margin-top: 6px; margin-bottom: 0;">Charge Estimate จะแสดงอัตโนมัติเมื่อระบบตรวจพบว่ารถกำลังชาร์จ</p>
            </div>
          </div>



          <!-- Section 4 Save Button -->
          <button type="submit" id="save-btn-sec4" class="save-btn">💾 บันทึกการตั้งค่า</button>
        </div>
      </form>
    </div>

    <div class="info">
      IP: 192.168.4.1 | SSID: OBD2-Dashboard | Pass: 12345678<br>
      Free Heap: <span id="lbl-free-heap">--</span> KB | FS Used: <span id="lbl-fs-used">--</span> / <span id="lbl-fs-total">--</span> KB
    </div>
  </div>

  <!-- SweetAlert Clone Modal DOM -->
  <div id="modal-overlay" class="modal-overlay">
    <div class="modal-card">
      <div id="modal-icon" class="modal-icon"></div>
      <div id="modal-title" class="modal-title"></div>
      <div id="modal-desc" class="modal-desc"></div>
      <div id="progress-container" class="progress-container">
        <div id="progress-bar" class="progress-bar"></div>
      </div>
      <button id="modal-btn" class="btn" style="display: none;"></button>
    </div>
  </div>
  
  <script>
    // Embedded GIF Libraries (gifenc + gifuct-js)
    var gifenc=(function(){var exports={};var __defProp=Object.defineProperty;var __markAsModule=(target)=>__defProp(target,"__esModule",{value:true});var __export=(target,all)=>{for(var name in all)__defProp(target,name,{get:all[name],enumerable:true});};__markAsModule(exports);__export(exports,{GIFEncoder:()=>GIFEncoder,applyPalette:()=>applyPalette,default:()=>src_default,nearestColor:()=>nearestColor,nearestColorIndex:()=>nearestColorIndex,nearestColorIndexWithDistance:()=>nearestColorIndexWithDistance,prequantize:()=>prequantize,quantize:()=>quantize,snapColorsToPalette:()=>snapColorsToPalette});var constants_default={signature:"GIF",version:"89a",trailer:59,extensionIntroducer:33,applicationExtensionLabel:255,graphicControlExtensionLabel:249,imageSeparator:44,signatureSize:3,versionSize:3,globalColorTableFlagMask:128,colorResolutionMask:112,sortFlagMask:8,globalColorTableSizeMask:7,applicationIdentifierSize:8,applicationAuthCodeSize:3,disposalMethodMask:28,userInputFlagMask:2,transparentColorFlagMask:1,localColorTableFlagMask:128,interlaceFlagMask:64,idSortFlagMask:32,localColorTableSizeMask:7};function createStream(initialCapacity=256){let cursor=0;let contents=new Uint8Array(initialCapacity);return{get buffer(){return contents.buffer;},reset(){cursor=0;},bytesView(){return contents.subarray(0,cursor);},bytes(){return contents.slice(0,cursor);},writeByte(byte){expand(cursor+1);contents[cursor]=byte;cursor++;},writeBytes(data,offset=0,byteLength=data.length){expand(cursor+byteLength);for(let i=0;i<byteLength;i++){contents[cursor++]=data[i+offset];}},writeBytesView(data,offset=0,byteLength=data.byteLength){expand(cursor+byteLength);contents.set(data.subarray(offset,offset+byteLength),cursor);cursor+=byteLength;}};function expand(newCapacity){var prevCapacity=contents.length;if(prevCapacity>=newCapacity)return;var CAPACITY_DOUBLING_MAX=1024*1024;newCapacity=Math.max(newCapacity,prevCapacity*(prevCapacity<CAPACITY_DOUBLING_MAX ? 2:1.125)>>>0);if(prevCapacity !=0)newCapacity=Math.max(newCapacity,256);const oldContents=contents;contents=new Uint8Array(newCapacity);if(cursor>0)contents.set(oldContents.subarray(0,cursor),0);}}var BITS=12;var DEFAULT_HSIZE=5003;var MASKS=[0,1,3,7,15,31,63,127,255,511,1023,2047,4095,8191,16383,32767,65535];function lzwEncode(width,height,pixels,colorDepth,outStream=createStream(512),accum=new Uint8Array(256),htab=new Int32Array(DEFAULT_HSIZE),codetab=new Int32Array(DEFAULT_HSIZE)){const hsize=htab.length;const initCodeSize=Math.max(2,colorDepth);accum.fill(0);codetab.fill(0);htab.fill(-1);let cur_accum=0;let cur_bits=0;const init_bits=initCodeSize+1;const g_init_bits=init_bits;let clear_flg=false;let n_bits=g_init_bits;let maxcode=(1<<n_bits)-1;const ClearCode=1<<init_bits-1;const EOFCode=ClearCode+1;let free_ent=ClearCode+2;let a_count=0;let ent=pixels[0];let hshift=0;for(let fcode=hsize;fcode<65536;fcode*=2){++hshift;}hshift=8-hshift;outStream.writeByte(initCodeSize);output(ClearCode);const length=pixels.length;for(let idx=1;idx<length;idx++){next_block:{const c=pixels[idx];const fcode=(c<<BITS)+ent;let i=c<<hshift ^ ent;if(htab[i]===fcode){ent=codetab[i];break next_block;}const disp=i===0 ? 1:hsize-i;while(htab[i]>=0){i-=disp;if(i<0)i+=hsize;if(htab[i]===fcode){ent=codetab[i];break next_block;}}output(ent);ent=c;if(free_ent<1<<BITS){codetab[i]=free_ent++;htab[i]=fcode;}else{htab.fill(-1);free_ent=ClearCode+2;clear_flg=true;output(ClearCode);}}}output(ent);output(EOFCode);outStream.writeByte(0);return outStream.bytesView();function output(code){cur_accum &=MASKS[cur_bits];if(cur_bits>0)cur_accum |=code<<cur_bits;else cur_accum=code;cur_bits+=n_bits;while(cur_bits>=8){accum[a_count++]=cur_accum & 255;if(a_count>=254){outStream.writeByte(a_count);outStream.writeBytesView(accum,0,a_count);a_count=0;}cur_accum>>=8;cur_bits-=8;}if(free_ent>maxcode || clear_flg){if(clear_flg){n_bits=g_init_bits;maxcode=(1<<n_bits)-1;clear_flg=false;}else{++n_bits;maxcode=n_bits===BITS ? 1<<n_bits:(1<<n_bits)-1;}}if(code==EOFCode){while(cur_bits>0){accum[a_count++]=cur_accum & 255;if(a_count>=254){outStream.writeByte(a_count);outStream.writeBytesView(accum,0,a_count);a_count=0;}cur_accum>>=8;cur_bits-=8;}if(a_count>0){outStream.writeByte(a_count);outStream.writeBytesView(accum,0,a_count);a_count=0;}}}}var lzwEncode_default=lzwEncode;function rgb888_to_rgb565(r,g,b){return r<<8 & 63488 | g<<2 & 992 | b>>3;}function rgba8888_to_rgba4444(r,g,b,a){return r>>4 | g & 240 |(b & 240)<<4 |(a & 240)<<8;}function rgb888_to_rgb444(r,g,b){return r>>4<<8 | g & 240 | b>>4;}function clamp(value,min,max){return value<min ? min:value>max ? max:value;}function sqr(value){return value*value;}function find_nn(bins,idx,hasAlpha){var nn=0;var err=1e100;const bin1=bins[idx];const n1=bin1.cnt;const wa=bin1.ac;const wr=bin1.rc;const wg=bin1.gc;const wb=bin1.bc;for(var i=bin1.fw;i !=0;i=bins[i].fw){const bin=bins[i];const n2=bin.cnt;const nerr2=n1*n2/(n1+n2);if(nerr2>=err)continue;var nerr=0;if(hasAlpha){nerr+=nerr2*sqr(bin.ac-wa);if(nerr>=err)continue;}nerr+=nerr2*sqr(bin.rc-wr);if(nerr>=err)continue;nerr+=nerr2*sqr(bin.gc-wg);if(nerr>=err)continue;nerr+=nerr2*sqr(bin.bc-wb);if(nerr>=err)continue;err=nerr;nn=i;}bin1.err=err;bin1.nn=nn;}function create_bin(){return{ac:0,rc:0,gc:0,bc:0,cnt:0,nn:0,fw:0,bk:0,tm:0,mtm:0,err:0};}function create_bin_list(data,format){const bincount=format==="rgb444" ? 4096:65536;const bins=new Array(bincount);const size=data.length;if(format==="rgba4444"){for(let i=0;i<size;++i){const color=data[i];const a=color>>24 & 255;const b=color>>16 & 255;const g=color>>8 & 255;const r=color & 255;const index=rgba8888_to_rgba4444(r,g,b,a);let bin=index in bins ? bins[index]:bins[index]=create_bin();bin.rc+=r;bin.gc+=g;bin.bc+=b;bin.ac+=a;bin.cnt++;}}else if(format==="rgb444"){for(let i=0;i<size;++i){const color=data[i];const b=color>>16 & 255;const g=color>>8 & 255;const r=color & 255;const index=rgb888_to_rgb444(r,g,b);let bin=index in bins ? bins[index]:bins[index]=create_bin();bin.rc+=r;bin.gc+=g;bin.bc+=b;bin.cnt++;}}else{for(let i=0;i<size;++i){const color=data[i];const b=color>>16 & 255;const g=color>>8 & 255;const r=color & 255;const index=rgb888_to_rgb565(r,g,b);let bin=index in bins ? bins[index]:bins[index]=create_bin();bin.rc+=r;bin.gc+=g;bin.bc+=b;bin.cnt++;}}return bins;}function quantize(rgba,maxColors,opts={}){const{format="rgb565",clearAlpha=true,clearAlphaColor=0,clearAlphaThreshold=0,oneBitAlpha=false}=opts;if(!rgba || !rgba.buffer){throw new Error("quantize()expected RGBA Uint8Array data");}if(!(rgba instanceof Uint8Array)&& !(rgba instanceof Uint8ClampedArray)){throw new Error("quantize()expected RGBA Uint8Array data");}const data=new Uint32Array(rgba.buffer);let useSqrt=opts.useSqrt !==false;const hasAlpha=format==="rgba4444";const bins=create_bin_list(data,format);const bincount=bins.length;const bincountMinusOne=bincount-1;const heap=new Uint32Array(bincount+1);var maxbins=0;for(var i=0;i<bincount;++i){const bin=bins[i];if(bin !=null){var d=1/bin.cnt;if(hasAlpha)bin.ac*=d;bin.rc*=d;bin.gc*=d;bin.bc*=d;bins[maxbins++]=bin;}}if(sqr(maxColors)/maxbins<0.022){useSqrt=false;}var i=0;for(;i<maxbins-1;++i){bins[i].fw=i+1;bins[i+1].bk=i;if(useSqrt)bins[i].cnt=Math.sqrt(bins[i].cnt);}if(useSqrt)bins[i].cnt=Math.sqrt(bins[i].cnt);var h,l,l2;for(i=0;i<maxbins;++i){find_nn(bins,i,false);var err=bins[i].err;for(l=++heap[0];l>1;l=l2){l2=l>>1;if(bins[h=heap[l2]].err<=err)break;heap[l]=h;}heap[l]=i;}var extbins=maxbins-maxColors;for(i=0;i<extbins;){var tb;for(;;){var b1=heap[1];tb=bins[b1];if(tb.tm>=tb.mtm && bins[tb.nn].mtm<=tb.tm)break;if(tb.mtm==bincountMinusOne)b1=heap[1]=heap[heap[0]--];else{find_nn(bins,b1,false);tb.tm=i;}var err=bins[b1].err;for(l=1;(l2=l+l)<=heap[0];l=l2){if(l2<heap[0]&& bins[heap[l2]].err>bins[heap[l2+1]].err)l2++;if(err<=bins[h=heap[l2]].err)break;heap[l]=h;}heap[l]=b1;}var nb=bins[tb.nn];var n1=tb.cnt;var n2=nb.cnt;var d=1/(n1+n2);if(hasAlpha)tb.ac=d*(n1*tb.ac+n2*nb.ac);tb.rc=d*(n1*tb.rc+n2*nb.rc);tb.gc=d*(n1*tb.gc+n2*nb.gc);tb.bc=d*(n1*tb.bc+n2*nb.bc);tb.cnt+=nb.cnt;tb.mtm=++i;bins[nb.bk].fw=nb.fw;bins[nb.fw].bk=nb.bk;nb.mtm=bincountMinusOne;}let palette=[];var k=0;for(i=0;;++k){let r=clamp(Math.round(bins[i].rc),0,255);let g=clamp(Math.round(bins[i].gc),0,255);let b=clamp(Math.round(bins[i].bc),0,255);let a=255;if(hasAlpha){a=clamp(Math.round(bins[i].ac),0,255);if(oneBitAlpha){const threshold=typeof oneBitAlpha==="number" ? oneBitAlpha:127;a=a<=threshold ? 0:255;}if(clearAlpha && a<=clearAlphaThreshold){r=g=b=clearAlphaColor;a=0;}}const color=hasAlpha ?[r,g,b,a]:[r,g,b];const exists=existsInPalette(palette,color);if(!exists)palette.push(color);if((i=bins[i].fw)==0)break;}return palette;}function existsInPalette(palette,color){for(let i=0;i<palette.length;i++){const p=palette[i];let matchesRGB=p[0]===color[0]&& p[1]===color[1]&& p[2]===color[2];let matchesAlpha=p.length>=4 && color.length>=4 ? p[3]===color[3]:true;if(matchesRGB && matchesAlpha)return true;}return false;}function euclideanDistanceSquared(a,b){var sum=0;var n;for(n=0;n<a.length;n++){const dx=a[n]-b[n];sum+=dx*dx;}return sum;}function roundStep(byte,step){return step>1 ? Math.round(byte/step)*step:byte;}function prequantize(rgba,{roundRGB=5,roundAlpha=10,oneBitAlpha=null}={}){const data=new Uint32Array(rgba.buffer);for(let i=0;i<data.length;i++){const color=data[i];let a=color>>24 & 255;let b=color>>16 & 255;let g=color>>8 & 255;let r=color & 255;a=roundStep(a,roundAlpha);if(oneBitAlpha){const threshold=typeof oneBitAlpha==="number" ? oneBitAlpha:127;a=a<=threshold ? 0:255;}r=roundStep(r,roundRGB);g=roundStep(g,roundRGB);b=roundStep(b,roundRGB);data[i]=a<<24 | b<<16 | g<<8 | r<<0;}}function applyPalette(rgba,palette,format="rgb565"){if(!rgba || !rgba.buffer){throw new Error("quantize()expected RGBA Uint8Array data");}if(!(rgba instanceof Uint8Array)&& !(rgba instanceof Uint8ClampedArray)){throw new Error("quantize()expected RGBA Uint8Array data");}if(palette.length>256){throw new Error("applyPalette()only works with 256 colors or less");}const data=new Uint32Array(rgba.buffer);const length=data.length;const bincount=format==="rgb444" ? 4096:65536;const index=new Uint8Array(length);const cache=new Array(bincount);const hasAlpha=format==="rgba4444";if(format==="rgba4444"){for(let i=0;i<length;i++){const color=data[i];const a=color>>24 & 255;const b=color>>16 & 255;const g=color>>8 & 255;const r=color & 255;const key=rgba8888_to_rgba4444(r,g,b,a);const idx=key in cache ? cache[key]:cache[key]=nearestColorIndexRGBA(r,g,b,a,palette);index[i]=idx;}}else{const rgb888_to_key=format==="rgb444" ? rgb888_to_rgb444:rgb888_to_rgb565;for(let i=0;i<length;i++){const color=data[i];const b=color>>16 & 255;const g=color>>8 & 255;const r=color & 255;const key=rgb888_to_key(r,g,b);const idx=key in cache ? cache[key]:cache[key]=nearestColorIndexRGB(r,g,b,palette);index[i]=idx;}}return index;}function nearestColorIndexRGBA(r,g,b,a,palette){let k=0;let mindist=1e100;for(let i=0;i<palette.length;i++){const px2=palette[i];const a2=px2[3];let curdist=sqr2(a2-a);if(curdist>mindist)continue;const r2=px2[0];curdist+=sqr2(r2-r);if(curdist>mindist)continue;const g2=px2[1];curdist+=sqr2(g2-g);if(curdist>mindist)continue;const b2=px2[2];curdist+=sqr2(b2-b);if(curdist>mindist)continue;mindist=curdist;k=i;}return k;}function nearestColorIndexRGB(r,g,b,palette){let k=0;let mindist=1e100;for(let i=0;i<palette.length;i++){const px2=palette[i];const r2=px2[0];let curdist=sqr2(r2-r);if(curdist>mindist)continue;const g2=px2[1];curdist+=sqr2(g2-g);if(curdist>mindist)continue;const b2=px2[2];curdist+=sqr2(b2-b);if(curdist>mindist)continue;mindist=curdist;k=i;}return k;}function snapColorsToPalette(palette,knownColors,threshold=5){if(!palette.length || !knownColors.length)return;const paletteRGB=palette.map((p)=>p.slice(0,3));const thresholdSq=threshold*threshold;const dim=palette[0].length;for(let i=0;i<knownColors.length;i++){let color=knownColors[i];if(color.length<dim){color=[color[0],color[1],color[2],255];}else if(color.length>dim){color=color.slice(0,3);}else{color=color.slice();}const r=nearestColorIndexWithDistance(paletteRGB,color.slice(0,3),euclideanDistanceSquared);const idx=r[0];const distanceSq=r[1];if(distanceSq>0 && distanceSq<=thresholdSq){palette[idx]=color;}}}function sqr2(a){return a*a;}function nearestColorIndex(colors,pixel,distanceFn=euclideanDistanceSquared){let minDist=Infinity;let minDistIndex=-1;for(let j=0;j<colors.length;j++){const paletteColor=colors[j];const dist=distanceFn(pixel,paletteColor);if(dist<minDist){minDist=dist;minDistIndex=j;}}return minDistIndex;}function nearestColorIndexWithDistance(colors,pixel,distanceFn=euclideanDistanceSquared){let minDist=Infinity;let minDistIndex=-1;for(let j=0;j<colors.length;j++){const paletteColor=colors[j];const dist=distanceFn(pixel,paletteColor);if(dist<minDist){minDist=dist;minDistIndex=j;}}return[minDistIndex,minDist];}function nearestColor(colors,pixel,distanceFn=euclideanDistanceSquared){return colors[nearestColorIndex(colors,pixel,distanceFn)];}function GIFEncoder(opt={}){const{initialCapacity=4096,auto=true}=opt;const stream=createStream(initialCapacity);const HSIZE=5003;const accum=new Uint8Array(256);const htab=new Int32Array(HSIZE);const codetab=new Int32Array(HSIZE);let hasInit=false;return{reset(){stream.reset();hasInit=false;},finish(){stream.writeByte(constants_default.trailer);},bytes(){return stream.bytes();},bytesView(){return stream.bytesView();},get buffer(){return stream.buffer;},get stream(){return stream;},writeHeader,writeFrame(index,width,height,opts={}){const{transparent=false,transparentIndex=0,delay=0,palette=null,repeat=0,colorDepth=8,dispose=-1}=opts;let first=false;if(auto){if(!hasInit){first=true;writeHeader();hasInit=true;}}else{first=Boolean(opts.first);}width=Math.max(0,Math.floor(width));height=Math.max(0,Math.floor(height));if(first){if(!palette){throw new Error("First frame must include a{palette}option");}encodeLogicalScreenDescriptor(stream,width,height,palette,colorDepth);encodeColorTable(stream,palette);if(repeat>=0){encodeNetscapeExt(stream,repeat);}}const delayTime=Math.round(delay/10);encodeGraphicControlExt(stream,dispose,delayTime,transparent,transparentIndex);const useLocalColorTable=Boolean(palette)&& !first;encodeImageDescriptor(stream,width,height,useLocalColorTable ? palette:null);if(useLocalColorTable)encodeColorTable(stream,palette);encodePixels(stream,index,width,height,colorDepth,accum,htab,codetab);}};function writeHeader(){writeUTFBytes(stream,"GIF89a");}}function encodeGraphicControlExt(stream,dispose,delay,transparent,transparentIndex){stream.writeByte(33);stream.writeByte(249);stream.writeByte(4);if(transparentIndex<0){transparentIndex=0;transparent=false;}var transp,disp;if(!transparent){transp=0;disp=0;}else{transp=1;disp=2;}if(dispose>=0){disp=dispose & 7;}disp<<=2;const userInput=0;stream.writeByte(0 | disp | userInput | transp);writeUInt16(stream,delay);stream.writeByte(transparentIndex || 0);stream.writeByte(0);}function encodeLogicalScreenDescriptor(stream,width,height,palette,colorDepth=8){const globalColorTableFlag=1;const sortFlag=0;const globalColorTableSize=colorTableSize(palette.length)-1;const fields=globalColorTableFlag<<7 | colorDepth-1<<4 | sortFlag<<3 | globalColorTableSize;const backgroundColorIndex=0;const pixelAspectRatio=0;writeUInt16(stream,width);writeUInt16(stream,height);stream.writeBytes([fields,backgroundColorIndex,pixelAspectRatio]);}function encodeNetscapeExt(stream,repeat){stream.writeByte(33);stream.writeByte(255);stream.writeByte(11);writeUTFBytes(stream,"NETSCAPE2.0");stream.writeByte(3);stream.writeByte(1);writeUInt16(stream,repeat);stream.writeByte(0);}function encodeColorTable(stream,palette){const colorTableLength=1<<colorTableSize(palette.length);for(let i=0;i<colorTableLength;i++){let color=[0,0,0];if(i<palette.length){color=palette[i];}stream.writeByte(color[0]);stream.writeByte(color[1]);stream.writeByte(color[2]);}}function encodeImageDescriptor(stream,width,height,localPalette){stream.writeByte(44);writeUInt16(stream,0);writeUInt16(stream,0);writeUInt16(stream,width);writeUInt16(stream,height);if(localPalette){const interlace=0;const sorted=0;const palSize=colorTableSize(localPalette.length)-1;stream.writeByte(128 | interlace | sorted | 0 | palSize);}else{stream.writeByte(0);}}function encodePixels(stream,index,width,height,colorDepth=8,accum,htab,codetab){lzwEncode_default(width,height,index,colorDepth,stream,accum,htab,codetab);}function writeUInt16(stream,short){stream.writeByte(short & 255);stream.writeByte(short>>8 & 255);}function writeUTFBytes(stream,text){for(var i=0;i<text.length;i++){stream.writeByte(text.charCodeAt(i));}}function colorTableSize(length){return Math.max(Math.ceil(Math.log2(length)),1);}var src_default=GIFEncoder;;return exports;})();
    window.gifenc = gifenc;
    window.__gifencLoaded = true;
    console.log("[Upload] gifenc library block executed");
  </script>
  <script>
    !function e(r,t,a){function n(i,o){if(!t[i]){if(!r[i]){var p="function"==typeof require&&require;if(!o&&p)return p(i,!0);if(s)return s(i,!0);var l=new Error("Cannot find module '"+i+"'");throw l.code="MODULE_NOT_FOUND",l}var u=t[i]={exports:{}};r[i][0].call(u.exports,function(e){var t=r[i][1][e];return n(t?t:e)},u,u.exports,e,r,t,a)}return t[i].exports}for(var s="function"==typeof require&&require,i=0;i<a.length;i++)n(a[i]);return n}({1:[function(e,r,t){function a(e){this.data=e,this.pos=0}a.prototype.readByte=function(){return this.pos<this.data.length?this.data[this.pos++]:0},a.prototype.peekByte=function(){return this.pos<this.data.length?this.data[this.pos]:0},a.prototype.readBytes=function(e){for(var r=new Array(e),t=0;e>t;t++)r[t]=this.readByte();return r},a.prototype.peekBytes=function(e){for(var r=new Array(e),t=0;e>t;t++)r[t]=this.pos+t<this.data.length?this.data[this.pos+t]:0;return r},a.prototype.readString=function(e){for(var r="",t=0;e>t;t++)r+=String.fromCharCode(this.readByte());return r},a.prototype.readBitArray=function(){for(var e=[],r=this.readByte(),t=7;t>=0;t--)e.push(!!(r&1<<t));return e},a.prototype.readUnsigned=function(e){var r=this.readBytes(2);return e?(r[1]<<8)+r[0]:(r[0]<<8)+r[1]},r.exports=a},{}],2:[function(e,r,t){function a(e){this.stream=new s(e),this.output={}}function n(e){return e.reduce(function(e,r){return 2*e+r},0)}var s=e("./bytestream");a.prototype.parse=function(e){return this.parseParts(this.output,e),this.output},a.prototype.parseParts=function(e,r){for(var t=0;t<r.length;t++){var a=r[t];this.parsePart(e,a)}},a.prototype.parsePart=function(e,r){var t,a=r.label;if(!r.requires||r.requires(this.stream,this.output,e))if(r.loop){for(var n=[];r.loop(this.stream);){var s={};this.parseParts(s,r.parts),n.push(s)}e[a]=n}else r.parts?(t={},this.parseParts(t,r.parts),e[a]=t):r.parser?(t=r.parser(this.stream,this.output,e),r.skip||(e[a]=t)):r.bits&&(e[a]=this.parseBits(r.bits))},a.prototype.parseBits=function(e){var r={},t=this.stream.readBitArray();for(var a in e){var s=e[a];r[a]=s.length?n(t.slice(s.index,s.index+s.length)):t[s.index]}return r},r.exports=a},{"./bytestream":1}],3:[function(e,r,t){var a={readByte:function(){return function(e){return e.readByte()}},readBytes:function(e){return function(r){return r.readBytes(e)}},readString:function(e){return function(r){return r.readString(e)}},readUnsigned:function(e){return function(r){return r.readUnsigned(e)}},readArray:function(e,r){return function(t,a,n){for(var s=r(t,a,n),i=new Array(s),o=0;s>o;o++)i[o]=t.readBytes(e);return i}}};r.exports=a},{}],4:[function(e,r,t){var a=window.GIF||{};a=e("./gif"),window.GIF=a},{"./gif":5}],5:[function(e,r,t){function a(e){var r=new Uint8Array(e),t=new n(r);this.raw=t.parse(s),this.raw.hasImages=!1;for(var a=0;a<this.raw.frames.length;a++)if(this.raw.frames[a].image){this.raw.hasImages=!0;break}}var n=e("../bower_components/js-binary-schema-parser/src/dataparser"),s=e("./schema");a.prototype.decompressFrame=function(e,r){function t(e,r,t){var a,n,s,i,o,p,l,u,d,c,f,h,y,g,b,m,v=4096,x=-1,w=t,B=new Array(t),k=new Array(v),A=new Array(v),S=new Array(v+1);for(h=e,n=1<<h,o=n+1,a=n+2,l=x,i=h+1,s=(1<<i)-1,d=0;n>d;d++)k[d]=0,A[d]=d;for(f=u=count=y=g=m=b=0,c=0;w>c;){if(0===g){if(i>u){f+=r[b]<<u,u+=8,b++;continue}if(d=f&s,f>>=i,u-=i,d>a||d==o)break;if(d==n){i=h+1,s=(1<<i)-1,a=n+2,l=x;continue}if(l==x){S[g++]=A[d],l=d,y=d;continue}for(p=d,d==a&&(S[g++]=y,d=l);d>n;)S[g++]=A[d],d=k[d];y=255&A[d],S[g++]=y,v>a&&(k[a]=l,A[a]=y,a++,0===(a&s)&&v>a&&(i++,s+=a)),l=p}g--,B[m++]=S[g],c++}for(c=m;w>c;c++)B[c]=0;return B}function a(e,r){for(var t=new Array(e.length),a=e.length/r,n=function(a,n){var s=e.slice(n*r,(n+1)*r);t.splice.apply(t,[a*r,r].concat(s))},s=[0,4,2,1],i=[8,8,4,2],o=0,p=0;4>p;p++)for(var l=s[p];a>l;l+=i[p])n(l,o),o++;return t}function n(e){for(var r=e.pixels.length,t=new Uint8ClampedArray(4*r),a=0;r>a;a++){var n=4*a,s=e.pixels[a],i=e.colorTable[s];t[n]=i[0],t[n+1]=i[1],t[n+2]=i[2],t[n+3]=s!==e.transparentIndex?255:0}return t}if(e>=this.raw.frames.length)return null;var s=this.raw.frames[e];if(s.image){var i=s.image.descriptor.width*s.image.descriptor.height,o=t(s.image.data.minCodeSize,s.image.data.blocks,i);s.image.descriptor.lct.interlaced&&(o=a(o,s.image.descriptor.width));var p={pixels:o,dims:{top:s.image.descriptor.top,left:s.image.descriptor.left,width:s.image.descriptor.width,height:s.image.descriptor.height}};return p.colorTable=s.image.descriptor.lct&&s.image.descriptor.lct.exists?s.image.lct:this.raw.gct,s.gce&&(p.delay=(Number.isFinite(s.gce.delay)&&Number.isSafeInteger(s.gce.delay)&&s.gce.delay>=0)?s.gce.delay*10:100,p.disposalType=s.gce.extras.disposal,s.gce.extras.transparentColorGiven&&(p.transparentIndex=s.gce.transparentColorIndex)),r&&(p.patch=n(p)),p}return null},a.prototype.decompressFrames=function(e){for(var r=[],t=0;t<this.raw.frames.length;t++){var a=this.raw.frames[t];a.image&&r.push(this.decompressFrame(t,e))}return r},r.exports=a},{"../bower_components/js-binary-schema-parser/src/dataparser":2,"./schema":6}],6:[function(e,r,t){var a=e("../bower_components/js-binary-schema-parser/src/parsers"),n={label:"blocks",parser:function(e){for(var r=[],t=0,a=e.readByte();a!==t;a=e.readByte())r=r.concat(e.readBytes(a));return r}},s={label:"gce",requires:function(e){var r=e.peekBytes(2);return 33===r[0]&&249===r[1]},parts:[{label:"codes",parser:a.readBytes(2),skip:!0},{label:"byteSize",parser:a.readByte()},{label:"extras",bits:{future:{index:0,length:3},disposal:{index:3,length:3},userInput:{index:6},transparentColorGiven:{index:7}}},{label:"delay",parser:a.readUnsigned(!0)},{label:"transparentColorIndex",parser:a.readByte()},{label:"terminator",parser:a.readByte(),skip:!0}]},i={label:"image",requires:function(e){var r=e.peekByte();return 44===r},parts:[{label:"code",parser:a.readByte(),skip:!0},{label:"descriptor",parts:[{label:"left",parser:a.readUnsigned(!0)},{label:"top",parser:a.readUnsigned(!0)},{label:"width",parser:a.readUnsigned(!0)},{label:"height",parser:a.readUnsigned(!0)},{label:"lct",bits:{exists:{index:0},interlaced:{index:1},sort:{index:2},future:{index:3,length:2},size:{index:5,length:3}}}]},{label:"lct",requires:function(e,r,t){return t.descriptor.lct.exists},parser:a.readArray(3,function(e,r,t){return Math.pow(2,t.descriptor.lct.size+1)})},{label:"data",parts:[{label:"minCodeSize",parser:a.readByte()},n]}]},o={label:"text",requires:function(e){var r=e.peekBytes(2);return 33===r[0]&&1===r[1]},parts:[{label:"codes",parser:a.readBytes(2),skip:!0},{label:"blockSize",parser:a.readByte()},{label:"preData",parser:function(e,r,t){return e.readBytes(t.text.blockSize)}},n]},p={label:"application",requires:function(e,r,t){var a=e.peekBytes(2);return 33===a[0]&&255===a[1]},parts:[{label:"codes",parser:a.readBytes(2),skip:!0},{label:"blockSize",parser:a.readByte()},{label:"id",parser:function(e,r,t){return e.readString(t.blockSize)}},n]},l={label:"comment",requires:function(e,r,t){var a=e.peekBytes(2);return 33===a[0]&&254===a[1]},parts:[{label:"codes",parser:a.readBytes(2),skip:!0},n]},u={label:"frames",parts:[s,p,l,i,o],loop:function(e){var r=e.peekByte();return 33===r||44===r}},d=[{label:"header",parts:[{label:"signature",parser:a.readString(3)},{label:"version",parser:a.readString(3)}]},{label:"lsd",parts:[{label:"width",parser:a.readUnsigned(!0)},{label:"height",parser:a.readUnsigned(!0)},{label:"gct",bits:{exists:{index:0},resolution:{index:1,length:3},sort:{index:4},size:{index:5,length:3}}},{label:"backgroundColorIndex",parser:a.readByte()},{label:"pixelAspectRatio",parser:a.readByte()}]},{label:"gct",requires:function(e,r){return r.lsd.gct.exists},parser:a.readArray(3,function(e,r){return Math.pow(2,r.lsd.gct.size+1)})},u];r.exports=d},{"../bower_components/js-binary-schema-parser/src/parsers":3}]},{},[4]);
  </script>
  <script>
    // Constants for resizing flow
    console.log("[Upload] __gifencLoaded:", window.__gifencLoaded);
    console.log("[Upload] gifenc:", window.gifenc);
    if (typeof logUpload === "function") {
      logUpload("gifenc library block executed: " + (window.__gifencLoaded ? "yes" : "no"));
      logUpload("gifenc available: " + (window.gifenc ? "yes" : "no"));
      logUpload("gifenc.GIFEncoder: " + (!!window.gifenc?.GIFEncoder ? "yes" : "no"));
      logUpload("gifenc.quantize: " + (!!window.gifenc?.quantize ? "yes" : "no"));
      logUpload("gifenc.applyPalette: " + (!!window.gifenc?.applyPalette ? "yes" : "no"));
    }
    const MAX_INPUT_MB = 50;
    const MAX_OUTPUT_BYTES = 204800;  // Single source of truth: 200KB exact (200 * 1024 bytes)

    // SweetAlert Clone Lifecycle & Scroll Lock Helpers
    let modalScrollY = 0;
    let modalScrollLocked = false;
    let prevBodyPosition = '';
    let prevBodyTop = '';
    let prevBodyLeft = '';
    let prevBodyRight = '';
    let prevBodyWidth = '';
    let prevBodyOverflow = '';
    let modalActiveTimer = null;

    function clearModalTimer() {
      if (modalActiveTimer !== null) {
        clearInterval(modalActiveTimer);
        clearTimeout(modalActiveTimer);
        modalActiveTimer = null;
      }
    }

    function lockModalScroll() {
      if (modalScrollLocked) return;
      modalScrollY = window.scrollY || window.pageYOffset || 0;
      prevBodyPosition = document.body.style.position;
      prevBodyTop = document.body.style.top;
      prevBodyLeft = document.body.style.left;
      prevBodyRight = document.body.style.right;
      prevBodyWidth = document.body.style.width;
      prevBodyOverflow = document.body.style.overflow;

      document.body.style.position = 'fixed';
      document.body.style.top = '-' + modalScrollY + 'px';
      document.body.style.left = '0';
      document.body.style.right = '0';
      document.body.style.width = '100%';
      document.body.style.overflow = 'hidden';
      modalScrollLocked = true;
    }

    function unlockModalScroll() {
      if (!modalScrollLocked) return;
      document.body.style.position = prevBodyPosition;
      document.body.style.top = prevBodyTop;
      document.body.style.left = prevBodyLeft;
      document.body.style.right = prevBodyRight;
      document.body.style.width = prevBodyWidth;
      document.body.style.overflow = prevBodyOverflow;
      window.scrollTo(0, modalScrollY);
      modalScrollLocked = false;
    }

    function openModal() {
      lockModalScroll();
      const overlay = document.getElementById('modal-overlay');
      if (overlay) {
        overlay.classList.add('active');
      }
    }

    function closeModal() {
      clearModalTimer();
      const overlay = document.getElementById('modal-overlay');
      if (overlay) {
        overlay.classList.remove('active');
      }
      unlockModalScroll();
    }

    function showModal(htmlContent, showButton, btnText, btnCallback) {
      clearModalTimer();
      const iconEl = document.getElementById('modal-icon');
      const btnEl = document.getElementById('modal-btn');
      
      // Default reset
      if (iconEl) {
        iconEl.className = 'modal-icon';
        iconEl.style.display = 'none';
      }
      if (btnEl) {
        btnEl.style.display = 'none';
      }
      
      openModal();
    }

    function showLoadingModal(title, desc) {
      clearModalTimer();
      const iconEl = document.getElementById('modal-icon');
      const titleEl = document.getElementById('modal-title');
      const descEl = document.getElementById('modal-desc');
      const btnEl = document.getElementById('modal-btn');
      const progContainer = document.getElementById('progress-container');
      const progBar = document.getElementById('progress-bar');
      
      if (iconEl) {
        iconEl.className = 'modal-icon loading';
        iconEl.style.display = 'flex';
        iconEl.innerHTML = '';
      }
      
      if (titleEl) titleEl.innerText = title;
      if (descEl) descEl.innerText = desc;
      if (btnEl) btnEl.style.display = 'none';
      
      if (progBar) progBar.style.width = '0%';
      if (progContainer) progContainer.style.display = 'block';
      
      openModal();
    }

    function updateLoadingProgress(percent, text) {
      const descEl = document.getElementById('modal-desc');
      const progBar = document.getElementById('progress-bar');
      if (descEl) descEl.innerText = text;
      if (progBar) progBar.style.width = percent + '%';
    }

    function showSuccessModal(title, callback, countdownText, durationSeconds, customDesc) {
      clearModalTimer();
      const iconEl = document.getElementById('modal-icon');
      const titleEl = document.getElementById('modal-title');
      const descEl = document.getElementById('modal-desc');
      const btnEl = document.getElementById('modal-btn');
      const progContainer = document.getElementById('progress-container');
      if (progContainer) progContainer.style.display = 'none';
      
      if (iconEl) {
        iconEl.className = 'modal-icon success';
        iconEl.style.display = 'flex';
        iconEl.innerHTML = '&#10004;';
      }
      
      if (titleEl) titleEl.innerText = title;
      
      let callbackExecuted = false;
      const execCallbackOnce = function() {
        if (!callbackExecuted) {
          callbackExecuted = true;
          if (callback) callback();
        }
      };

      let timeLeft = (durationSeconds !== undefined && durationSeconds > 0) ? durationSeconds : 0;
      
      if (timeLeft > 0) {
        if (descEl) descEl.innerText = countdownText + timeLeft + " วินาที...";
        
        if (timeLeft > 3) {
          if (btnEl) {
            btnEl.innerText = "กรุณารอสักครู่...";
            btnEl.disabled = true;
            btnEl.style.opacity = "0.5";
            btnEl.style.cursor = "not-allowed";
            btnEl.style.display = "block";
            btnEl.onclick = null;
          }
        } else {
          if (btnEl) btnEl.style.display = "none";
        }
        
        modalActiveTimer = setInterval(function() {
          timeLeft--;
          if (timeLeft <= 0) {
            closeModal();
            execCallbackOnce();
          } else {
            if (descEl) descEl.innerText = countdownText + timeLeft + " วินาที...";
          }
        }, 1000);
      } else {
        if (descEl) descEl.innerText = customDesc || "การดำเนินการเสร็จสิ้นสมบูรณ์";
        if (btnEl) {
          btnEl.innerText = "ตกลง";
          btnEl.disabled = false;
          btnEl.style.opacity = "1";
          btnEl.style.cursor = "pointer";
          btnEl.className = "btn";
          btnEl.style.background = "#10b981";
          btnEl.style.color = "#fff";
          btnEl.style.display = "block";
          btnEl.onclick = function() {
            closeModal();
            execCallbackOnce();
          };
        }
      }
      
      openModal();
    }

    function showErrorModal(errorMsg) {
      clearModalTimer();
      const iconEl = document.getElementById('modal-icon');
      const titleEl = document.getElementById('modal-title');
      const descEl = document.getElementById('modal-desc');
      const btnEl = document.getElementById('modal-btn');
      const progContainer = document.getElementById('progress-container');
      if (progContainer) progContainer.style.display = 'none';
      
      // Re-enable form buttons immediately on error
      document.querySelectorAll('form button[type="submit"]').forEach(btn => {
        btn.disabled = false;
        btn.style.opacity = "1";
      });
      
      if (iconEl) {
        iconEl.className = 'modal-icon error';
        iconEl.style.display = 'flex';
        iconEl.innerHTML = '&#10008;';
      }
      
      if (titleEl) titleEl.innerText = "เกิดข้อผิดพลาด!";
      if (descEl) descEl.innerText = errorMsg;
      
      if (btnEl) {
        btnEl.innerText = "ปิด";
        btnEl.className = "btn btn-boot";
        btnEl.style.display = "block";
        btnEl.onclick = function() {
          closeModal();
          document.querySelectorAll('form button[type="submit"]').forEach(btn => {
            btn.disabled = false;
            btn.style.opacity = "1";
          });
        };
      }
      
      openModal();
    }

    // Helper function to log upload steps to the diagnostic console
    function logUpload(msg) {
      console.log("[UploadDebug] " + msg);
      const logEl = document.getElementById("upload-debug-log");
      if (logEl) {
        const timestamp = new Date().toLocaleTimeString();
        if (logEl.innerText === "Waiting for upload action...") {
          logEl.innerText = "[" + timestamp + "] " + msg;
        } else {
          logEl.innerText += "\n[" + timestamp + "] " + msg;
        }
        logEl.scrollTop = logEl.scrollHeight;
      }
    }

    /**
     * Compute the LZW Minimum Code Size required for a given GIF palette.
     * Formula: max(2, ceil(log2(paletteLength)))
     *
     * This value MUST be passed as `colorDepth` to encoder.writeFrame() for every frame.
     * Verified: matches colorTableSize() in embedded gifenc for all palette sizes including
     * non-power-of-two (e.g. palette.length=33 -> colorTableSize=6 -> paletteColorDepth=6).
     *
     * Note: In gifenc, colorDepth is also placed into the Color Resolution field of the LSD
     * (as colorDepth-1). Do NOT fix the Color Resolution field — only LZW Minimum Code Size
     * is the root cause of DECODER_RESULT=FAIL.
     *
     * @param {Array} palette - Array of color entries [[r,g,b], ...]
     * @returns {number} LZW Minimum Code Size (integer 2-8)
     * @throws {Error} If palette.length < 2 or > 256
     */
    function paletteColorDepth(palette) {
      if (!palette || palette.length < 2) {
        throw new Error("Invalid GIF palette: minimum 2 colors required");
      }
      if (palette.length > 256) {
        throw new Error("Invalid GIF palette: maximum 256 colors allowed");
      }
      return Math.max(2, Math.ceil(Math.log2(palette.length)));
    }

    function normalizePaletteToSize(palette, targetColors) {
      if (![16, 32, 64].includes(targetColors)) {
        throw new Error("Unsupported palette size: " + targetColors);
      }

      const result = Array.isArray(palette) ? palette.slice(0, targetColors) : [];
      while (result.length < targetColors) {
        result.push([0, 0, 0]);
      }

      for (let i = 0; i < result.length; i++) {
        const c = result[i];
        if (
          !Array.isArray(c) ||
          c.length < 3 ||
          !Number.isInteger(c[0]) ||
          !Number.isInteger(c[1]) ||
          !Number.isInteger(c[2]) ||
          c[0] < 0 || c[0] > 255 ||
          c[1] < 0 || c[1] > 255 ||
          c[2] < 0 || c[2] > 255
        ) {
          throw new Error("Invalid palette RGB entry at index " + i);
        }
      }

      return result;
    }

    function normalizeGifDelay(delayMs) {
      if (!Number.isFinite(delayMs)) {
        return 100;
      }
      let d = Math.round(delayMs / 10) * 10;
      if (d < 10) d = 10;
      if (d > 655350) d = 655350;
      return d;
    }

    const MAX_INPUT_FRAMES = 250;              // Maximum raw frames allowed before decompression
    const MAX_DECOMPRESSED_PIXELS = 15000000; // 15 Megapixels decompression guard
    const MAX_RENDERED_FRAMES = 250;           // Maximum rendered 320x172 RGBA frames in heap
    const MAX_RENDERED_FRAME_BYTES = 60 * 1024 * 1024; // 60 MB heap guard

    // Client-Side Bounds-Safe Binary & Semantic Validator
    function validateGifStructure(bytes, expectedGctEntries, expectedFrameCount) {
      if (!bytes || bytes.length < 14) return { valid: false, reason: "FILE_TOO_SMALL" };
      
      // 1. Signature Check (6 bytes: GIF87a or GIF89a)
      const sig = String.fromCharCode(bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]);
      if (sig !== "GIF87a" && sig !== "GIF89a") return { valid: false, reason: "INVALID_SIGNATURE" };
      
      // 2. Logical Screen Descriptor (7 bytes: bytes 6..12)
      const w = bytes[6] | (bytes[7] << 8);
      const h = bytes[8] | (bytes[9] << 8);
      if (w !== 320 || h !== 172) return { valid: false, reason: "INVALID_CANVAS_DIMS" };
      
      const packed = bytes[10];
      const hasGCT = (packed >> 7) & 1;
      if (!hasGCT) return { valid: false, reason: "MISSING_GCT" };
      
      const gctEntries = 2 << (packed & 7);
      if (expectedGctEntries && gctEntries !== expectedGctEntries) {
        return { valid: false, reason: "GCT_SIZE_MISMATCH" };
      }
      
      const gctBytes = 3 * gctEntries;
      let pos = 13 + gctBytes;
      if (pos > bytes.length) return { valid: false, reason: "GCT_OVERRUN" };
      
      let frameCount = 0;
      let trailerFound = false;
      
      while (pos < bytes.length) {
        const blockByte = bytes[pos];
        
        // Check duplicate GIF signature at block boundary
        if (blockByte === 0x47 && pos + 6 <= bytes.length) {
          const peekSig = String.fromCharCode(bytes[pos], bytes[pos+1], bytes[pos+2], bytes[pos+3], bytes[pos+4], bytes[pos+5]);
          if (peekSig === "GIF87a" || peekSig === "GIF89a") {
            return { valid: false, reason: "DUPLICATE_HEADER" };
          }
        }
        
        // Trailer 0x3B
        if (blockByte === 0x3B) {
          trailerFound = true;
          if (pos !== bytes.length - 1) return { valid: false, reason: "DATA_AFTER_TRAILER" };
          break;
        }
        
        // Extension Block 0x21
        if (blockByte === 0x21) {
          if (pos + 2 > bytes.length) return { valid: false, reason: "TRUNCATED_EXT_HEADER" };
          pos += 2; // skip introducer + label
          let extTerminated = false;
          while (pos < bytes.length) {
            const subLen = bytes[pos++];
            if (subLen === 0) { extTerminated = true; break; }
            if (pos + subLen > bytes.length) return { valid: false, reason: "EXT_SUBBLOCK_OVERRUN" };
            pos += subLen;
          }
          if (!extTerminated) return { valid: false, reason: "EXT_MISSING_TERMINATOR" };
          continue;
        }
        
        // Image Descriptor 0x2C
        if (blockByte === 0x2C) {
          if (pos + 10 > bytes.length) return { valid: false, reason: "TRUNCATED_IMAGE_DESCRIPTOR" };
          
          const imageLeft   = bytes[pos + 1] | (bytes[pos + 2] << 8);
          const imageTop    = bytes[pos + 3] | (bytes[pos + 4] << 8);
          const imageWidth  = bytes[pos + 5] | (bytes[pos + 6] << 8);
          const imageHeight = bytes[pos + 7] | (bytes[pos + 8] << 8);
          
          if (
            !Number.isInteger(imageLeft) || !Number.isInteger(imageTop) ||
            !Number.isInteger(imageWidth) || !Number.isInteger(imageHeight) ||
            imageWidth <= 0 || imageHeight <= 0 ||
            imageLeft < 0 || imageTop < 0 ||
            imageLeft + imageWidth > 320 ||
            imageTop + imageHeight > 172
          ) {
            return { valid: false, reason: "IMAGE_BOUNDS_INVALID" };
          }
          
          const idPacked = bytes[pos + 9];
          pos += 10;
          
          const hasLCT = (idPacked >> 7) & 1;
          if (hasLCT) {
            const lctEntries = 2 << (idPacked & 7);
            const lctBytes = 3 * lctEntries;
            if (pos + lctBytes > bytes.length) return { valid: false, reason: "LCT_OVERRUN" };
            pos += lctBytes;
          }
          
          if (pos >= bytes.length) return { valid: false, reason: "MISSING_LZW_MIN_CODE_SIZE" };
          const lzwMinCodeSize = bytes[pos++];
          if (lzwMinCodeSize < 2 || lzwMinCodeSize > 8) return { valid: false, reason: "INVALID_LZW_MIN_CODE_SIZE" };
          
          const expectedLzwMinCodeSize =
            expectedGctEntries === 64 ? 6 :
            expectedGctEntries === 32 ? 5 :
            expectedGctEntries === 16 ? 4 :
            null;
          
          if (expectedLzwMinCodeSize !== null && lzwMinCodeSize !== expectedLzwMinCodeSize) {
            return { valid: false, reason: "LZW_MIN_CODE_SIZE_MISMATCH" };
          }
          
          let imgTerminated = false;
          while (pos < bytes.length) {
            const subLen = bytes[pos++];
            if (subLen === 0) { imgTerminated = true; break; }
            if (pos + subLen > bytes.length) return { valid: false, reason: "IMG_SUBBLOCK_OVERRUN" };
            pos += subLen;
          }
          if (!imgTerminated) return { valid: false, reason: "IMG_MISSING_TERMINATOR" };
          
          // Increment frameCount ONLY after complete frame data validation
          frameCount++;
          continue;
        }
        
        return { valid: false, reason: "UNKNOWN_BLOCK_0x" + blockByte.toString(16).toUpperCase() };
      }
      
      if (!trailerFound) return { valid: false, reason: "MISSING_TRAILER" };
      if (frameCount === 0) return { valid: false, reason: "NO_FRAMES" };
      if (expectedFrameCount && frameCount !== expectedFrameCount) {
        return { valid: false, reason: "FRAME_COUNT_MISMATCH" };
      }
      
      return { valid: true, frameCount: frameCount, gctEntries: gctEntries };
    }

    // Process static images (PNG/JPEG/WEBP)
    function processAsStaticImage(file, type, onProgress, onSuccess, onError) {
      logUpload("decode started");
      onProgress(20, "กำลังเปิดไฟล์ภาพ...");
      
      const img = new Image();
      const url = URL.createObjectURL(file);
      
      img.onload = function() {
        logUpload("decode success");
        logUpload("detected original width/height: " + img.width + "x" + img.height);
        logUpload("resize started");
        onProgress(40, "กำลังวาดและย่อขนาดภาพ...");
        const canvas = document.createElement('canvas');
        canvas.width = 320;
        canvas.height = 172;
        const ctx = canvas.getContext('2d');
        
        ctx.fillStyle = '#000000';
        ctx.fillRect(0, 0, 320, 172);
        
        const srcAspect = img.width / img.height;
        const targetAspect = 320 / 172;
        let drawW, drawH, drawX, drawY;
        
        if (srcAspect > targetAspect) {
          drawW = 320;
          drawH = 320 / srcAspect;
          drawX = 0;
          drawY = (172 - drawH) / 2;
        } else {
          drawH = 172;
          drawW = 172 * srcAspect;
          drawX = (320 - drawW) / 2;
          drawY = 0;
        }
        
        ctx.drawImage(img, drawX, drawY, drawW, drawH);
        logUpload("canvas resize completed");
        
        onProgress(70, "กำลังเตรียมไฟล์ภาพ...");
        
        setTimeout(function() {
          try {
            const gifencLib = window.gifenc || (typeof gifenc !== "undefined" ? gifenc : null);
            if (!gifencLib) {
              throw new Error("ระบบแปลงไฟล์ภาพยังโหลดไม่ครบ: gifenc missing");
            }
            
            const imgData = ctx.getImageData(0, 0, 320, 172).data;
            const format = 'rgb444';
            
            const sampledPixels = [];
            for (let y = 0; y < 172; y += 4) {
              for (let x = 0; x < 320; x += 4) {
                const idx = (y * 320 + x) * 4;
                sampledPixels.push(imgData[idx], imgData[idx+1], imgData[idx+2], imgData[idx+3]);
              }
            }
            const sampledData = new Uint8Array(sampledPixels);
            
            // Power-of-two palette tiers: 64, 32, 16
            const presets = [64, 32, 16];
            let outputBytes = null;
            let chosenColors = 0;
            let validationResult = null;
            
            for (let i = 0; i < presets.length; i++) {
              const colors = presets[i];
              onProgress(70 + i * 8, "กำลังปรับขนาดไฟล์อัตโนมัติ... (" + colors + " สี)");
              
              const rawPalette = gifencLib.quantize(sampledData, colors, { format });
              const palette = normalizePaletteToSize(rawPalette, colors);
              const index = gifencLib.applyPalette(imgData, palette, format);
              const colorDepth = Math.round(Math.log2(colors));
              
              const encoder = gifencLib.GIFEncoder();
              encoder.writeFrame(index, 320, 172, { palette, delay: 0, colorDepth });
              encoder.finish();
              
              const candidateBytes = encoder.bytes();
              if (candidateBytes.length <= MAX_OUTPUT_BYTES) {
                validationResult = validateGifStructure(candidateBytes, colors, 1);
                if (validationResult.valid) {
                  outputBytes = candidateBytes;
                  chosenColors = colors;
                  break;
                } else {
                  console.warn("[GIF Validation FAIL]", validationResult.reason);
                }
              }
            }
            
            URL.revokeObjectURL(url);
            
            if (!outputBytes || !(outputBytes instanceof Uint8Array) || outputBytes.length === 0 || outputBytes.length > MAX_OUTPUT_BYTES) {
              onError("ไฟล์รูปภาพใหญ่เกินไป ระบบย่อให้อัตโนมัติแล้วแต่ยังเกิน 200KB กรุณาเลือกรูปที่รายละเอียดน้อยลง");
              return;
            }
            
            const origKB = (file.size / 1024).toFixed(1);
            const optKB = (outputBytes.length / 1024).toFixed(1);
            const compressionRatio = ((1 - (outputBytes.length / file.size)) * 100).toFixed(1);
            
            const reportMsg = 
              "[GIF_OPT]\n" +
              "Type=STATIC\n" +
              "Original:\n" +
              "  size=" + file.size + " (" + origKB + " KB)\n" +
              "  avgFPS=N/A\n" +
              "Optimized:\n" +
              "  size=" + outputBytes.length + " (" + optKB + " KB)\n" +
              "  colors=" + chosenColors + "\n" +
              "  paletteEntries=" + (validationResult ? validationResult.gctEntries : chosenColors) + "\n" +
              "  canvas=320x172\n" +
              "  avgFPS=N/A\n" +
              "Preset=" + chosenColors + "C\n" +
              "Compression=" + compressionRatio + "%\n" +
              "Validation=PASS";
            
            console.log(reportMsg);
            logUpload(reportMsg);
            
            const blob = new Blob([outputBytes], { type: 'image/gif' });
            if (blob.size !== outputBytes.length) {
              onError("Blob integrity error: byte length mismatch");
              return;
            }
            onSuccess(blob);
          } catch (err) {
            URL.revokeObjectURL(url);
            onError("เกิดข้อผิดพลาดในการประมวลผลภาพ: " + err.message);
          }
        }, 50);
      };
      
      img.onerror = function() {
        URL.revokeObjectURL(url);
        onError("ไม่สามารถอ่านไฟล์ภาพนี้ได้ กรุณาลองใช้ JPG, PNG หรือ WEBP อื่น");
      };
      
      img.src = url;
    }

    // Process GIFs (animated or static)
    function resizeAndUploadGIF(file, type, onProgress, onSuccess, onError) {
      const isGif = file.type === 'image/gif' || file.name.toLowerCase().endsWith('.gif');

      if (!isGif) {
        processAsStaticImage(file, type, onProgress, onSuccess, onError);
        return;
      }

      onProgress(5, "กำลังโหลดตัวถอดรหัส GIF...");
      const reader = new FileReader();
      reader.onload = function(e) {
        try {
          const gifencLib = window.gifenc || (typeof gifenc !== "undefined" ? gifenc : null);
          if (!gifencLib) {
            throw new Error("ระบบแปลงไฟล์ภาพยังโหลดไม่ครบ: gifenc missing");
          }
          onProgress(10, "กำลังอ่านโครงสร้างของรูปภาพ...");
          
          let gifReader;
          try {
            gifReader = new GIF(e.target.result);
          } catch (parseErr) {
            onError("ไม่สามารถอ่านโครงสร้างไฟล์ GIF ได้: " + parseErr.message);
            return;
          }

          // 1. Strict raw.frames structural validation
          if (!gifReader.raw || !Array.isArray(gifReader.raw.frames) || gifReader.raw.frames.length === 0) {
            onError("ไม่พบโครงสร้าง GIF frame ที่ถูกต้อง");
            return;
          }

          // 2. Verified Logical Screen Descriptor parsing before preflight
          const lsd = gifReader.raw && gifReader.raw.lsd;
          if (!lsd || !Number.isSafeInteger(lsd.width) || !Number.isSafeInteger(lsd.height) || lsd.width <= 0 || lsd.height <= 0) {
            onError("โครงสร้าง Logical Screen Descriptor ของ GIF ไม่ถูกต้อง");
            return;
          }
          const gifWidth = lsd.width;
          const gifHeight = lsd.height;

          // 3. Preflight Image Frame Counting, Geometry Bounds & Overflow-Safe Pixel Budget
          let inputImageFrameCount = 0;
          let totalRawPixels = 0;

          for (let fIdx = 0; fIdx < gifReader.raw.frames.length; fIdx++) {
            const rf = gifReader.raw.frames[fIdx];
            if (rf && rf.image && rf.image.descriptor) {
              inputImageFrameCount++;
              if (inputImageFrameCount > MAX_INPUT_FRAMES) {
                onError("จำนวนเฟรมภาพของ GIF สูงเกินขีดจำกัด (" + inputImageFrameCount + " เฟรม, สูงสุด " + MAX_INPUT_FRAMES + " เฟรม)");
                return;
              }
              
              const left = rf.image.descriptor.left;
              const top  = rf.image.descriptor.top;
              const rw   = rf.image.descriptor.width;
              const rh   = rf.image.descriptor.height;
              const right = left + rw;
              const bottom = top + rh;
              
              if (
                !Number.isSafeInteger(left) ||
                !Number.isSafeInteger(top) ||
                !Number.isSafeInteger(rw) ||
                !Number.isSafeInteger(rh) ||
                left < 0 ||
                top < 0 ||
                rw <= 0 ||
                rh <= 0 ||
                !Number.isSafeInteger(right) ||
                !Number.isSafeInteger(bottom) ||
                right > gifWidth ||
                bottom > gifHeight
              ) {
                onError("ขนาดหรือตำแหน่งของเฟรมภาพอยู่นอกขอบเขตของ Logical Screen");
                return;
              }
              
              const framePixels = rw * rh;
              if (!Number.isSafeInteger(framePixels)) {
                onError("ขนาดพิกเซลของเฟรมมีค่าเกินขอบเขต (Pixel count overflow)");
                return;
              }
              
              totalRawPixels += framePixels;
              if (!Number.isSafeInteger(totalRawPixels) || totalRawPixels > MAX_DECOMPRESSED_PIXELS) {
                onError("ขนาดพิกเซลรวมของ GIF สูงเกินขีดจำกัดหน่วยความจำ (" + totalRawPixels + " พิกเซล) กรุณาลดจำนวนเฟรมหรือความละเอียดต้นฉบับ");
                return;
              }
            }
          }

          if (inputImageFrameCount === 0) {
            onError("ไม่พบเฟรมภาพเคลื่อนไหวในไฟล์ GIF");
            return;
          }

          // 4. Safe Decompression
          let allFrames;
          try {
            allFrames = gifReader.decompressFrames(true);
          } catch (decodeErr) {
            onError("ไม่สามารถถอดรหัสไฟล์ GIF ได้: ไฟล์อาจมีความซับซ้อนสูงเกินไปหรือเสียหาย (" + decodeErr.message + ")");
            return;
          }
          
          if (!allFrames || allFrames.length === 0) {
            onError("ไม่พบเฟรมภาพเคลื่อนไหวในไฟล์ GIF หรือโครงสร้างไฟล์ไม่ถูกต้อง");
            return;
          }

          // 5. Overflow-Safe Post-Decompression Defense-in-Depth Guard
          let totalDecompressedPixels = 0;
          for (let i = 0; i < allFrames.length; i++) {
            const f = allFrames[i];
            if (!f || !f.dims) {
              onError("โครงสร้าง frame หลังถอดรหัสไม่ถูกต้อง");
              return;
            }
            const w = f.dims.width;
            const h = f.dims.height;
            if (!Number.isSafeInteger(w) || !Number.isSafeInteger(h) || w <= 0 || h <= 0) {
              onError("ขนาด frame หลังถอดรหัสไม่ถูกต้อง");
              return;
            }
            const pixels = w * h;
            if (!Number.isSafeInteger(pixels)) {
              onError("ขนาดพิกเซลหลังถอดรหัสเกินขอบเขต");
              return;
            }
            totalDecompressedPixels += pixels;
            if (!Number.isSafeInteger(totalDecompressedPixels) || totalDecompressedPixels > MAX_DECOMPRESSED_PIXELS) {
              onError("ขนาดพิกเซลรวมหลังถอดรหัสสูงเกินขีดจำกัดหน่วยความจำ (" + totalDecompressedPixels + " พิกเซล) กรุณาลดจำนวนเฟรมหรือความละเอียดต้นฉบับ");
              return;
            }
          }

          // 6. Determine Logical Screen Background Color from LSD / GCT with bounds check
          let bgColor = '#000000';
          if (gifReader.raw && Array.isArray(gifReader.raw.gct)) {
            const bgIndex = Number.isInteger(lsd.backgroundColorIndex) ? lsd.backgroundColorIndex : 0;
            if (bgIndex >= 0 && bgIndex < gifReader.raw.gct.length) {
              const bgEntry = gifReader.raw.gct[bgIndex];
              if (Array.isArray(bgEntry) && bgEntry.length >= 3) {
                bgColor = 'rgb(' + bgEntry[0] + ',' + bgEntry[1] + ',' + bgEntry[2] + ')';
              }
            }
          }

          console.log("[GIF Decompressed] Total frames:", allFrames.length, "Logical Canvas:", gifWidth, "x", gifHeight);

          const srcAspect = gifWidth / gifHeight;
          const targetAspect = 320 / 172;
          let drawW, drawH, drawX, drawY;

          if (srcAspect > targetAspect) {
            drawW = 320; drawH = 320 / srcAspect;
            drawY = (172 - drawH) / 2; drawX = 0;
          } else {
            drawW = 172 * srcAspect; drawH = 172;
            drawX = (320 - drawW) / 2; drawY = 0;
          }

          // Step 1: Pre-render all raw frames into 320x172 RGB buffers with correct Disposal 2 & 3 semantics
          const tempCanvas = document.createElement('canvas');
          tempCanvas.width  = gifWidth;
          tempCanvas.height = gifHeight;
          const tempCtx     = tempCanvas.getContext('2d');

          const targetCanvas = document.createElement('canvas');
          targetCanvas.width  = 320;
          targetCanvas.height = 172;
          const targetCtx     = targetCanvas.getContext('2d');

          tempCtx.fillStyle = bgColor;
          tempCtx.fillRect(0, 0, gifWidth, gifHeight);

          const renderedFrames = [];
          let decodedOriginalDuration = 0;
          let normalizedOriginalDuration = 0;
          let savedCanvasSnapshot = null;

          for (let i = 0; i < allFrames.length; i++) {
            const fObj = allFrames[i];
            const rawDecodedDelay = (Number.isFinite(fObj.delay) && fObj.delay >= 0) ? fObj.delay : 100;
            const frameDelay = normalizeGifDelay(fObj.delay);
            decodedOriginalDuration += rawDecodedDelay;
            normalizedOriginalDuration += frameDelay;

            const left = fObj.dims ? fObj.dims.left : 0;
            const top = fObj.dims ? fObj.dims.top : 0;
            const width = fObj.dims ? fObj.dims.width : 0;
            const height = fObj.dims ? fObj.dims.height : 0;

            if (
              !Number.isInteger(left) || !Number.isInteger(top) ||
              !Number.isInteger(width) || !Number.isInteger(height) ||
              left < 0 || top < 0 || width <= 0 || height <= 0 ||
              left + width > gifWidth || top + height > gifHeight
            ) {
              onError("ขนาดหรือตำแหน่งของเฟรมที่ " + (i + 1) + " เกินขอบเขตของภาพ Logical Screen");
              return;
            }

            // 1. Apply disposal of previous frame
            if (i > 0) {
              const prev = allFrames[i - 1];
              if (prev.disposalType === 2) {
                // Disposal 2: Restore to Logical Screen background color
                tempCtx.fillStyle = bgColor;
                tempCtx.fillRect(prev.dims.left, prev.dims.top, prev.dims.width, prev.dims.height);
              } else if (prev.disposalType === 3 && savedCanvasSnapshot) {
                // Disposal 3: Restore to previous snapshot
                tempCtx.putImageData(savedCanvasSnapshot, 0, 0);
                savedCanvasSnapshot = null; // Prevent accidental stale reuse
              }
            }

            // 2. If current frame specifies disposalType 3, take fresh snapshot AFTER previous disposal is applied
            if (fObj.disposalType === 3) {
              savedCanvasSnapshot = tempCtx.getImageData(0, 0, gifWidth, gifHeight);
            }

            // 3. Draw current frame patch
            const patchCanvas = document.createElement('canvas');
            patchCanvas.width  = width;
            patchCanvas.height = height;
            const patchCtx = patchCanvas.getContext('2d');
            const imgData  = patchCtx.createImageData(width, height);
            imgData.data.set(fObj.patch);
            patchCtx.putImageData(imgData, 0, 0);
            tempCtx.drawImage(patchCanvas, left, top);

            // 4. Render to 320x172 target
            targetCtx.fillStyle = '#000000';
            targetCtx.fillRect(0, 0, 320, 172);
            targetCtx.drawImage(tempCanvas, drawX, drawY, drawW, drawH);

            // Pre-allocation heap memory guard (checks budget before allocating new buffer)
            const BYTES_PER_RENDERED_FRAME = 320 * 172 * 4;
            const nextFrameCount = renderedFrames.length + 1;
            if (!Number.isSafeInteger(nextFrameCount)) {
              onError("Invalid rendered frame count");
              return;
            }
            const nextFrameBytes = nextFrameCount * BYTES_PER_RENDERED_FRAME;
            if (
              !Number.isSafeInteger(nextFrameBytes) ||
              nextFrameCount > MAX_RENDERED_FRAMES ||
              nextFrameBytes > MAX_RENDERED_FRAME_BYTES
            ) {
              onError("จำนวนเฟรมของ GIF สูงเกินขีดจำกัดหน่วยความจำ (" + allFrames.length + " เฟรม, สูงสุด " + MAX_RENDERED_FRAMES + " เฟรม)");
              return;
            }

            const frameData = targetCtx.getImageData(0, 0, 320, 172).data;
            renderedFrames.push({
              data: new Uint8Array(frameData),
              delay: frameDelay
            });
          }

          // Step 2: Exact Consecutive Duplicate Frame Merging & Delay Accumulation
          const dedupFrames = [];
          if (renderedFrames.length > 0) {
            let cur = { data: renderedFrames[0].data, delay: renderedFrames[0].delay };
            for (let i = 1; i < renderedFrames.length; i++) {
              const nxt = renderedFrames[i];
              let isMatch = true;
              const len = cur.data.length;
              for (let p = 0; p < len; p++) {
                if (cur.data[p] !== nxt.data[p]) {
                  isMatch = false;
                  break;
                }
              }
              if (isMatch) {
                cur.delay += nxt.delay; // Accumulate delay into retained frame
              } else {
                dedupFrames.push(cur);
                cur = { data: nxt.data, delay: nxt.delay };
              }
            }
            dedupFrames.push(cur);
          }

          let totalDedupDuration = 0;
          for (let i = 0; i < dedupFrames.length; i++) {
            totalDedupDuration += dedupFrames[i].delay;
          }
          const duplicatesRemoved = allFrames.length - dedupFrames.length;

          console.log("[GIF Dedup] Original frames:", allFrames.length, "Deduplicated frames:", dedupFrames.length, "Duplicates removed:", duplicatesRemoved);

          // Step 3: Progressive Quality Optimization Hierarchy (Power-of-Two Palette Tiers)
          const gifPresets = [
            { name: "64C_STEP_1", colors: 64, step: 1, desc: "กำลังบีบอัดระดับคุณภาพสูงสุด (64 สี - ทุกเฟรม)..." },
            { name: "64C_STEP_2", colors: 64, step: 2, desc: "กำลังบีบอัดระดับคุณภาพสูง (64 สี - สเต็ป 2)..." },
            { name: "64C_STEP_3", colors: 64, step: 3, desc: "กำลังปรับจำนวนเฟรม (64 สี - สเต็ป 3)..." },
            { name: "64C_STEP_4", colors: 64, step: 4, desc: "กำลังลดขนาดภาพ (64 สี - สเต็ป 4)..." },
            { name: "32C_STEP_2", colors: 32, step: 2, desc: "กำลังปรับแต่งพาเลตต์สีสำรอง (32 สี)..." },
            { name: "16C_STEP_2", colors: 16, step: 2, desc: "กำลังปรับแต่งขั้นสุดท้าย (16 สี)..." }
          ];

          let presetIdx = 0;

          function tryNextPreset() {
            if (presetIdx >= gifPresets.length) {
              onError("ไม่สามารถบีบอัด GIF ให้อยู่ภายใน 200 KB และผ่าน validation ได้ กรุณาลองใช้ GIF ที่สั้นลง หรือลดรายละเอียดของภาพต้นฉบับ");
              return;
            }

            const p = gifPresets[presetIdx];
            onProgress(15 + presetIdx * 13, p.desc);

            setTimeout(function() {
              try {
                // Exact delay accumulation across uniform step slices (including final odd slice)
                const step = p.step;
                const framesToEncode = [];
                for (let i = 0; i < dedupFrames.length; i += step) {
                  let accumulatedDelay = 0;
                  const end = Math.min(i + step, dedupFrames.length);
                  for (let k = i; k < end; k++) {
                    accumulatedDelay += dedupFrames[k].delay;
                  }
                  framesToEncode.push({
                    data: dedupFrames[i].data,
                    delay: accumulatedDelay
                  });
                }

                if (!Number.isSafeInteger(framesToEncode.length) || framesToEncode.length <= 0 || framesToEncode.length > MAX_RENDERED_FRAMES) {
                  onError("จำนวน output frame ไม่ถูกต้อง");
                  return;
                }

                // Isolated encoder and quantization per preset candidate
                const encoder = gifencLib.GIFEncoder();
                const format = 'rgb565';

                for (let frameIndex = 0; frameIndex < framesToEncode.length; frameIndex++) {
                  const f = framesToEncode[frameIndex];
                  const rawPalette = gifencLib.quantize(f.data, p.colors, { format });
                  const palette = normalizePaletteToSize(rawPalette, p.colors);
                  const index = gifencLib.applyPalette(f.data, palette, format);
                  const colorDepth = Math.round(Math.log2(p.colors));
                  encoder.writeFrame(index, 320, 172, { palette, delay: f.delay, colorDepth });
                }

                encoder.finish();
                const outputBytes = encoder.bytes();

                console.log("[GIF Preset]", p.name, "output size:", outputBytes.length, "bytes, frames:", framesToEncode.length);

                // HARD GATE: Validate binary & semantic metadata before upload
                if (outputBytes instanceof Uint8Array && outputBytes.length > 0 && outputBytes.length <= MAX_OUTPUT_BYTES) {
                  const validation = validateGifStructure(outputBytes, p.colors, framesToEncode.length);
                  if (!validation.valid) {
                    console.warn("[GIF Validation FAIL]", validation.reason, "preset:", p.name);
                    presetIdx++;
                    tryNextPreset();
                    return;
                  }

                  // Explicit Candidate Metadata Assertions
                  if (validation.frameCount !== framesToEncode.length) {
                    console.warn("[GIF Validation FAIL] Frame count mismatch", validation.frameCount, framesToEncode.length);
                    presetIdx++;
                    tryNextPreset();
                    return;
                  }

                  if (validation.gctEntries !== p.colors) {
                    console.warn("[GIF Validation FAIL] GCT size mismatch", validation.gctEntries, p.colors);
                    presetIdx++;
                    tryNextPreset();
                    return;
                  }

                  let totalOptimizedDuration = 0;
                  for (let i = 0; i < framesToEncode.length; i++) {
                    totalOptimizedDuration += framesToEncode[i].delay;
                  }

                  // Exact Duration Identity Assertion
                  if (totalOptimizedDuration !== normalizedOriginalDuration) {
                    console.warn("[GIF Validation FAIL] DURATION_MISMATCH expected:", normalizedOriginalDuration, "got:", totalOptimizedDuration);
                    presetIdx++;
                    tryNextPreset();
                    return;
                  }
                  
                  const durationDelta = totalOptimizedDuration - normalizedOriginalDuration;
                  const decSec = (decodedOriginalDuration / 1000).toFixed(1);
                  const normSec = (normalizedOriginalDuration / 1000).toFixed(1);
                  const optSec = (totalOptimizedDuration / 1000).toFixed(1);
                  const origKB = (file.size / 1024).toFixed(1);
                  const optKB = (outputBytes.length / 1024).toFixed(1);
                  const origFPS = decodedOriginalDuration > 0 ? (allFrames.length / (decodedOriginalDuration / 1000)).toFixed(1) : "0";
                  const optFPS = totalOptimizedDuration > 0 ? (framesToEncode.length / (totalOptimizedDuration / 1000)).toFixed(1) : "0";
                  const compressionRatio = ((1 - (outputBytes.length / file.size)) * 100).toFixed(1);

                  const reportMsg = 
                    "[GIF_OPT]\n" +
                    "Original:\n" +
                    "  frames=" + allFrames.length + "\n" +
                    "  size=" + file.size + " (" + origKB + " KB)\n" +
                    "  decodedDuration=" + decodedOriginalDuration + "ms (" + decSec + "s)\n" +
                    "  normalizedDuration=" + normalizedOriginalDuration + "ms (" + normSec + "s)\n" +
                    "  avgFPS=" + origFPS + "\n" +
                    "After Dedup:\n" +
                    "  frames=" + dedupFrames.length + "\n" +
                    "  duplicatesRemoved=" + duplicatesRemoved + "\n" +
                    "  duration=" + totalDedupDuration + "ms\n" +
                    "Optimized:\n" +
                    "  frames=" + framesToEncode.length + "\n" +
                    "  colors=" + p.colors + "\n" +
                    "  paletteEntries=" + validation.gctEntries + "\n" +
                    "  size=" + outputBytes.length + " (" + optKB + " KB)\n" +
                    "  duration=" + totalOptimizedDuration + "ms (" + optSec + "s)\n" +
                    "  durationDelta=" + durationDelta + "ms\n" +
                    "  avgPresentedFPS=" + optFPS + "\n" +
                    "Preset=" + p.name + "\n" +
                    "Step=" + p.step + "\n" +
                    "Compression=" + compressionRatio + "%\n" +
                    "Validation=PASS";

                  console.log(reportMsg);
                  logUpload(reportMsg);
                  if (p.colors < 64) {
                    logUpload("หมายเหตุ: ปรับลดจำนวนสีเป็น " + p.colors + " สีเพื่อให้ขนาดไฟล์ไม่เกิน 200KB");
                  }

                  // ONLY path to onSuccess(blob) with full integrity assertion
                  const blob = new Blob([outputBytes], { type: 'image/gif' });
                  if (blob.size !== outputBytes.length) {
                    onError("Output Blob size mismatch — upload blocked");
                    return;
                  }
                  onSuccess(blob);
                } else {
                  // Try next preset
                  presetIdx++;
                  tryNextPreset();
                }

              } catch (err) {
                console.warn("[GIF Preset Exception]", err.message);
                presetIdx++;
                tryNextPreset();
              }
            }, 30);
          }

          // Start preset optimization loop
          tryNextPreset();

        } catch (err) {
          onError("เกิดข้อผิดพลาดในการประมวลผลอนิเมชั่น: " + err.message);
        }
      };
      reader.onerror = function() { onError("ไม่สามารถอ่านข้อมูลไฟล์ได้"); };
      reader.readAsArrayBuffer(file);
    }

    var uploadBusy = false; // Frontend double-click/race lock

    function handleFormSubmit(form, type) {
      console.log("[Upload] upload button clicked");
      logUpload("upload button clicked");
      logUpload("upload button clicked for: " + type);
      if (uploadBusy) {
        logUpload("BLOCKED: upload already in progress");
        showErrorModal("กำลังอัปโหลดอยู่แล้ว กรุณารอจนกว่าจะเสร็จ");
        return;
      }
      uploadBusy = true;
      var fileInput = form.querySelector('input[type="file"]');
      if (fileInput.files && fileInput.files[0]) {
        var file = fileInput.files[0];
        
        logUpload("selected file: " + file.name + " (" + (file.size / 1024).toFixed(1) + " KB)");
        logUpload("selected file type: " + file.type);
        
        if (file.size > MAX_INPUT_MB * 1024 * 1024) {
          logUpload("Error: File exceeds limit of " + MAX_INPUT_MB + "MB");
          uploadBusy = false; // Release lock on validation rejection
          showErrorModal(
            "ไฟล์ต้นฉบับใหญ่เกินกำหนด\n\n" +
            "ไฟล์ต้นฉบับมีขนาดเกิน " + MAX_INPUT_MB + " MB (ขนาดไฟล์: " + (file.size / 1024 / 1024).toFixed(1) + " MB)\n" +
            "กรุณาเลือกไฟล์ที่มีขนาดไม่เกิน " + MAX_INPUT_MB + " MB"
          );
          return;
        }
        
        console.log("[Upload] File:", file.name, "Type:", file.type, "Size:", (file.size / 1024).toFixed(1) + "KB");
        var uploadStartMs = Date.now();
        var uploadCompleted = false;
        
        var isGifFile = file.type === 'image/gif' || file.name.toLowerCase().endsWith('.gif');
        logUpload("selected mode: GIF");
        var loadingTitle = "กำลังเตรียมไฟล์ GIF...";
        showLoadingModal(loadingTitle, "กำลังอ่านข้อมูลไฟล์...");
        
        // 20-second timeout protection
        var prepTimeout = setTimeout(function() {
          if (uploadCompleted) return; // late-callback guard
          uploadCompleted = true;
          logUpload("Error: Processing timeout reached (20s).");
          console.error("[Upload] Timeout after 20s. File:", file.name);
          showErrorModal("ไม่สามารถเตรียมไฟล์ GIF ได้ (หมดเวลา 20 วินาที)\n\nกรุณาลองใช้ GIF ที่สั้นลง หรือลดขนาดไฟล์ก่อนอัปโหลด");
        }, 20000);
        
        resizeAndUploadGIF(
          file, 
          type, 
          function(percent, text) {
            if (uploadCompleted) return; // late-callback guard
            updateLoadingProgress(percent, text);
          },
          function(resizedBlob) {
            // Validate GIF output size — must be <= 200KB to match firmware GIF_MAX_FILE_BYTES
            if (resizedBlob.size > MAX_OUTPUT_BYTES) {
              if (uploadCompleted) return;
              uploadCompleted = true;
              clearTimeout(prepTimeout);
              uploadBusy = false; // Release lock!
              var errText = "ไม่สามารถบีบอัด GIF ให้อยู่ภายใน 200 KB ได้\n\n(ขนาดหลังบีบอัด: " + (resizedBlob.size / 1024).toFixed(1) + " KB, สูงสุด 200 KB)\nกรุณาลองใช้ GIF ที่สั้นลง หรือลดรายละเอียดของภาพต้นฉบับ";
              logUpload("Error: " + errText);
              console.error("[Upload] Error:", errText);
              showErrorModal(errText);
              return;
            }

            if (uploadCompleted) return; // late-callback guard
            uploadCompleted = true;
            clearTimeout(prepTimeout);
            
            var prepTimeMs = Date.now() - uploadStartMs;
            logUpload("blob created");
            logUpload("output blob type: " + resizedBlob.type);
            logUpload("output blob size: " + (resizedBlob.size / 1024).toFixed(1) + " KB");
            logUpload("image preparation done in " + prepTimeMs + "ms");
            
            console.log("[Upload] Preparation done in", prepTimeMs, "ms. Output size:", (resizedBlob.size / 1024).toFixed(1) + "KB");
            updateLoadingProgress(100, "ย่อขนาดเสร็จแล้ว! กำลังส่งข้อมูลไปยังบอร์ด...");
            
            var formData = new FormData();
            var resizedFile = new File([resizedBlob], file.name, { type: 'image/gif' });
            formData.append("file", resizedFile);
            
            var xhr = new XMLHttpRequest();
            var url = "/upload?type=" + type;
            logUpload("upload URL: " + url);
            logUpload("multipart field: file");
            logUpload("upload started");
            console.log("[Upload] Sending to:", url);
            
            xhr.upload.addEventListener("progress", function(e) {
              if (e.lengthComputable) {
                var percent = Math.round((e.loaded / e.total) * 100);
                updateLoadingProgress(percent, "กำลังส่งข้อมูลไปยังบอร์ด... " + percent + "%");
              }
            }, false);
            
            xhr.onreadystatechange = function() {
              if (xhr.readyState === 4) {
                logUpload("response received. status: " + xhr.status);
                console.log("[Upload] Response status:", xhr.status, "Total time:", (Date.now() - uploadStartMs) + "ms");
                
                var resp = xhr.responseText;
                logUpload("response text: " + resp.substring(0, 150));
                
                var json = null;
                try {
                  json = JSON.parse(resp);
                } catch(e) {
                  logUpload("JSON parse error: " + e.message);
                }

                uploadBusy = false; // Release lock on any response
                if (xhr.status === 200) {
                  // Strict success validation: ok:true + valid path + size > 0 + valid GIF header
                  var validPath = (json && (json.path === '/boot.gif' || json.path === '/loop.gif'));
                  var validSize = (json && json.size > 0);
                  var validHeader = (json && (json.header === 'GIF87a' || json.header === 'GIF89a'));
                  if (json && json.ok && validPath && validSize && validHeader) {
                    logUpload("SUCCESS: path=" + json.path + " size=" + json.size + " header=" + json.header);
                    
                    var successMsg = "อัปโหลดและประมวลผล GIF สำเร็จ!";
                    if (json.path === '/loop.gif') {
                      successMsg = "อัปโหลดสำเร็จแล้ว หากภาพยังไม่เปลี่ยน ให้รอ 2-3 วินาที หรือกลับไปหน้า Loop/Home";
                    } else {
                      successMsg = "อัปโหลดสำเร็จแล้ว! บอร์ดกำลังเริ่มระบบใหม่เพื่อเปิดใช้งานภาพบูตใหม่...";
                    }
                    
                    // Fetch and log display state for diagnostics
                    fetch("/api/display_gif_state")
                      .then(function(r) { return r.json(); })
                      .then(function(state) {
                        console.log("[Upload Debug] Display State:", state);
                        logUpload("Display State: " + JSON.stringify(state));
                      })
                      .catch(function(e) {
                        console.error("[Upload Debug] Error fetching display state:", e);
                        logUpload("Failed to fetch display state: " + e.message);
                      });
                    
                    // Fetch and log gif file status for diagnostics
                    fetch("/api/gif_file_status")
                      .then(function(r) { return r.json(); })
                      .then(function(status) {
                        console.log("[Upload Debug] GIF File Status:", status);
                        logUpload("GIF File Status: " + JSON.stringify(status));
                      })
                      .catch(function(e) {
                        console.error("[Upload Debug] Error fetching GIF file status:", e);
                        logUpload("Failed to fetch GIF file status: " + e.message);
                      });
                    
                    showSuccessModal(successMsg, function() {
                      location.reload();
                    });
                  } else {
                    var reason = "";
                    if (json && json.ok) {
                      if (!validPath) reason += "path invalid(" + (json.path||'none') + ") ";
                      if (!validSize) reason += "size=0 ";
                      if (!validHeader) reason += "header invalid(" + (json.header||'none') + ") ";
                    }
                    var errorMsg = (json && json.error) ? json.error : "ไฟล์ถูกส่งแต่ไม่ผ่านการตรวจสอบ: " + reason;
                    logUpload("FAIL despite 200: " + errorMsg);
                    showErrorModal(errorMsg);
                  }
                } else if (xhr.status === 409) {
                  logUpload("409 Conflict: upload already in progress on server");
                  showErrorModal("มีการอัปโหลดอื่นอยู่ระหว่างดำเนินการ กรุณารอแล้วลองใหม่");
                } else {
                  var errorMsg = (json && json.error) ? json.error : "ไม่สามารถอัปโหลดไฟล์ได้ (รหัส " + xhr.status + ")";
                  logUpload("HTTP error status " + xhr.status + ": " + errorMsg);
                  showErrorModal(errorMsg);
                }
              }
            };
            
            xhr.open("POST", url, true);
            xhr.send(formData);
          },
          function(errText) {
            if (uploadCompleted) return; // late-callback guard
            uploadCompleted = true;
            clearTimeout(prepTimeout);
            uploadBusy = false; // Release lock on preparation error
            logUpload("Error callback triggered: " + errText);
            console.error("[Upload] Error:", errText);
            showErrorModal(errText);
          }
        );
      }
    };


    function previewGIF(input, previewId) {
      var previewBox = document.getElementById(previewId);
      if (input.files && input.files[0]) {
        var file = input.files[0];
        var sizeKB = (file.size / 1024).toFixed(1);
        var sizeClass = (file.size > MAX_INPUT_MB * 1024 * 1024) ? 'size-warn' : 'size-ok';
        var sizeIcon = (file.size > MAX_INPUT_MB * 1024 * 1024) ? '\u26a0\ufe0f' : '\u2705';
        previewBox.innerHTML =
          '<div class="preview-info">' +
          '\ud83d\udcc4 ' + file.name +
          '<br>' + sizeIcon + ' \u0e02นาด: <span class="' + sizeClass + '">' + sizeKB + ' KB</span> ' +
          (file.size > MAX_INPUT_MB * 1024 * 1024 ? '<span class="size-warn">(\u0e40\u0e01\u0e34\u0e19 ' + MAX_INPUT_MB + 'MB!)</span>' : '\u2192 \u0e08\u0e31\u0e14\u0e01\u0e32\u0e23\u0e22\u0e48\u0e2d\u0e02นาด 320\u00d7172 \u0e43\u0e2b\u0e43\u0e19\u0e40คร\u0e37\u0e48\u0e2dง\u0e40\u0e25\u0e22\u0e17\u0e31น\u0e17\u0e35 \u26a1') +
          '</div>';
        previewBox.style.display = 'block';
      } else {
        previewBox.style.display = 'none';
        previewBox.innerHTML = '';
      }
    }

    function restoreDefaultGIF(type) {
      var modalTitle = "กำลังคืนค่าเริ่มต้น...";
      var modalMsg = "กำลังคัดลอกไฟล์จากบอร์ด...";
      if (type === 'all') {
        modalTitle = "กำลังคืนค่าเริ่มต้นทั้งหมด...";
      }
      showLoadingModal(modalTitle, modalMsg);
      document.getElementById('progress-container').style.display = 'none';
      var xhr = new XMLHttpRequest();
      xhr.onreadystatechange = function() {
        if (xhr.readyState === 4) {
          if (xhr.status === 200) {
            try {
              var resp = JSON.parse(xhr.responseText);
              if (resp.status === "success") {
                if (type === 'loop') {
                  showSuccessModal("คืนค่าเริ่มต้นรูปภาพลูปสำเร็จ!", function() {
                    location.reload();
                  }, "รีเฟรชใน ", 3);
                } else if (type === 'boot') {
                  showSuccessModal("คืนค่าเริ่มต้นภาพเปิดเครื่องสำเร็จ! กำลังรีบูตบอร์ดเพื่อแสดงผล...", function() {
                    location.reload();
                  }, "รีเฟรชใน ", 8);
                } else {
                  showSuccessModal("คืนค่าเริ่มต้นทั้งหมดสำเร็จ! กำลังรีบูตบอร์ดเพื่อแสดงผล...", function() {
                    location.reload();
                  }, "รีเฟรชใน ", 8);
                }
              } else {
                showErrorModal("ไม่สามารถคืนค่าได้: " + resp.message);
              }
            } catch (e) {
              showErrorModal("ไม่สามารถอ่านคำตอบจากเซิร์ฟเวอร์ได้");
            }
          } else {
            try {
              var resp = JSON.parse(xhr.responseText);
              showErrorModal("เกิดข้อผิดพลาด: " + (resp.message || "รหัส " + xhr.status));
            } catch (e) {
              showErrorModal("คืนค่าล้มเหลว (รหัส " + xhr.status + ")");
            }
          }
        }
      };
      
      var url = "/api/restore_default_loop";
      if (type === 'boot') {
        url = "/api/restore_default_boot";
      } else if (type === 'all') {
        url = "/api/restore_default_all";
      }
      
      xhr.open("POST", url, true);
      xhr.send();
    }

    function resetTripEnergy() {
      const btn = document.querySelector("button[onclick='resetTripEnergy()']");
      const status = document.getElementById("energy-reset-status");
      status.innerText = "กำลังรีเซ็ต...";
      status.style.color = "#9ca3af";
      btn.disabled = true;

      fetch("/reset_trip", { method: "POST" })
        .then(res => {
          if (res.ok) {
            status.innerText = "รีเซ็ตสำเร็จ!";
            status.style.color = "#10b981";
            setTimeout(() => { status.innerText = ""; }, 3000);
          } else {
            status.innerText = "รีเซ็ตล้มเหลว";
            status.style.color = "#ef4444";
          }
        })
        .catch(err => {
          status.innerText = "เกิดข้อผิดพลาด";
          status.style.color = "#ef4444";
        })
        .finally(() => {
          btn.disabled = false;
        });
    }

    const slotOptions = {
      EV: [
        { value: 0, text: "12V Battery" },
        { value: 1, text: "Battery Temp" },
        { value: 2, text: "Motor RPM" },
        { value: 3, text: "Motor Temp" },
        { value: 4, text: "Battery SOH" },
        { value: 5, text: "Trip Energy (TRIP kWh (DIS))" },
        { value: 6, text: "Battery SOC" },
        { value: 7, text: "Vehicle Speed" }
      ],
      ICE: [
        { value: 0, text: "12V Battery" },
        { value: 7, text: "Vehicle Speed" }
      ]
    };

    function updateTargetSocDisplay(val) {
      const lbl = document.getElementById("target-soc-val");
      if (lbl) {
        lbl.innerText = val + "%";
      }
    }

    function toggleAutoBackType() {
      const enabledSelect = document.getElementById("autoback-enabled");
      const typeSelect = document.getElementById("autoback-type");
      const targetSocContainer = document.getElementById("target-soc-container");
      if (enabledSelect && typeSelect && targetSocContainer) {
        const isEnabled = enabledSelect.value === "1";
        const isChargeEst = typeSelect.value === "3";
        targetSocContainer.style.display = (isEnabled && isChargeEst) ? "block" : "none";
      }
    }

    function populateSlotDropdowns(type, selectedValues) {
      const options = slotOptions[type] || slotOptions.EV;
      for (let i = 0; i < 6; i++) {
        const select = document.querySelector(`select[name="slot${i}"]`);
        if (!select) continue;
        
        const currentVal = (selectedValues && selectedValues[i] !== undefined) 
          ? selectedValues[i] 
          : parseInt(select.value);
        
        select.innerHTML = "";
        options.forEach(opt => {
          const optionEl = document.createElement("option");
          optionEl.value = opt.value;
          optionEl.text = opt.text;
          if (opt.value === currentVal) {
            optionEl.selected = true;
          }
          select.appendChild(optionEl);
        });
      }
    }

    let isFormDirty = false;
    let isUserEditing = false;

    function updateSaveButtonsState(state) {
      const saveBtn3 = document.getElementById("save-btn-sec3");
      const saveBtn4 = document.getElementById("save-btn-sec4");
      if (!saveBtn3 || !saveBtn4) return;
      if (state === "dirty") {
        saveBtn3.disabled = false;
        saveBtn3.innerText = "💾 บันทึกการตั้งค่า";
        saveBtn3.style.background = ""; // Reverts to stylesheet default gradient
        saveBtn3.style.cursor = "pointer";

        saveBtn4.disabled = false;
        saveBtn4.innerText = "💾 บันทึกการตั้งค่า";
        saveBtn4.style.background = "";
        saveBtn4.style.cursor = "pointer";
      } else if (state === "saving") {
        saveBtn3.disabled = true;
        saveBtn3.innerText = "กำลังบันทึก...";
        saveBtn3.style.background = "#4b5563"; // gray
        saveBtn3.style.cursor = "not-allowed";

        saveBtn4.disabled = true;
        saveBtn4.innerText = "กำลังบันทึก...";
        saveBtn4.style.background = "#4b5563";
        saveBtn4.style.cursor = "not-allowed";
      } else if (state === "saved") {
        saveBtn3.disabled = true;
        saveBtn3.innerText = "บันทึกแล้ว";
        saveBtn3.style.background = "#4b5563";
        saveBtn3.style.cursor = "not-allowed";

        saveBtn4.disabled = true;
        saveBtn4.innerText = "บันทึกแล้ว";
        saveBtn4.style.background = "#4b5563";
        saveBtn4.style.cursor = "not-allowed";
      }
    }

    document.addEventListener("DOMContentLoaded", function() {
      // Safe helper to set text content
      function safeSetText(id, val) {
        const el = document.getElementById(id);
        if (el) el.innerText = val;
      }

      // Safe helper to set select/input value
      function safeSetValue(id, val) {
        const el = document.getElementById(id);
        if (el) el.value = val;
      }

      const initialSocEl = document.getElementById("charge-target-soc");
      if (initialSocEl) updateTargetSocDisplay(initialSocEl.value);
      toggleAutoBackType();

      // Fetch status and configurations from server via AJAX
      fetch("/api/status")
        .then(response => {
          if (!response.ok) throw new Error("Status API returned " + response.status);
          return response.json();
        })
        .then(data => {
          try {
            // Update status labels
            safeSetText("lbl-free-heap", data.free_heap);
            safeSetText("lbl-fs-used", data.fs_used);
            safeSetText("lbl-fs-total", data.fs_total);
            
            // Set Energy source and cost settings
            if (data.energy_source_mode !== undefined) {
              safeSetValue("energy-source-mode", data.energy_source_mode);
            }
            if (data.elec_rate !== undefined) {
              safeSetValue("elec-rate", data.elec_rate);
            }

            // Set Energy comparison inputs
            if (data.car_display_energy_val !== undefined) {
              safeSetValue("car-eng-val", data.car_display_energy_val);
            }
            if (data.car_display_energy_unit !== undefined) {
              safeSetValue("car-eng-unit", data.car_display_energy_unit);
            }
            if (data.car_display_energy_label !== undefined) {
              safeSetValue("car-eng-lbl", data.car_display_energy_label);
            }
            
            // Populate options and set selections for slots (always use EV slot options as default)
            populateSlotDropdowns("EV", data.slots);
            
            // Set AutoBack options
            if (data.autoback_enabled !== undefined) {
              safeSetValue("autoback-enabled", data.autoback_enabled ? "1" : "0");
            }
            if (data.autoback_type !== undefined) {
              safeSetValue("autoback-type", data.autoback_type);
            }

            // Set Charging options
            if (data.charge_target_soc !== undefined) {
              safeSetValue("charge-target-soc", data.charge_target_soc);
              updateTargetSocDisplay(data.charge_target_soc);
            }
            toggleAutoBackType();
          } catch (e) {
            console.error("Error populating data:", e);
            const errBanner = document.getElementById("js-error-banner");
            if (errBanner) errBanner.style.display = "block";
          }
        })
        .catch(err => {
          console.error("Failed to load dashboard status:", err);
          const errBanner = document.getElementById("js-error-banner");
          if (errBanner) errBanner.style.display = "block";
        })
        .finally(() => {
          // Always show Section 4 since layout-section holds forms
          const layoutSec = document.getElementById("layout-section");
          if (layoutSec) {
            layoutSec.style.display = "block";
            layoutSec.classList.add("fade-in");
          }
        });

      function markFormDirty() {
        isFormDirty = true;
        updateSaveButtonsState("dirty");
      }

      // Track whether the user is actively focusing or typing in any input fields across the page
      document.querySelectorAll("input, select, textarea").forEach(el => {
        el.addEventListener("focus", () => { isUserEditing = true; });
        el.addEventListener("blur", () => { isUserEditing = false; });
      });

      document.querySelectorAll("#main-config-form input, #main-config-form select").forEach(el => {
        el.addEventListener("input", () => { markFormDirty(); });
        el.addEventListener("change", () => { markFormDirty(); });
      });

      // Handle touchmove: block background page scroll when modal is active while allowing internal modal-card scrolling
      window.addEventListener("touchmove", function(e) {
        const overlay = document.getElementById('modal-overlay');
        if (overlay && overlay.classList.contains('active')) {
          const target = e.target;
          const insideModalCard = (target instanceof Element) && target.closest('.modal-card');
          if (insideModalCard) {
            return; // Permit internal vertical scrolling of modal-card
          }
          e.preventDefault(); // Block background page scrolling
          return;
        }
        const active = document.activeElement;
        if (active && (active.tagName === "INPUT" || active.tagName === "SELECT" || active.tagName === "TEXTAREA")) {
          active.blur();
        }
      }, { passive: false });

      // Defocus active input when clicking/tapping outside input elements on iOS
      document.addEventListener("click", function(e) {
        const active = document.activeElement;
        if (active && (active.tagName === "INPUT" || active.tagName === "SELECT" || active.tagName === "TEXTAREA")) {
          if (e.target !== active && !e.target.closest("input, select, textarea")) {
            active.blur();
          }
        }
      });

      // Poll live values for energy debug info every 2 seconds
      setInterval(() => {
        if (isUserEditing || isFormDirty) return; // Pause polling when user is focusing or editing fields
        const carProfileIdEl = document.getElementById("car-profile-id");
        const carProfileId = carProfileIdEl ? carProfileIdEl.value : "";
        if (carProfileId === "BYD_DOLPHIN_TH_EV") {
          fetch("/api/status")
            .then(res => {
              if (!res.ok) throw new Error("HTTP error " + res.status);
              return res.json();
            })
            .then(data => {
              try {


              } catch (e) {
                console.error("Error populating interval data:", e);
              }
            })
            .catch(err => {
              console.warn("Polling error:", err);
            });
        }
      }, 2000);



      // Handle configuration save via AJAX with SweetAlert-style modal alert
      const configForm = document.getElementById('main-config-form');
      if (configForm) {
        configForm.addEventListener('submit', function(e) {
          e.preventDefault();
          
          updateSaveButtonsState("saving");
          showLoadingModal("กำลังบันทึกการตั้งค่า...", "กรุณารอสักครู่");
          
          const formData = new URLSearchParams();
          for (const pair of new FormData(configForm)) {
            formData.append(pair[0], pair[1]);
          }
          
          fetch('/save_config', {
            method: 'POST',
            body: formData,
            headers: {
              'Content-Type': 'application/x-www-form-urlencoded'
            }
          })
          .then(response => {
            if (response.ok) {
              return response.text();
            } else {
              throw new Error("HTTP error " + response.status);
            }
          })
          .then(text => {
            if (text.trim() === "OK") {
              isFormDirty = false;
              updateSaveButtonsState("saved");
              
              showSuccessModal(
                "บันทึกสำเร็จ",
                null,
                "",
                0,
                "การตั้งค่าจะมีผลเมื่อออกจากหน้า Setup Portal หรือปิด WiFi ของอุปกรณ์"
              );
            } else {
              throw new Error("เซิร์ฟเวอร์ตอบกลับไม่ถูกต้อง: " + text);
            }
          })
          .catch(err => {
            isFormDirty = true;
            updateSaveButtonsState("dirty");
            showErrorModal("ไม่สามารถบันทึกการตั้งค่าได้: " + err.message);
          });
        });
      }

      // Defocus on page load and force scroll to top
      window.addEventListener("load", function() {
        if (document.activeElement && (document.activeElement.tagName === "INPUT" || document.activeElement.tagName === "SELECT")) {
          document.activeElement.blur();
        }
        window.scrollTo(0, 0);
        setTimeout(function() { window.scrollTo(0, 0); }, 50);
        setTimeout(function() { window.scrollTo(0, 0); }, 150);
      });
    });
  </script>
</body>
</html>
)rawliteral";
