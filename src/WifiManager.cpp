#include "WifiManager.h"

#include <WiFi.h>
#include <time.h>

#include "Config.h"
#include "Storage.h"

bool apActive = false;
namespace { unsigned long apStartedAt = 0; }

void startConfigAP() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(Config::kApSsid, Config::kApPassword);
  apActive = true;
  apStartedAt = millis();
}

void stopConfigAPIfExpired() {
  if (!apActive || selfHostedMode) return;
  if (millis() - apStartedAt < Config::kConfigApDurationMs) return;
  WiFi.softAPdisconnect(true);
  apActive = false;
  if (WiFi.status() != WL_CONNECTED && wifiSsid.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  }
}

void connectConfiguredWifi() {
  if (selfHostedMode) {
    WiFi.mode(WIFI_AP);
    return;
  }
  if (wifiSsid.length() == 0) return;
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
}

void setupTimeIfConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    configTzTime(Config::kTimezoneBerlin, "pool.ntp.org", "time.nist.gov");
  }
}

String wifiStatusHtml() {
  String html;
  html += "<p><b>AP:</b> ";
  html += apActive ? "active" : "inactive";
  html += "</p><p><b>AP SSID:</b> ";
  html += Config::kApSsid;
  html += "</p><p><b>AP IP:</b> ";
  html += WiFi.softAPIP().toString();
  html += "</p><p><b>Station:</b> ";
  html += WiFi.status() == WL_CONNECTED ? "connected" : "not connected";
  html += "</p><p><b>Station SSID:</b> ";
  html += wifiSsid;
  html += "</p><p><b>Station IP:</b> ";
  html += WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "-";
  html += "</p><p><b>Self-hosted mode:</b> ";
  html += selfHostedMode ? "enabled" : "disabled";
  html += "</p>";
  return html;
}
