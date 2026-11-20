#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// Bijli-Sync-AI Local Configuration Template
// Copy this file to `config.h` and fill in your credentials before flashing.
// `config.h` is excluded from Git via .gitignore.
// ============================================================================

// Wi-Fi Credentials (Local LAN for mDNS & Optional SMTP Alerts)
#define WIFI_SSID          "YOUR_WIFI_SSID"
#define WIFI_PASSWORD      "YOUR_WIFI_PASSWORD"
#define MDNS_HOSTNAME      "bijlisync"   // Accessible at http://bijlisync.local

// Prepaid Meter Initial State (BDT)
#define INITIAL_BALANCE_BDT       1000.0f
#define LOW_BALANCE_THRESHOLD_BDT  150.0f
#define METER_DEMAND_CHARGE_BDT     42.0f   // Monthly fixed demand charge (1 kW sanctioned)
#define VAT_RATE                    0.05f   // 5% statutory VAT on electricity charges

// SMTP Email Alert Settings (Gmail App Password or Custom SMTP)
#define ENABLE_SMTP_ALERTS   true
#define SMTP_HOST            "smtp.gmail.com"
#define SMTP_PORT            465
#define SMTP_AUTHOR_EMAIL    "your-sender-email@gmail.com"
#define SMTP_AUTHOR_PASSWORD "your-16-char-app-password"
#define ALERT_RECIPIENT      "recipient-email@gmail.com"

// Hardware Pin Definitions (ESP32 DevKit V1)
#define PZEM_RX_PIN          16   // Connects to PZEM-004T TX
#define PZEM_TX_PIN          17   // Connects to PZEM-004T RX
#define SD_CS_PIN             5   // microSD SPI Chip Select
#define OLED_SDA_PIN         21   // I2C SDA (1.3" SH1106 / SSD1306)
#define OLED_SCL_PIN         22   // I2C SCL

// Persistence & Telemetry Intervals
#define SENSOR_POLL_MS       1000UL
#define NVS_COMMIT_DELTA_KWH 0.05f   // Wear-leveling: commit to NVS every 0.05 kWh or 10 min
#define NVS_COMMIT_MAX_MS    600000UL
#define SD_LOG_INTERVAL_MS   60000UL // Log 1-minute aggregated records to microSD

#endif // CONFIG_H
