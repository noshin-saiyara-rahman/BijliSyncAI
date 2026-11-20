#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <PZEM004Tv30.h>
#include <ESP_Mail_Client.h>

#if __has_include("config.h")
  #include "config.h"
#else
  #include "config.example.h"
#endif

#include "web_dashboard.h"

// Hardware Instances
PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Preferences prefs;
SMTPSession smtp;

// Persistent Billing & Telemetry State
struct MeterState {
  float lastPzemKwh;
  float monthlyKwh;
  float balanceBdt;
  bool lowBalanceAlertSent;
} state;

struct LiveTelemetry {
  float voltage;
  float current;
  float power;
  float energyTotal;
  float frequency;
  float pf;
  float marginalRate;
  String activeSlab;
} live;

unsigned long lastPollMs = 0;
unsigned long lastNvsCommitMs = 0;
unsigned long lastSdLogMs = 0;
float lastCommittedKwh = 0.0f;
bool sdAvailable = false;

// ============================================================================
// DESCO 7-Tier Residential Progressive Tariff Calculator (BDT/kWh)
// ============================================================================
float getMarginalTariffBdt(float monthlyKwh, String &slabLabel) {
  if (monthlyKwh <= 50.0f) {
    slabLabel = "Lifeline (0-50 kWh)";
    return 4.63f;
  } else if (monthlyKwh <= 75.0f) {
    slabLabel = "Tier 1 (0-75 kWh)";
    return 5.26f;
  } else if (monthlyKwh <= 200.0f) {
    slabLabel = "Tier 2 (76-200 kWh)";
    return 7.20f;
  } else if (monthlyKwh <= 300.0f) {
    slabLabel = "Tier 3 (201-300 kWh)";
    return 7.59f;
  } else if (monthlyKwh <= 400.0f) {
    slabLabel = "Tier 4 (301-400 kWh)";
    return 8.02f;
  } else if (monthlyKwh <= 600.0f) {
    slabLabel = "Tier 5 (401-600 kWh)";
    return 12.67f;
  } else {
    slabLabel = "Tier 6 (600+ kWh)";
    return 14.61f;
  }
}

// ============================================================================
// Wear-Leveled NVS Flash Persistence
// ============================================================================
void loadStateFromNVS() {
  prefs.begin("bijlisync", true);
  state.lastPzemKwh = prefs.getFloat("last_pzem", -1.0f);
  state.monthlyKwh = prefs.getFloat("month_kwh", 0.0f);
  state.balanceBdt = prefs.getFloat("balance", INITIAL_BALANCE_BDT);
  state.lowBalanceAlertSent = prefs.getBool("alert_sent", false);
  prefs.end();
  lastCommittedKwh = state.monthlyKwh;
}

void saveStateToNVS(bool force = false) {
  unsigned long now = millis();
  float deltaKwh = fabs(state.monthlyKwh - lastCommittedKwh);
  if (!force && deltaKwh < NVS_COMMIT_DELTA_KWH && (now - lastNvsCommitMs) < NVS_COMMIT_MAX_MS) {
    return;
  }
  prefs.begin("bijlisync", false);
  prefs.putFloat("last_pzem", state.lastPzemKwh);
  prefs.putFloat("month_kwh", state.monthlyKwh);
  prefs.putFloat("balance", state.balanceBdt);
  prefs.putBool("alert_sent", state.lowBalanceAlertSent);
  prefs.end();

  lastCommittedKwh = state.monthlyKwh;
  lastNvsCommitMs = now;
}

// ============================================================================
// microSD Telemetry Logging (CSV Schema aligned with AI Pipeline)
// ============================================================================
void logTelemetryToSD() {
  if (!sdAvailable) return;
  bool writeHeader = !SD.exists("/telemetry.csv");
  File file = SD.open("/telemetry.csv", FILE_APPEND);
  if (!file) return;

  if (writeHeader) {
    file.println("uptime_sec,voltage_v,current_a,power_w,frequency_hz,power_factor,monthly_kwh,balance_bdt,marginal_rate_bdt");
  }
  file.printf("%lu,%.2f,%.3f,%.2f,%.2f,%.3f,%.4f,%.2f,%.2f\n",
              millis() / 1000UL,
              live.voltage,
              live.current,
              live.power,
              live.frequency,
              live.pf,
              state.monthlyKwh,
              state.balanceBdt,
              live.marginalRate);
  file.close();
}

// ============================================================================
// SMTP Low-Balance Email Notification
// ============================================================================
void sendLowBalanceEmail() {
#if ENABLE_SMTP_ALERTS
  if (WiFi.status() != WL_CONNECTED) return;

  Session_Config config;
  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = SMTP_AUTHOR_EMAIL;
  config.login.password = SMTP_AUTHOR_PASSWORD;

  SMTP_Message message;
  message.sender.name = "Bijli-Sync-AI Shadow Meter";
  message.sender.email = SMTP_AUTHOR_EMAIL;
  message.subject = "Alert: Low Prepaid Electricity Balance";
  message.addRecipient("Homeowner", ALERT_RECIPIENT);

  String body = "Your remaining prepaid electricity balance has dropped to " +
                String(state.balanceBdt, 2) + " BDT (Monthly usage: " +
                String(state.monthlyKwh, 2) + " kWh, Active rate: " +
                String(live.marginalRate, 2) + " BDT/kWh).\n" +
                "Please recharge soon to avoid automatic disconnection.";
  message.text.content = body.c_str();

  if (smtp.connect(&config)) {
    MailClient.sendMail(&smtp, &message);
    smtp.closeSession();
  }
#endif
}

// ============================================================================
// OLED Display Rendering (128x64 SH1106)
// ============================================================================
void renderOLED() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 10, "BIJLI-SYNC-AI");
  u8g2.drawHLine(0, 13, 128);

  char buf[32];
  snprintf(buf, sizeof(buf), "Bal : %.2f BDT", state.balanceBdt);
  u8g2.drawStr(0, 26, buf);

  snprintf(buf, sizeof(buf), "Load: %.1f W (%.2f A)", live.power, live.current);
  u8g2.drawStr(0, 39, buf);

  snprintf(buf, sizeof(buf), "Grid: %.1f V | PF %.2f", live.voltage, live.pf);
  u8g2.drawStr(0, 51, buf);

  snprintf(buf, sizeof(buf), "Used: %.2f kWh", state.monthlyKwh);
  u8g2.drawStr(0, 63, buf);

  u8g2.sendBuffer();
}

// ============================================================================
// WebSocket Broadcast & Client Command Handler
// ============================================================================
void broadcastTelemetry() {
  JsonDocument doc;
  doc["voltage"] = live.voltage;
  doc["current"] = live.current;
  doc["power"] = live.power;
  doc["freq"] = live.frequency;
  doc["pf"] = live.pf;
  doc["monthly_kwh"] = state.monthlyKwh;
  doc["balance"] = state.balanceBdt;
  doc["marginal_rate"] = live.marginalRate;
  doc["slab"] = live.activeSlab;

  String json;
  serializeJson(doc, json);
  ws.textAll(json);
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len) == DeserializationError::Ok) {
      String action = doc["action"] | "";
      if (action == "topup") {
        float amount = doc["amount"] | 0.0f;
        if (amount > 0.0f) {
          state.balanceBdt += amount;
          if (state.balanceBdt > LOW_BALANCE_THRESHOLD_BDT) {
            state.lowBalanceAlertSent = false;
          }
          saveStateToNVS(true);
        }
      } else if (action == "reset_month") {
        state.monthlyKwh = 0.0f;
        state.balanceBdt -= (METER_DEMAND_CHARGE_BDT * (1.0f + VAT_RATE));
        saveStateToNVS(true);
      }
    }
  }
}

// ============================================================================
// Setup & Main Execution Loop
// ============================================================================
void setup() {
  Serial.begin(115200);
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  u8g2.begin();

  loadStateFromNVS();

  if (SD.begin(SD_CS_PIN)) {
    sdAvailable = true;
    Serial.println("[SD] microSD telemetry logging initialized.");
  } else {
    Serial.println("[SD] Card not detected; running in NVS-only persistence mode.");
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - wifiStart) < 10000) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED && MDNS.begin(MDNS_HOSTNAME)) {
    Serial.printf("[mDNS] Dashboard available at http://%s.local\n", MDNS_HOSTNAME);
  }

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", INDEX_HTML);
  });
  server.begin();
}

void loop() {
  ws.cleanupClients();
  unsigned long now = millis();

  if (now - lastPollMs >= SENSOR_POLL_MS) {
    lastPollMs = now;

    float v = pzem.voltage();
    float i = pzem.current();
    float p = pzem.power();
    float e = pzem.energy();
    float f = pzem.frequency();
    float pf = pzem.pf();

    if (!isnan(v) && !isnan(e)) {
      live.voltage = v;
      live.current = i;
      live.power = p;
      live.frequency = f;
      live.pf = pf;

      if (state.lastPzemKwh < 0.0f) {
        state.lastPzemKwh = e;
      }

      float deltaKwh = e - state.lastPzemKwh;
      if (deltaKwh > 0.0f && deltaKwh < 5.0f) {
        state.monthlyKwh += deltaKwh;
        live.marginalRate = getMarginalTariffBdt(state.monthlyKwh, live.activeSlab);
        float costBdt = deltaKwh * live.marginalRate * (1.0f + VAT_RATE);
        state.balanceBdt -= costBdt;
      }
      state.lastPzemKwh = e;
    } else {
      live.marginalRate = getMarginalTariffBdt(state.monthlyKwh, live.activeSlab);
    }

    if (state.balanceBdt <= LOW_BALANCE_THRESHOLD_BDT && !state.lowBalanceAlertSent) {
      sendLowBalanceEmail();
      state.lowBalanceAlertSent = true;
      saveStateToNVS(true);
    }

    saveStateToNVS(false);
    renderOLED();
    broadcastTelemetry();
  }

  if (now - lastSdLogMs >= SD_LOG_INTERVAL_MS) {
    lastSdLogMs = now;
    logTelemetryToSD();
  }
}
