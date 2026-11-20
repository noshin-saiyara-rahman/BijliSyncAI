#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <PZEM004Tv30.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <ESP_Mail_Client.h>
#include "time.h"

// --- HARDWARE PINS ---
#define PZEM_RX_PIN 16
#define PZEM_TX_PIN 17
#define SD_CS_PIN 5
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// --- SMTP EMAIL SETTINGS ---
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define AUTHOR_EMAIL "bijlisyncai@gmail.com"
#define AUTHOR_PASSWORD "shcz mwvn fxwb niug"

// --- OBJECTS ---
PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Preferences pref;
WebServer server(80);
WebSocketsServer webSocket(81);
WiFiManager wm;
SMTPSession smtp;

// --- SYSTEM VARIABLES ---
float current_balance = 0.0;
float monthly_kwh = 0.0;
float daily_kwh = 0.0; 
float last_pzem_kwh = -1.0; 
float daily_history[5] = {0.0, 0.0, 0.0, 0.0, 0.0}; 
int days_left = -1;
unsigned long lastUpdate = 0;
String user_email = "";
bool email_sent = false;
int last_day = -1; 

// --- NTP SETTINGS ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 21600; 
const int   daylightOffset_sec = 0;

// --- TARIFF LOGIC ---
double getRate(double units) {
  if (units <= 50) return 4.63;
  if (units <= 75) return 4.80;
  if (units <= 200) return 5.45;
  if (units <= 300) return 5.70;
  if (units <= 400) return 6.02;
  if (units <= 600) return 9.30;
  return 10.70;
}

// ==========================================
// 💾 SD CARD LOGGING LOGIC
// ==========================================
void logToSD(String filename, String data) {
  File file = SD.open(filename, FILE_APPEND);
  if (file) {
    file.println(data);
    file.close();
    Serial.println("Logged to SD: " + filename);
  } else {
    Serial.println("SD Write Failed");
  }
}

void checkMidnight() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return;

  if (timeinfo.tm_mday != last_day) {
    char dateBuf[20];
    strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &timeinfo);
    
    String logEntry = String(dateBuf) + "," + String(daily_kwh, 3);
    logToSD("/weekly_usage.csv", logEntry);

    // This resets the tariff bracket to 0 every 1st of the month!
    if (timeinfo.tm_mday == 1) {
       logToSD("/monthly_usage.csv", String(dateBuf) + "," + String(monthly_kwh, 2));
       monthly_kwh = 0.0; 
       pref.putFloat("monthly_kwh", 0.0);
    }

    daily_kwh = 0.0; 
    last_day = timeinfo.tm_mday;
  }
}

// ==========================================
// 📧 FUNCTION TO SEND LOW BALANCE ALERT
// ==========================================
void sendAlertEmail() {
  if (user_email == "" || email_sent) return;

  Serial.println("Attempting to send low balance email...");
  
  Session_Config config;
  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;
  config.login.user_domain = "bijlisync.local";

  SMTP_Message message;
  message.sender.name = "Bijli-Sync-Ai";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "⚠️ Alert: Low Smart Meter Balance";
  message.addRecipient("User", user_email);

  String htmlMsg = "<div style=\"font-family:Arial; color:#333;\"><h2>⚡ Bijli-Sync-Ai Alert</h2>";
  htmlMsg += "<p>Your smart meter balance has dropped to <b><span style=\"color:#ef4444;\">৳ " + String(current_balance, 2) + "</span></b>.</p>";
  htmlMsg += "<p>Please recharge soon to ensure continuous power supply.</p></div>";
  message.html.content = htmlMsg.c_str();

  if (MailClient.sendMail(&smtp, &message)) {
    Serial.println("✅ Email sent successfully");
    email_sent = true;
  } else {
    Serial.println("❌ Error sending email: " + smtp.errorReason());
  }
}

// ==========================================
// 🌐 BIJLI-SYNC-AI DASHBOARD (HTML/JS)
// ==========================================
String getHTML() {
  String html = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Bijli-Sync-Ai Dashboard</title>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/3.9.1/chart.min.js"></script>
  <style>
    :root {
      --bg: #f3f4f6;
      --card-bg: #ffffff;
      --text-main: #1f2937;
      --text-muted: #6b7280;
      --orange: #f97316;
      --blue: #3b82f6;
      --green: #10b981;
      --purple: #8b5cf6;
      --border: #e5e7eb;
    }
    body { background: var(--bg); color: var(--text-main); font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; padding: 20px; margin: 0; }
    h2 { color: var(--text-main); margin-bottom: 20px; text-align: center; }
    
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; margin-bottom: 20px; }
    .card { background: var(--card-bg); padding: 20px; border-radius: 12px; border: 1px solid var(--border); box-shadow: 0 1px 3px rgba(0,0,0,0.05); }
    
    .top-flex { display: flex; align-items: center; justify-content: space-between; }
    .icon { background: rgba(249, 115, 22, 0.1); color: var(--orange); padding: 8px; border-radius: 50%; font-size: 1.2rem; }
    .value { font-size: 1.8rem; font-weight: bold; margin-top: 10px; color: var(--text-main); }
    .unit { font-size: 0.9rem; color: var(--text-muted); font-weight: normal; }
    .label { color: var(--text-muted); font-size: 0.9rem; font-weight: 500; }

    .recharge-card { background: var(--card-bg); padding: 20px; border-radius: 12px; border: 1px solid var(--border); text-align: center; margin-bottom: 20px; }
    input { padding: 10px; font-size: 1rem; border: 1px solid var(--border); border-radius: 6px; width: 220px; outline: none; }
    input:focus { border-color: var(--orange); }
    button { background: var(--orange); color: white; border: none; padding: 10px 20px; font-size: 1rem; border-radius: 6px; cursor: pointer; font-weight: bold; transition: 0.2s; }
    button:hover { opacity: 0.9; }
    .btn-blue { background: var(--blue); }
    .btn-green { background: var(--green); }
    .btn-reset { background: #ef4444; }
    .btn-gray { background: #6b7280; }
    
    .flex-row { display: flex; gap: 10px; justify-content: center; flex-wrap: wrap; margin-bottom: 15px; }

    .chart-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(400px, 1fr)); gap: 15px; }
    .chart-card { background: var(--card-bg); padding: 20px; border-radius: 12px; border: 1px solid var(--border); }
    canvas { width: 100% !important; max-height: 250px; }
    
    #status { position: fixed; top: 10px; right: 10px; background: #ef4444; color: white; padding: 5px 10px; border-radius: 20px; font-size: 0.8rem; display: none; }
  </style>
</head>
<body>
  <div id="status">Disconnected - Reconnecting...</div>
  <h2>⚡ Bijli-Sync-Ai Dashboard</h2>
  
  <div class="recharge-card">
    <div class="label" style="margin-bottom:15px; font-size:1.1rem; color:#1f2937;">Secure Meter Management</div>
    
    <div class="flex-row">
      <input type="email" id="userEmail" placeholder="Enter Alert Email (e.g. you@mail.com)">
      <button class="btn-blue" onclick="saveEmail()">Save Email</button>
      <button class="btn-green" onclick="window.location.href='/download_weekly'">⬇️ Weekly Log</button>
      <button class="btn-green" style="background:#059669;" onclick="window.location.href='/download_monthly'">⬇️ Monthly Log</button>
    </div>

    <div class="flex-row">
      <input type="number" id="rechargeAmount" min="1" placeholder="Amount (BDT)">
      <button onclick="sendRecharge()">Recharge</button>
      <button class="btn-reset" onclick="resetMeter()">Reset Meter</button>
      <button class="btn-gray" onclick="resetWiFi()">Reset Wi-Fi</button>
    </div>
  </div>

  <div class="grid">
    <div class="card"><div class="top-flex"><div class="label">Power</div><div class="icon">⚡</div></div><div class="value" id="pow">0.0 <span class="unit">W</span></div></div>
    <div class="card"><div class="top-flex"><div class="label">Current</div><div class="icon">🔌</div></div><div class="value" id="cur">0.00 <span class="unit">A</span></div></div>
    <div class="card"><div class="top-flex"><div class="label">Voltage</div><div class="icon" style="color:var(--blue); background:rgba(59,130,246,0.1);">💡</div></div><div class="value" id="vol">0.0 <span class="unit">V</span></div></div>
    <div class="card"><div class="top-flex"><div class="label">Frequency</div><div class="icon" style="color:var(--purple); background:rgba(139,92,246,0.1);">〰️</div></div><div class="value" id="freq">0.0 <span class="unit">Hz</span></div></div>
    <div class="card"><div class="top-flex"><div class="label">Power Factor</div><div class="icon">📐</div></div><div class="value" id="pf">0.00</div></div>
    <div class="card"><div class="top-flex"><div class="label">Total Energy</div><div class="icon">📊</div></div><div class="value" id="eng">0.00 <span class="unit">kWh</span></div></div>
    <div class="card" style="border-left: 4px solid #f97316;"><div class="top-flex"><div class="label">Remaining Balance</div><div class="icon">💰</div></div><div class="value" id="bal" style="color:#f97316;">৳ 0.00</div></div>
    <div class="card" style="border-left: 4px solid #10b981;"><div class="top-flex"><div class="label">AI Prediction</div><div class="icon">⏳</div></div><div class="value" id="pred" style="color:#10b981;">Calc...</div></div>
  </div>

  <div class="chart-grid">
    <div class="chart-card"><div class="label" style="margin-bottom: 10px;">Power Trend (W)</div><canvas id="lineChart"></canvas></div>
    <div class="chart-card"><div class="label" style="margin-bottom: 10px;">Voltage & Frequency Trends</div><canvas id="vfChart"></canvas></div>
    <div class="chart-card"><div class="label" style="margin-bottom: 10px;">Weekly Consumption (kWh)</div><canvas id="weeklyBarChart"></canvas></div>
    <div class="chart-card"><div class="label" style="margin-bottom: 10px;">Monthly Consumption (kWh)</div><canvas id="monthlyBarChart"></canvas></div>
  </div>

  <script>
    let ws;
    let weekDates = [];
    for(let i=6; i>=0; i--) { let d = new Date(); d.setDate(d.getDate() - i); weekDates.push(d.toLocaleDateString()); }
    
    let monthNames = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
    let currentMonth = new Date().getMonth();
    let monthDates = [];
    for(let i=5; i>=0; i--) { let index = (currentMonth - i + 12) % 12; monthDates.push(monthNames[index]); }

    const commonOptions = { responsive: true, maintainAspectRatio: false, animation: false, scales: { y: { beginAtZero: true } } };

    let lineChart = new Chart(document.getElementById('lineChart').getContext('2d'), { type: 'line', data: { labels: [], datasets: [{ label: 'Power (W)', data: [], borderColor: '#f97316', backgroundColor: 'rgba(249,115,22,0.1)', fill: true, tension: 0.3 }] }, options: commonOptions });
    let vfChart = new Chart(document.getElementById('vfChart').getContext('2d'), { type: 'line', data: { labels: [], datasets: [ { label: 'Voltage (V)', data: [], borderColor: '#3b82f6', yAxisID: 'y' }, { label: 'Frequency (Hz)', data: [], borderColor: '#8b5cf6', yAxisID: 'y1' } ]}, options: { responsive: true, maintainAspectRatio: false, animation: false, scales: { y: { type: 'linear', position: 'left', min: 180, max: 260 }, y1: { type: 'linear', position: 'right', min: 45, max: 55, grid: { drawOnChartArea: false } } } } });
    let weeklyBarChart = new Chart(document.getElementById('weeklyBarChart').getContext('2d'), { type: 'bar', data: { labels: weekDates, datasets: [{ label: 'Energy (kWh)', data: [0,0,0,0,0,0,0], backgroundColor: '#f97316' }] }, options: commonOptions });
    let monthlyBarChart = new Chart(document.getElementById('monthlyBarChart').getContext('2d'), { type: 'bar', data: { labels: monthDates, datasets: [{ label: 'Energy (kWh)', data: [0,0,0,0,0,0], backgroundColor: '#10b981' }] }, options: commonOptions });

    function connect() {
      ws = new WebSocket('ws://' + window.location.hostname + ':81');
      ws.onopen = () => { document.getElementById('status').style.display = 'none'; };
      ws.onclose = () => { document.getElementById('status').style.display = 'block'; setTimeout(connect, 2000); };

      ws.onmessage = function(e) {
        let d = JSON.parse(e.data);
        if (d.mail && document.getElementById('userEmail').value === "") document.getElementById('userEmail').value = d.mail;

        document.getElementById('vol').innerHTML = d.v + ' <span class="unit">V</span>';
        document.getElementById('cur').innerHTML = d.i + ' <span class="unit">A</span>';
        document.getElementById('pow').innerHTML = d.w + ' <span class="unit">W</span>';
        document.getElementById('eng').innerHTML = d.e + ' <span class="unit">kWh</span>';
        document.getElementById('freq').innerHTML = d.f + ' <span class="unit">Hz</span>';
        document.getElementById('pf').innerText = d.pf;
        document.getElementById('bal').innerText = '৳ ' + d.bal;
        document.getElementById('pred').innerHTML = (d.pred === "-1" || d.pred === "0") ? ((d.bal <= 0) ? '<span style="color:#ef4444">Recharge Req.</span>' : 'Calc...') : d.pred + ' <span class="unit">Days</span>';

        let time = new Date().toLocaleTimeString();
        if(lineChart.data.labels.length > 15) { lineChart.data.labels.shift(); lineChart.data.datasets[0].data.shift(); vfChart.data.labels.shift(); vfChart.data.datasets[0].data.shift(); vfChart.data.datasets[1].data.shift(); }
        
        lineChart.data.labels.push(time); lineChart.data.datasets[0].data.push(d.w); lineChart.update();
        vfChart.data.labels.push(time); vfChart.data.datasets[0].data.push(d.v); vfChart.data.datasets[1].data.push(d.f); vfChart.update();

        let sessionEnergy = parseFloat(d.delta);
        if(sessionEnergy > 0) {
           weeklyBarChart.data.datasets[0].data[6] = sessionEnergy; weeklyBarChart.update();
           monthlyBarChart.data.datasets[0].data[5] = sessionEnergy; monthlyBarChart.update();
        }
      };
    }

    function saveEmail() {
      let em = document.getElementById('userEmail').value;
      if(em.includes("@") && em.includes(".")) { ws.send("EMAIL:" + em); alert("✅ Alert Email Saved!"); } 
      else { alert("❌ Please enter a valid email."); }
    }

    function sendRecharge() {
      let amt = parseFloat(document.getElementById('rechargeAmount').value);
      if(isNaN(amt) || amt <= 0) return alert("❌ Valid amount required.");
      if(confirm("⚠️ Recharge ৳ " + amt + " ?")) { ws.send("RECHARGE:" + amt); document.getElementById('rechargeAmount').value = ''; }
    }
    
    function resetMeter() { if(confirm("⚠️ DANGER: Reset balance/kWh to zero?")) ws.send("RESET:ALL"); }
    function resetWiFi() { if(confirm("⚠️ Reboot to Setup Mode?")) ws.send("WIFI_RESET"); }

    connect();
  </script>
</body>
</html>
  )=====";
  return html;
}

// ==========================================
// 📡 WEBSOCKET HANDLER
// ==========================================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    String msg = String((char*)payload);
    
    if (msg.startsWith("RECHARGE:")) {
      float amount = msg.substring(9).toFloat();
      if(amount > 0) {
        current_balance += amount;
        pref.putFloat("balance", current_balance); 
        email_sent = false; 
      }
    }
    if (msg.startsWith("EMAIL:")) { user_email = msg.substring(6); pref.putString("email", user_email); }
    if (msg.startsWith("RESET:ALL")) {
      current_balance = 0.0; monthly_kwh = 0.0; last_pzem_kwh = pzem.energy(); 
      pref.putFloat("balance", current_balance); pref.putFloat("monthly_kwh", monthly_kwh);
    }
    if (msg.startsWith("WIFI_RESET")) { wm.resetSettings(); delay(1000); ESP.restart(); }
  }
}

// ==========================================
// 🚀 SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  // --- OLED START ---
  if(!display.begin(0x3C, true)) {
    Serial.println("SH1106 OLED failed");
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println("Bijli-Sync AI");
  display.println("Starting...");
  display.display();

  pref.begin("meter", false);
  current_balance = pref.getFloat("balance", 0.0);
  monthly_kwh = pref.getFloat("monthly_kwh", 0.0);
  user_email = pref.getString("email", ""); 
  
  Serial2.begin(9600, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);
  if (!SD.begin(SD_CS_PIN)) { 
    display.println("SD Fail!"); 
    display.display(); 
  } else {
    if(!SD.exists("/weekly_usage.csv")) logToSD("/weekly_usage.csv", "Date,kWh_Used");
    if(!SD.exists("/monthly_usage.csv")) logToSD("/monthly_usage.csv", "Month_End,Total_kWh");
  }

  WiFi.setAutoReconnect(true);
  wm.setConnectTimeout(60); 
  display.println("WiFi Setup...");
  display.display();
  wm.autoConnect("Bijli-Sync-Setup"); 
  
  if (MDNS.begin("bijli")) { Serial.println("mDNS started"); }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  MailClient.networkReconnect(true);
  smtp.debug(0); 

  server.on("/", []() { server.send(200, "text/html", getHTML()); });
  
  // NEW: Two distinct routes to download from SD card directly!
  server.on("/download_weekly", []() {
    File f = SD.open("/weekly_usage.csv");
    server.streamFile(f, "text/csv");
    f.close();
  });
  server.on("/download_monthly", []() {
    File f = SD.open("/monthly_usage.csv");
    server.streamFile(f, "text/csv");
    f.close();
  });
  
  server.begin(); webSocket.begin(); webSocket.onEvent(webSocketEvent);
  
  display.clearDisplay(); 
  display.setCursor(0,0); 
  display.println("Connected!"); 
  display.println(WiFi.localIP().toString());
  display.display();
}

// ==========================================
// 🔄 MAIN LOOP (5-SECOND POLLING)
// ==========================================
void loop() {
  server.handleClient();
  webSocket.loop();
  checkMidnight();

  if (current_balance <= 50.0 && !email_sent) sendAlertEmail();
  else if (current_balance > 50.0) email_sent = false;

  if (millis() - lastUpdate >= 5000) { 
    
    float v = pzem.voltage();
    float i = pzem.current();
    float p = pzem.power();
    float e = pzem.energy();
    float f = pzem.frequency();
    
    // FIX: Force frequency to 0 if power is out
    if (v < 10.0 || isnan(v)) f = 0.0; 

    float pf = pzem.pf();
    float session_delta = 0.0;

    // --- SENSOR GLITCH FILTER ---
    float display_energy = e; 
    
    if (isnan(e) || (last_pzem_kwh > 0 && e > (last_pzem_kwh + 50.0))) { 
      if(last_pzem_kwh != -1.0) display_energy = last_pzem_kwh; else display_energy = 0.0;
    } else if (e >= 0.0) {
      if (last_pzem_kwh == -1.0) last_pzem_kwh = e; 
      else if (e > last_pzem_kwh) {
        float delta_kwh = e - last_pzem_kwh;
        session_delta = delta_kwh; 
        
        float cost = delta_kwh * getRate(monthly_kwh);
        if(current_balance > -500.0) { 
            current_balance -= cost;
            pref.putFloat("balance", current_balance);
        }
        monthly_kwh += delta_kwh;
        daily_kwh += delta_kwh;
        pref.putFloat("monthly_kwh", monthly_kwh);
        last_pzem_kwh = e;
      }
    }

    // --- Prediction Math ---
    if (current_balance <= 0.0) days_left = 0; 
    else {
      float avg = (daily_history[0]+daily_history[1]+daily_history[2]+daily_history[3]+daily_history[4])/5.0;
      days_left = (avg > 0.01) ? (current_balance / avg) : -1; 
    }

// --- Update OLED ---
    display.clearDisplay();
    
    // 1. Header (Always stays at the top)
    display.setCursor(25, 0); 
    display.print("Bijli-Sync-Ai");
    display.drawLine(0, 10, 128, 10, SH110X_WHITE); 

    // 2. The Paging Logic
    static bool showPage2 = false; // This remembers which page we are on
    display.setCursor(0, 15);

    if (!showPage2) {
        // ================= PAGE 1 =================
        display.printf("V: %.1fV   I: %.2fA\n\n", v, i); 
        display.printf("P: %.1fW   Bal: %.1f\n\n", p, current_balance);
        display.printf("Mon: %.2f kWh", monthly_kwh);
    } else {
        // ================= PAGE 2 =================
        display.printf("Freq: %.1fHz  PF: %.2f\n\n", f, pf);
        display.printf("Total: %.2f kWh\n\n", e);
        
        if (days_left >= 0) {
            display.printf("Pred: %d Days Left", days_left);
        } else {
            display.printf("Pred: Calc...");
        }
    }

    // Toggle the page so it switches on the next 5-second loop
    showPage2 = !showPage2;

    // 3. Footer (Always stays at the bottom)
    display.setCursor(0, 56);
    display.printf("IP:%s", WiFi.localIP().toString().c_str());
    
    display.display();

    // --- Full JSON Payload ---
    String json = "{";
    json += "\"v\":\"" + String(isnan(v) ? 0 : v, 1) + "\",";
    json += "\"i\":\"" + String(isnan(i) ? 0 : i, 2) + "\",";
    json += "\"w\":\"" + String(isnan(p) ? 0 : p, 1) + "\",";
    json += "\"e\":\"" + String(display_energy, 2) + "\",";
    json += "\"f\":\"" + String(isnan(f) ? 0 : f, 1) + "\",";
    json += "\"pf\":\"" + String(isnan(pf) ? 0 : pf, 2) + "\",";
    json += "\"bal\":\"" + String(current_balance, 2) + "\",";
    json += "\"pred\":\"" + String(days_left) + "\",";
    json += "\"delta\":\"" + String(session_delta, 4) + "\",";
    json += "\"mail\":\"" + user_email + "\""; 
    json += "}";

    webSocket.broadcastTXT(json);
    
    lastUpdate = millis();
  }
}