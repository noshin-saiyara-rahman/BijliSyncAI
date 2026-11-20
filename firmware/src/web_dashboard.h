#ifndef WEB_DASHBOARD_H
#define WEB_DASHBOARD_H

#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Bijli-Sync-AI | Edge Shadow Meter</title>
  <style>
    :root {
      --bg: #0f172a;
      --card: #1e293b;
      --border: #334155;
      --text: #f8fafc;
      --muted: #94a3b8;
      --accent: #38bdf8;
      --good: #22c55e;
      --warn: #f59e0b;
      --danger: #ef4444;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background: var(--bg); color: var(--text); padding: 20px; max-width: 1080px; margin: 0 auto; }
    header { display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid var(--border); padding-bottom: 16px; margin-bottom: 24px; flex-wrap: wrap; gap: 12px; }
    h1 { font-size: 1.5rem; letter-spacing: -0.02em; }
    .badge { padding: 4px 12px; border-radius: 999px; font-size: 0.8rem; font-weight: 600; background: #0284c7; color: #fff; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 16px; margin-bottom: 24px; }
    .card { background: var(--card); border: 1px solid var(--border); border-radius: 12px; padding: 18px; }
    .label { color: var(--muted); font-size: 0.82rem; text-transform: uppercase; letter-spacing: 0.05em; margin-bottom: 8px; }
    .value { font-size: 1.85rem; font-weight: 700; color: var(--accent); }
    .unit { font-size: 0.95rem; color: var(--muted); font-weight: 400; }
    .sub { margin-top: 6px; font-size: 0.82rem; color: var(--muted); }
    .banner { background: var(--card); border-left: 4px solid var(--good); border-radius: 8px; padding: 16px; margin-bottom: 24px; }
    .banner.warn { border-left-color: var(--warn); }
    .banner.danger { border-left-color: var(--danger); }
    .controls { display: flex; gap: 10px; flex-wrap: wrap; align-items: center; }
    input[type="number"] { background: #0f172a; border: 1px solid var(--border); color: var(--text); padding: 10px 12px; border-radius: 8px; width: 160px; }
    button { background: var(--accent); color: #0f172a; border: none; padding: 10px 16px; border-radius: 8px; font-weight: 600; cursor: pointer; }
    button:hover { opacity: 0.9; }
  </style>
</head>
<body>
  <header>
    <div>
      <h1>Bijli-Sync-AI</h1>
      <div class="sub">Offline-First Edge Shadow Meter &amp; Explainable Grid Monitor</div>
    </div>
    <span id="ws-status" class="badge">Connecting...</span>
  </header>

  <div id="advisory-banner" class="banner">
    <strong id="advisory-title">Grid Status: Nominal</strong>
    <div id="advisory-text" class="sub">Voltage, power factor, and prepaid credit levels are within normal operating thresholds.</div>
  </div>

  <div class="grid">
    <div class="card">
      <div class="label">Prepaid Balance</div>
      <div class="value" id="balance">-- <span class="unit">BDT</span></div>
      <div class="sub" id="tariff-tier">Active Slab: --</div>
    </div>
    <div class="card">
      <div class="label">Active Load</div>
      <div class="value" id="power">-- <span class="unit">W</span></div>
      <div class="sub" id="current">Current: -- A</div>
    </div>
    <div class="card">
      <div class="label">Grid Voltage</div>
      <div class="value" id="voltage">-- <span class="unit">V</span></div>
      <div class="sub" id="freq-pf">Freq: -- Hz | PF: --</div>
    </div>
    <div class="card">
      <div class="label">Monthly Energy</div>
      <div class="value" id="energy">-- <span class="unit">kWh</span></div>
      <div class="sub" id="rate">Marginal Rate: -- BDT/kWh</div>
    </div>
  </div>

  <div class="card">
    <div class="label">Prepaid Top-Up &amp; Cycle Reset</div>
    <div class="controls" style="margin-top: 10px;">
      <input type="number" id="topup-amount" placeholder="Amount (BDT)" min="10" step="10" />
      <button onclick="sendCommand('topup')">Add Recharge</button>
      <button style="background:#334155;color:#f8fafc;" onclick="sendCommand('reset_month')">New Billing Cycle</button>
    </div>
  </div>

  <script>
    let ws;
    function connectWS() {
      ws = new WebSocket(`ws://${window.location.hostname}/ws`);
      ws.onopen = () => {
        const badge = document.getElementById('ws-status');
        badge.textContent = 'Live Telemetry';
        badge.style.background = '#16a34a';
      };
      ws.onclose = () => {
        const badge = document.getElementById('ws-status');
        badge.textContent = 'Reconnecting...';
        badge.style.background = '#dc2626';
        setTimeout(connectWS, 2000);
      };
      ws.onmessage = (event) => {
        const d = JSON.parse(event.data);
        document.getElementById('balance').innerHTML = `${d.balance.toFixed(2)} <span class="unit">BDT</span>`;
        document.getElementById('tariff-tier').textContent = `Active Slab: ${d.slab}`;
        document.getElementById('power').innerHTML = `${d.power.toFixed(1)} <span class="unit">W</span>`;
        document.getElementById('current').textContent = `Current: ${d.current.toFixed(2)} A`;
        document.getElementById('voltage').innerHTML = `${d.voltage.toFixed(1)} <span class="unit">V</span>`;
        document.getElementById('freq-pf').textContent = `Freq: ${d.freq.toFixed(1)} Hz | PF: ${d.pf.toFixed(2)}`;
        document.getElementById('energy').innerHTML = `${d.monthly_kwh.toFixed(3)} <span class="unit">kWh</span>`;
        document.getElementById('rate').textContent = `Marginal Rate: ${d.marginal_rate.toFixed(2)} BDT/kWh`;

        const banner = document.getElementById('advisory-banner');
        const title = document.getElementById('advisory-title');
        const text = document.getElementById('advisory-text');
        banner.className = 'banner';

        if (d.voltage > 0 && d.voltage < 195.0) {
          banner.classList.add('danger');
          title.textContent = 'High Brownout / Blackout Risk Detected';
          text.textContent = `Severe undervoltage (${d.voltage.toFixed(1)} V). Consider shedding high-load inductive appliances (AC/pump).`;
        } else if (d.balance < 150.0) {
          banner.classList.add('warn');
          title.textContent = 'Low Prepaid Credit Warning';
          text.textContent = `Remaining balance is ${d.balance.toFixed(2)} BDT. Recharge soon to prevent automatic relay disconnect.`;
        } else {
          title.textContent = 'Grid Status: Nominal';
          text.textContent = 'Voltage, power factor, and prepaid credit levels are within normal operating thresholds.';
        }
      };
    }

    function sendCommand(action) {
      if (!ws || ws.readyState !== WebSocket.OPEN) return;
      const amt = parseFloat(document.getElementById('topup-amount').value || '0');
      ws.send(JSON.stringify({ action: action, amount: amt }));
      if (action === 'topup') document.getElementById('topup-amount').value = '';
    }

    window.onload = connectWS;
  </script>
</body>
</html>
)rawliteral";

#endif // WEB_DASHBOARD_H
