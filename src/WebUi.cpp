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

String htmlEscape(const String& s) {
  String out = s;
  out.replace("&", "&amp;");
  out.replace("<", "&lt;");
  out.replace(">", "&gt;");
  out.replace("\"", "&quot;");
  out.replace("'", "&#39;");
  return out;
}

String checked(bool v) { return v ? "checked" : ""; }
String twoDigits(int value) { return value < 10 ? "0" + String(value) : String(value); }
String timeInputValue(int hour, int minute) { return twoDigits(hour) + ":" + twoDigits(minute); }
bool parseTimeInput(const String& value, int& hour, int& minute) {
  if (value.length() != 5 || value.charAt(2) != ':') return false;
  hour = value.substring(0, 2).toInt();
  minute = value.substring(3, 5).toInt();
  return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59;
}

String pageHeader(const String& title) {
  String html;
  html += "<!doctype html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Automatic Blinds</title>";
  html += "<style>";
  html += "body{font-family:system-ui,-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif;margin:24px;background:#f7f7f7;color:#111}";
  html += "main{max-width:900px;margin:0 auto}";
  html += "section{background:white;padding:18px;margin:16px 0;border-radius:14px;box-shadow:0 2px 10px rgba(0,0,0,.06)}";
  html += "h1,h2{margin-top:0}";
  html += "button,input,select{font:inherit;padding:9px 12px;margin:4px;border-radius:8px;border:1px solid #ccc}";
  html += "button{cursor:pointer;background:#111;color:white;border-color:#111}";
  html += "button.secondary{background:white;color:#111}";
  html += "table{width:100%;border-collapse:collapse}";
  html += "td,th{padding:8px;border-bottom:1px solid #eee;text-align:left}";
  html += ".row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}";
  html += ".muted{color:#666}";
  html += ".danger{background:#9b111e;border-color:#9b111e}";
  html += "</style></head><body><main>";
  html += "<h1>" + title + "</h1>";
  html += "<p><a href='/'>Home</a> - <a href='/wifi'>Wi-Fi</a> - <a href='/schedule'>Schedule</a></p>";
  return html;
}

String pageFooter() { return "</main></body></html>"; }

String blindCard(BlindMotor& blind) {
  const long pos = blind.stepper->currentPosition();
  String html;
  html += "<section>";
  html += "<h2>" + String(blind.label) + " blind</h2>";
  html += "<p><b>Current position:</b> " + String(pos) + "</p>";
  html += "<p><b>Up position:</b> " + String(blind.upPosition) + " - <b>Closed position:</b> " + String(blind.closedPosition) + "</p>";
  html += "<form class='row' method='post' action='/action'>";
  html += "<input type='hidden' name='blind' value='" + String(blind.id) + "'>";
  html += "<button name='cmd' value='open'>Open</button>";
  html += "<button name='cmd' value='close'>Close</button>";
  html += "<button class='danger' name='cmd' value='stop'>Stop</button>";
  html += "</form>";

  html += "<h3>Calibration / jog</h3>";
  html += "<form class='row' method='post' action='/action'>";
  html += "<input type='hidden' name='blind' value='" + String(blind.id) + "'>";
  html += "<input type='number' name='steps' value='" + String(Config::kDefaultJogSteps) + "'>";
  html += "<button name='cmd' value='jog_up'>Jog up</button>";
  html += "<button name='cmd' value='jog_down'>Jog down</button>";
  html += "</form>";

  html += "<form class='row' method='post' action='/action'>";
  html += "<input type='hidden' name='blind' value='" + String(blind.id) + "'>";
  html += "<button class='secondary' name='cmd' value='set_up'>Set current as up</button>";
  html += "<button class='secondary' name='cmd' value='set_closed'>Set current as closed</button>";
  html += "<button class='secondary' name='cmd' value='zero_here'>Set current as zero</button>";
  html += "</form>";

  html += "<form class='row' method='post' action='/action'>";
  html += "<input type='hidden' name='blind' value='" + String(blind.id) + "'>";
  html += "<label><input type='checkbox' name='inverted' value='1' " + checked(blind.inverted) + "> Invert jog direction</label>";
  html += "<button class='secondary' name='cmd' value='save_direction'>Save direction</button>";
  html += "</form>";

  html += "</section>";
  return html;
}

void handleHome() {
  String html = pageHeader("Automatic Blinds");
  html += "<section><h2>Both blinds</h2>";
  html += "<form class='row' method='post' action='/action'><input type='hidden' name='blind' value='all'><button name='cmd' value='open'>Open both</button><button name='cmd' value='close'>Close both</button><button class='danger' name='cmd' value='stop'>Stop both</button></form></section>";

  html += blindCard(leftBlind);
  html += blindCard(rightBlind);

  html += "<section><h2>Status</h2>";
  html += wifiStatusHtml();

  tm timeInfo;
  if (getLocalTime(&timeInfo, 50)) {
    html += "<p><b>Local time:</b> ";
    html += String(1900 + timeInfo.tm_year) + "-" + twoDigits(timeInfo.tm_mon + 1) + "-" + twoDigits(timeInfo.tm_mday);
    html += " " + twoDigits(timeInfo.tm_hour) + ":" + twoDigits(timeInfo.tm_min) + "</p>";
  } else {
    html += "<p><b>Local time:</b> not synchronized</p>";
  }
  html += "</section>";
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
    else if (cmd == "stop") {
      stopBlind(leftBlind);
      stopBlind(rightBlind);
    }
  } else {
    BlindMotor* blind = getBlindById(blindId);
    if (blind == nullptr) {
      server.send(400, "text/plain", "Unknown blind");
      return;
    }
    if (cmd == "open") openBlind(*blind);
    else if (cmd == "close") closeBlind(*blind);
    else if (cmd == "stop") stopBlind(*blind);
    else if (cmd == "jog_up") jogBlind(*blind, -steps);
    else if (cmd == "jog_down") jogBlind(*blind, steps);
    else if (cmd == "set_up") {
      blind->upPosition = blind->stepper->currentPosition();
      saveBlindConfig(*blind);
    } else if (cmd == "set_closed") {
      blind->closedPosition = blind->stepper->currentPosition();
      saveBlindConfig(*blind);
    } else if (cmd == "zero_here") {
      const long oldPos = blind->stepper->currentPosition();
      blind->stepper->setCurrentPosition(0);
      blind->upPosition -= oldPos;
      blind->closedPosition -= oldPos;
      saveBlindConfig(*blind);
    } else if (cmd == "save_direction") {
      blind->inverted = server.hasArg("inverted");
      saveBlindConfig(*blind);
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleWifi() {
  String html = pageHeader("Wi-Fi Settings");
  html += "<section><h2>Current status</h2>" + wifiStatusHtml() + "</section>";
  html += "<section><h2>Configure Wi-Fi</h2><form method='post' action='/wifi/save'>";
  html += "<p><label>Wi-Fi SSID<br><input name='ssid' value='" + htmlEscape(wifiSsid) + "'></label></p>";
  html += "<p><label>Wi-Fi password<br><input name='password' type='password' value='" + htmlEscape(wifiPassword) + "'></label></p>";
  html += "<p><label><input type='checkbox' name='self_hosted' value='1' " + checked(selfHostedMode) + "> Stay in self-hosted AP mode</label></p>";
  html += "<p class='muted'>On boot, the setup AP is available for 5 minutes. If self-hosted mode is enabled, it stays available permanently.</p>";
  html += "<button>Save Wi-Fi settings</button></form></section>";
  html += pageFooter();
  server.send(200, "text/html", html);
}

void handleWifiSave() {
  wifiSsid = server.arg("ssid");
  wifiPassword = server.arg("password");
  selfHostedMode = server.hasArg("self_hosted");
  saveWifiConfig();
  if (!selfHostedMode && wifiSsid.length() > 0) {
    WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  }
  server.sendHeader("Location", "/wifi");
  server.send(303);
}

void handleSchedule() {
  const char* dayNames[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  String html = pageHeader("Schedule");
  html += "<section><h2>Weekday schedule</h2>";
  html += "<p class='muted'>The schedule requires the ESP32 to be connected to Wi-Fi so it can synchronize time using NTP.</p>";
  html += "<form method='post' action='/schedule/save'>";
  html += "<table><tr><th>Day</th><th>Enabled</th><th>Open</th><th>Close</th></tr>";
  for (int d = 0; d < 7; d++) {
    html += "<tr><td>" + String(dayNames[d]) + "</td>";
    html += "<td><input type='checkbox' name='en_" + String(d) + "' value='1' " + checked(scheduleDays[d].enabled) + "></td>";
    html += "<td><input type='time' name='open_" + String(d) + "' value='" + timeInputValue(scheduleDays[d].openHour, scheduleDays[d].openMinute) + "'></td>";
    html += "<td><input type='time' name='close_" + String(d) + "' value='" + timeInputValue(scheduleDays[d].closeHour, scheduleDays[d].closeMinute) + "'></td></tr>";
  }
  html += "</table><p><button>Save schedule</button></p></form></section>";
  html += pageFooter();
  server.send(200, "text/html", html);
}

void handleScheduleSave() {
  for (int d = 0; d < 7; d++) {
    scheduleDays[d].enabled = server.hasArg("en_" + String(d));
    int h, m;
    if (parseTimeInput(server.arg("open_" + String(d)), h, m)) {
      scheduleDays[d].openHour = h;
      scheduleDays[d].openMinute = m;
    }
    if (parseTimeInput(server.arg("close_" + String(d)), h, m)) {
      scheduleDays[d].closeHour = h;
      scheduleDays[d].closeMinute = m;
    }
  }
  saveSchedule();
  server.sendHeader("Location", "/schedule");
  server.send(303);
}
}  // namespace

void setupRoutes() {
  server.on("/", HTTP_GET, handleHome);
  server.on("/action", HTTP_POST, handleAction);
  server.on("/wifi", HTTP_GET, handleWifi);
  server.on("/wifi/save", HTTP_POST, handleWifiSave);
  server.on("/schedule", HTTP_GET, handleSchedule);
  server.on("/schedule/save", HTTP_POST, handleScheduleSave);
  server.onNotFound([]() {
    server.sendHeader("Location", "/");
    server.send(303);
  });
  server.begin();
}

void handleWebServer() { server.handleClient(); }
