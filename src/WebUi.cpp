#include "WebUi.h"

#include <WebServer.h>
#include <WiFi.h>
#include <time.h>

#include "BlindMotor.h"
#include "Config.h"
#include "Storage.h"
#include "WifiManager.h"

namespace {
WebServer server(Config::kWebServerPort);

String checked(bool v) { return v ? "checked" : ""; }
String twoDigits(int value) { return value < 10 ? "0" + String(value) : String(value); }
String timeInputValue(int hour, int minute) { return twoDigits(hour) + ":" + twoDigits(minute); }
bool parseTimeInput(const String& value, int& hour, int& minute) {
  if (value.length() != 5 || value.charAt(2) != ':') return false;
  hour = value.substring(0, 2).toInt(); minute = value.substring(3, 5).toInt();
  return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59;
}
String pageHeader(const String& title) { return "<!doctype html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><title>Automatic Blinds</title></head><body><h1>" + title + "</h1><p><a href='/'>Home</a> · <a href='/wifi'>Wi-Fi</a> · <a href='/schedule'>Schedule</a></p>"; }
String pageFooter() { return "</body></html>"; }

void handleHome() {
  String html = pageHeader("Automatic Blinds");
  html += "<form method='post' action='/action'><input type='hidden' name='blind' value='all'><button name='cmd' value='open'>Open both</button><button name='cmd' value='close'>Close both</button><button name='cmd' value='stop'>Stop both</button></form>";
  html += "<p>Left current: " + String(leftBlind.stepper->currentPosition()) + "</p>";
  html += "<p>Right current: " + String(rightBlind.stepper->currentPosition()) + "</p>";
  html += wifiStatusHtml();
  html += pageFooter();
  server.send(200, "text/html", html);
}

void handleAction() {
  const String blindId = server.arg("blind");
  const String cmd = server.arg("cmd");
  long steps = server.arg("steps").toInt();
  if (steps <= 0) steps = Config::kDefaultJogSteps;
  if (blindId == "all") {
    if (cmd == "open") openAllBlinds();
    else if (cmd == "close") closeAllBlinds();
    else if (cmd == "stop") { stopBlind(leftBlind); stopBlind(rightBlind); }
  } else {
    BlindMotor* blind = getBlindById(blindId);
    if (blind == nullptr) { server.send(400, "text/plain", "Unknown blind"); return; }
    if (cmd == "open") openBlind(*blind);
    else if (cmd == "close") closeBlind(*blind);
    else if (cmd == "stop") stopBlind(*blind);
    else if (cmd == "jog_up") jogBlind(*blind, -steps);
    else if (cmd == "jog_down") jogBlind(*blind, steps);
    else if (cmd == "set_up") { blind->upPosition = blind->stepper->currentPosition(); saveBlindConfig(*blind); }
    else if (cmd == "set_closed") { blind->closedPosition = blind->stepper->currentPosition(); saveBlindConfig(*blind); }
    else if (cmd == "zero_here") { const long oldPos = blind->stepper->currentPosition(); blind->stepper->setCurrentPosition(0); blind->upPosition -= oldPos; blind->closedPosition -= oldPos; saveBlindConfig(*blind); }
    else if (cmd == "save_direction") { blind->inverted = server.hasArg("inverted"); saveBlindConfig(*blind); }
  }
  server.sendHeader("Location", "/"); server.send(303);
}

void handleWifi() { String html = pageHeader("Wi-Fi Settings"); html += wifiStatusHtml(); html += pageFooter(); server.send(200, "text/html", html); }
void handleWifiSave() { wifiSsid = server.arg("ssid"); wifiPassword = server.arg("password"); selfHostedMode = server.hasArg("self_hosted"); saveWifiConfig(); server.sendHeader("Location", "/wifi"); server.send(303); }
void handleSchedule() {
  String html = pageHeader("Schedule"); html += "<form method='post' action='/schedule/save'>";
  for (int d=0; d<7; d++) {
    html += "<p>D" + String(d) + " <input type='checkbox' name='en_" + String(d) + "' " + checked(scheduleDays[d].enabled) + "> O:<input type='time' name='open_" + String(d) + "' value='" + timeInputValue(scheduleDays[d].openHour, scheduleDays[d].openMinute) + "'> C:<input type='time' name='close_" + String(d) + "' value='" + timeInputValue(scheduleDays[d].closeHour, scheduleDays[d].closeMinute) + "'></p>";
  }
  html += "<button>Save</button></form>" + pageFooter(); server.send(200, "text/html", html);
}
void handleScheduleSave() {
  for (int d = 0; d < 7; d++) { scheduleDays[d].enabled = server.hasArg("en_" + String(d)); int h,m; if (parseTimeInput(server.arg("open_"+String(d)), h, m)) {scheduleDays[d].openHour=h; scheduleDays[d].openMinute=m;} if (parseTimeInput(server.arg("close_"+String(d)), h, m)) {scheduleDays[d].closeHour=h; scheduleDays[d].closeMinute=m;} }
  saveSchedule(); server.sendHeader("Location", "/schedule"); server.send(303);
}
}  // namespace

void setupRoutes() {
  server.on("/", HTTP_GET, handleHome);
  server.on("/action", HTTP_POST, handleAction);
  server.on("/wifi", HTTP_GET, handleWifi);
  server.on("/wifi/save", HTTP_POST, handleWifiSave);
  server.on("/schedule", HTTP_GET, handleSchedule);
  server.on("/schedule/save", HTTP_POST, handleScheduleSave);
  server.onNotFound([]() { server.sendHeader("Location", "/"); server.send(303); });
  server.begin();
}

void handleWebServer() { server.handleClient(); }
