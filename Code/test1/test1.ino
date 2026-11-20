#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <PZEM004Tv30.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

// --- HARDWARE PINS ---
#define PZEM_RX_PIN 16
#define PZEM_TX_PIN 17
#define SD_CS_PIN 5

// --- OBJECTS ---
PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Preferences pref;
WebServer server(80);
WebSocketsServer webSocket(81);

// --- SYSTEM VARIABLES ---
float current_balance = 0.0;
float monthly_kwh = 0.0;
float last_pzem_kwh = -1.0; 
float daily_history[5] = {0.0, 0.0, 0.0, 0.0, 0.0}; 
int days_left = -1;
unsigned long lastUpdate = 0;

// --- TARIFF LOGIC (DESCO LT-A) ---
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
    input { padding: 10px; font-size: 1rem; border: 1px solid var(--border); border-radius: 6px; width: 200px; outline: none; }
    input:focus { border-color: var(--orange); }
    button { background: var(--orange); color: white; border: none; padding: 10px 20px; font-size: 1rem; border-radius: 6px; cursor: pointer; font-weight: bold; transition: 0.2s; }
    button:hover { background: #ea580c; }
    .btn-reset { background: #ef4444; margin-left: 10px; }
    .btn-reset:hover { background: #dc2626; }

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
    <div class="label" style="margin-bottom:10px;">Secure Meter Management</div>
    <input type="number" id="rechargeAmount" min="1" placeholder="Amount (BDT)">
    <button onclick="sendRecharge()">Recharge</button>
    <button class="btn-reset" onclick="resetMeter()">Reset Meter</button>
  </div>

  <div class="grid">
    <div class="card">
      <div class="top-flex"><div class="label">Power</div><div class="icon">⚡</div></div>
      <div class="value" id="pow">0.0 <span class="unit">W</span></div>
    </div>
    <div class="card">
      <div class="top-flex"><div class="label">Current</div><div class="icon">🔌</div></div>
      <div class="value" id="cur">0.00 <span class="unit">A</span></div>
    </div>
    <div class="card">
      <div class="top-flex"><div class="label">Voltage</div><div class="icon" style="color:var(--blue); background:rgba(59,130,246,0.1);">💡</div></div>
      <div class="value" id="vol">0.0 <span class="unit">V</span></div>
    </div>
    <div class="card">
      <div class="top-flex"><div class="label">Frequency</div><div class="icon" style="color:var(--purple); background:rgba(139,92,246,0.1);">〰️</div></div>
      <div class="value" id="freq">0.0 <span class="unit">Hz</span></div>
    </div>
    <div class="card">
      <div class="top-flex"><div class="label">Power Factor</div><div class="icon">📐</div></div>
      <div class="value" id="pf">0.00</div>
    </div>
    <div class="card">
      <div class="top-flex"><div class="label">Total Energy</div><div class="icon">📊</div></div>
      <div class="value" id="eng">0.00 <span class="unit">kWh</span></div>
    </div>
    <div class="card" style="border-left: 4px solid #f97316;">
      <div class="top-flex"><div class="label">Remaining Balance</div><div class="icon">💰</div></div>
      <div class="value" id="bal" style="color:#f97316;">৳ 0.00</div>
    </div>
    <div class="card" style="border-left: 4px solid #10b981;">
      <div class="top-flex"><div class="label">AI Prediction</div><div class="icon">⏳</div></div>
      <div class="value" id="pred" style="color:#10b981;">Calc...</div>
    </div>
  </div>

  <div class="chart-grid">
    <div class="chart-card">
      <div class="label" style="margin-bottom: 10px;">Power Trend (W)</div>
      <canvas id="lineChart"></canvas>
    </div>
    <div class="chart-card">
      <div class="label" style="margin-bottom: 10px;">Voltage & Frequency Trends</div>
      <canvas id="vfChart"></canvas>
    </div>
    <div class="chart-card">
      <div class="label" style="margin-bottom: 10px;">Weekly Consumption (kWh)</div>
      <canvas id="weeklyBarChart"></canvas>
    </div>
    <div class="chart-card">
      <div class="label" style="margin-bottom: 10px;">Monthly Consumption (kWh)</div>
      <canvas id="monthlyBarChart"></canvas>
    </div>
  </div>

  <script>
    let ws;
    
    // Generate actual dynamic dates for the X-Axis
    let weekDates = [];
    for(let i=6; i>=0; i--) {
        let d = new Date();
        d.setDate(d.getDate() - i);
        weekDates.push(d.toLocaleDateString());
    }
    
    let monthNames = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
    let currentMonth = new Date().getMonth();
    let monthDates = [];
    for(let i=5; i>=0; i--) {
        let index = (currentMonth - i + 12) % 12;
        monthDates.push(monthNames[index]);
    }

    const commonOptions = { responsive: true, maintainAspectRatio: false, animation: false, scales: { y: { beginAtZero: true } } };

    // 1. Power Line Chart
    let lineChart = new Chart(document.getElementById('lineChart').getContext('2d'), {
      type: 'line',
      data: { labels: [], datasets: [{ label: 'Power (W)', data: [], borderColor: '#f97316', backgroundColor: 'rgba(249,115,22,0.1)', fill: true, tension: 0.3 }] },
      options: commonOptions
    });

    // 2. Voltage & Frequency Line Chart
    let vfChart = new Chart(document.getElementById('vfChart').getContext('2d'), {
      type: 'line',
      data: { labels: [], datasets: [
        { label: 'Voltage (V)', data: [], borderColor: '#3b82f6', yAxisID: 'y' },
        { label: 'Frequency (Hz)', data: [], borderColor: '#8b5cf6', yAxisID: 'y1' }
      ]},
      options: {
        responsive: true, maintainAspectRatio: false, animation: false,
        scales: {
          y: { type: 'linear', position: 'left', min: 180, max: 260 },
          y1: { type: 'linear', position: 'right', min: 45, max: 55, grid: { drawOnChartArea: false } }
        }
      }
    });

    // 3. Weekly Bar Chart (Initialized with 0, not dummy data)
    let weeklyBarChart = new Chart(document.getElementById('weeklyBarChart').getContext('2d'), {
      type: 'bar',
      data: { labels: weekDates, datasets: [{ label: 'Energy (kWh)', data: [0,0,0,0,0,0,0], backgroundColor: '#f97316' }] },
      options: commonOptions
    });

    // 4. Monthly Bar Chart (Initialized with 0)
    let monthlyBarChart = new Chart(document.getElementById('monthlyBarChart').getContext('2d'), {
      type: 'bar',
      data: { labels: monthDates, datasets: [{ label: 'Energy (kWh)', data: [0,0,0,0,0,0], backgroundColor: '#10b981' }] },
      options: commonOptions
    });

    function connect() {
      ws = new WebSocket('ws://' + window.location.hostname + ':81');
      ws.onopen = () => { document.getElementById('status').style.display = 'none'; };
      ws.onclose = () => { document.getElementById('status').style.display = 'block'; setTimeout(connect, 2000); };

      ws.onmessage = function(e) {
        let d = JSON.parse(e.data);
        
        // Update Cards
        document.getElementById('vol').innerHTML = d.v + ' <span class="unit">V</span>';
        document.getElementById('cur').innerHTML = d.i + ' <span class="unit">A</span>';
        document.getElementById('pow').innerHTML = d.w + ' <span class="unit">W</span>';
        document.getElementById('eng').innerHTML = d.e + ' <span class="unit">kWh</span>';
        document.getElementById('freq').innerHTML = d.f + ' <span class="unit">Hz</span>';
        document.getElementById('pf').innerText = d.pf;
        document.getElementById('bal').innerText = '৳ ' + d.bal;
        
        if(d.pred === "-1" || d.pred === "0") {
          document.getElementById('pred').innerHTML = (d.bal <= 0) ? '<span style="color:#ef4444">Recharge Req.</span>' : 'Calc...';
        } else {
          document.getElementById('pred').innerHTML = d.pred + ' <span class="unit">Days</span>';
        }

        // Update Time Series Charts
        let time = new Date().toLocaleTimeString();
        if(lineChart.data.labels.length > 15) { 
            lineChart.data.labels.shift(); lineChart.data.datasets[0].data.shift(); 
            vfChart.data.labels.shift(); vfChart.data.datasets[0].data.shift(); vfChart.data.datasets[1].data.shift();
        }
        
        lineChart.data.labels.push(time);
        lineChart.data.datasets[0].data.push(d.w);
        lineChart.update();

        vfChart.data.labels.push(time);
        vfChart.data.datasets[0].data.push(d.v);
        vfChart.data.datasets[1].data.push(d.f);
        vfChart.update();

        // Update Today's Energy Live (Index 6 is today for week, Index 5 is this month)
        let sessionEnergy = parseFloat(d.delta);
        if(sessionEnergy > 0) {
           weeklyBarChart.data.datasets[0].data[6] = sessionEnergy; 
           weeklyBarChart.update();
           monthlyBarChart.data.datasets[0].data[5] = sessionEnergy;
           monthlyBarChart.update();
        }
      };
    }

    function sendRecharge() {
      let amt = parseFloat(document.getElementById('rechargeAmount').value);
      if(isNaN(amt) || amt <= 0) return alert("❌ Valid amount required.");
      if(confirm("⚠️ Recharge ৳ " + amt + " ?")) {
        ws.send("RECHARGE:" + amt); 
        document.getElementById('rechargeAmount').value = '';
      }
    }
    
    function resetMeter() {
      if(confirm("⚠️ DANGER: This resets your balance and stored kWh to zero. Continue?")) {
        ws.send("RESET:ALL");
      }
    }

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
        Serial.println("Recharged: " + String(amount));
      }
    }
    if (msg.startsWith("RESET:ALL")) {
      current_balance = 0.0;
      monthly_kwh = 0.0;
      last_pzem_kwh = pzem.energy(); // Reset baseline to current read
      pref.putFloat("balance", current_balance);
      pref.putFloat("monthly_kwh", monthly_kwh);
      Serial.println("Meter Reset Triggered");
    }
  }
}

// ==========================================
// 🚀 SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Bijli-Sync Start");

  // Load Memory
  pref.begin("meter", false);
  current_balance = pref.getFloat("balance", 0.0);
  monthly_kwh = pref.getFloat("monthly_kwh", 0.0);
  
  // Hardware Setup
  Serial2.begin(9600, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);
  if (!SD.begin(SD_CS_PIN)) { Serial.println("SD Card Failed"); }

  // WiFi Setup (Ensures it reconnects if router drops)
  WiFi.setAutoReconnect(true);
  WiFiManager wm;
  lcd.setCursor(0,1); 
  lcd.print("WiFi Setup...");
  wm.autoConnect("Bijli-Sync-Setup"); 
  
  if (MDNS.begin("bijli")) { Serial.println("mDNS started"); }

  server.on("/", []() { server.send(200, "text/html", getHTML()); });
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("IP: ");
  lcd.print(WiFi.localIP());
}

// ==========================================
// 🔄 MAIN LOOP
// ==========================================
void loop() {
  server.handleClient();
  webSocket.loop();
  
  if (millis() - lastUpdate >= 2000) { 
    
    // Read ALL Metrics
    float v = pzem.voltage();
    float i = pzem.current();
    float p = pzem.power();
    float e = pzem.energy();
    float f = pzem.frequency();
    float pf = pzem.pf();
    float session_delta = 0.0;

    // 🛡️ Safe Delta Calculation
    if (!isnan(e) && e >= 0.0) {
      if (last_pzem_kwh == -1.0) {
        last_pzem_kwh = e; // Establish baseline
      } else if (e > last_pzem_kwh) {
        float delta_kwh = e - last_pzem_kwh;
        session_delta = delta_kwh; // For tracking live session on charts
        
        // Prevent massive fake drops from sensor glitches (Max 50kWh jump allowed at once)
        if (delta_kwh < 50.0) { 
          float cost = delta_kwh * getRate(monthly_kwh);
          
          // Only deduct if we aren't already massively negative
          if(current_balance > -500.0) { 
              current_balance -= cost;
              pref.putFloat("balance", current_balance);
          }
          
          monthly_kwh += delta_kwh;
          pref.putFloat("monthly_kwh", monthly_kwh);
        }
        last_pzem_kwh = e;
      }
    }

    // Predictive Math Fix
    if (current_balance <= 0.0) {
      days_left = 0; // Prevent negative days
    } else {
      float avg_daily_cost = (daily_history[0] + daily_history[1] + daily_history[2] + daily_history[3] + daily_history[4]) / 5.0;
      if (avg_daily_cost > 0.01) { 
        days_left = current_balance / avg_daily_cost; 
      } else {
        days_left = -1; // -1 triggers "Calc..." in Frontend
      }
    }

    // Prepare JSON Payload
    String json = "{";
    json += "\"v\":\"" + String(isnan(v) ? 0 : v, 1) + "\",";
    json += "\"i\":\"" + String(isnan(i) ? 0 : i, 2) + "\",";
    json += "\"w\":\"" + String(isnan(p) ? 0 : p, 1) + "\",";
    json += "\"e\":\"" + String(isnan(e) ? 0 : e, 2) + "\",";
    json += "\"f\":\"" + String(isnan(f) ? 0 : f, 1) + "\",";
    json += "\"pf\":\"" + String(isnan(pf) ? 0 : pf, 2) + "\",";
    json += "\"bal\":\"" + String(current_balance, 2) + "\",";
    json += "\"pred\":\"" + String(days_left) + "\",";
    json += "\"delta\":\"" + String(session_delta, 4) + "\"";
    json += "}";

    webSocket.broadcastTXT(json);
    lastUpdate = millis();
  }
}