#include <Arduino.h>
#include <WiFi.h>

#include "BlindMotor.h"
#include "Scheduler.h"
#include "Storage.h"
#include "WebUi.h"
#include "WifiManager.h"
#include "Config.h"

void setup() {
  Serial.begin(115200);
  initStorage();
  configureMotors();
  loadWifiConfig();
  loadBlindConfig(leftBlind);
  loadBlindConfig(rightBlind);
  loadSchedule();
  startConfigAP();
  connectConfiguredWifi();
  setupRoutes();
  setupTimeIfConnected();
  Serial.println("Automatic blinds started");
  Serial.print("Setup AP SSID: ");
  Serial.println(Config::kApSsid);
  Serial.print("Setup AP IP: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  handleWebServer();
  runMotors();
  stopConfigAPIfExpired();
  static bool timeConfigured = false;
  if (WiFi.status() == WL_CONNECTED && !timeConfigured) { setupTimeIfConnected(); timeConfigured = true; }
  handleSchedule();
  saveCurrentPositionsIfNeeded();
}
