#include <WiFi.h>
#include <WiFiManager.h>  // Added WiFiManager
#include <SD.h>
#include <SPI.h>
#include <ESP_Mail_Client.h>

// --- HARDWARE PINS ---
#define SD_CS_PIN 5

// --- SMTP EMAIL SETTINGS ---
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define AUTHOR_EMAIL "bijlisyncai@gmail.com"
#define AUTHOR_PASSWORD "shcz mwvn fxwb niug"
#define RECIPIENT_EMAIL "ifftihasan794@gmail.com" 

// --- OBJECTS ---
SMTPSession smtp;
WiFiManager wm;

// ==========================================
// 💾 SD CARD TEST FUNCTIONS
// ==========================================
void writeToSD(String filename, String data) {
  File file = SD.open(filename, FILE_APPEND);
  if (file) {
    file.println(data);
    file.close();
    Serial.println("✅ Successfully wrote to SD: " + filename);
  } else {
    Serial.println("❌ SD Write Failed! Check wiring, CS pin, or format SD to FAT32.");
  }
}

void readFromSD(String filename) {
  File file = SD.open(filename, FILE_READ);
  if (file) {
    Serial.println("📂 Reading data from " + filename + ":");
    Serial.println("----------------------------------");
    while (file.available()) {
      Serial.write(file.read());
    }
    Serial.println("----------------------------------");
    file.close();
    Serial.println("✅ SD Read Complete.");
  } else {
    Serial.println("❌ SD Read Failed! File might not exist.");
  }
}

// ==========================================
// 📧 EMAIL TEST FUNCTION
// ==========================================
void testEmail() {
  Serial.println("\n📧 Attempting to send test email...");
  
  Session_Config config;
  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;
  config.login.user_domain = "bijlisync.local";

  SMTP_Message message;
  message.sender.name = "Bijli-Sync-Ai Test";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "⚠️ Test Alert: SD & Email Diagnostics";
  message.addRecipient("Tester", RECIPIENT_EMAIL);

  String htmlMsg = "<div style=\"font-family:Arial; color:#333;\"><h2>⚡ Bijli-Sync-Ai Diagnostics</h2>";
  htmlMsg += "<p>If you are reading this, your ESP32 has successfully connected to WiFi and sent an email via SMTP!</p></div>";
  message.html.content = htmlMsg.c_str();

  if (!smtp.connect(&config)) {
     Serial.println("❌ SMTP Connect Error. Check App Password or Internet Connection.");
     return;
  }

  if (MailClient.sendMail(&smtp, &message)) {
    Serial.println("✅ Email sent successfully to " + String(RECIPIENT_EMAIL));
  } else {
    Serial.println("❌ Error sending email: " + smtp.errorReason());
  }
}

// ==========================================
// 🚀 SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n--- BIJLI-SYNC AI HARDWARE TEST ---");

  // 1. CONNECT TO WIFI USING WIFIMANAGER
  Serial.println("📡 Starting Wi-Fi Manager...");
  
  // NOTE: If you need to force the ESP32 to forget old Wi-Fi passwords, 
  // remove the "//" from the line below, upload, run it once, then put the "//" back.
  // wm.resetSettings(); 

  bool res;
  // This will create a Wi-Fi network called "Bijli-Sync-Test" if it can't find your home router.
  res = wm.autoConnect("Bijli-Sync-Test"); 

  if(!res) {
    Serial.println("❌ Failed to connect to Wi-Fi. Restarting...");
    delay(3000);
    ESP.restart();
  } else {
    Serial.println("\n✅ WiFi Connected! IP: " + WiFi.localIP().toString());
  }

  // 2. INITIALIZE AND TEST SD CARD
  Serial.println("\n💾 Initializing SD Card...");
  if (!SD.begin(SD_CS_PIN)) { 
    Serial.println("❌ SD Card Mount Failed! Check connections."); 
  } else {
    Serial.println("✅ SD Card Mounted successfully.");
    
    if(!SD.exists("/test_data.csv")) {
      writeToSD("/test_data.csv", "Date,kWh_Used");
    }
    writeToSD("/test_data.csv", "2026-04-15,14.5");
    readFromSD("/test_data.csv");
  }

  // 3. INITIALIZE AND TEST EMAIL
  MailClient.networkReconnect(true);
  smtp.debug(0); // Set to 1 if you want to see all the background server communication
  testEmail();

  Serial.println("\n🎉 DIAGNOSTICS COMPLETE.");
}

void loop() {
  // Nothing needed in loop for a one-time test
}