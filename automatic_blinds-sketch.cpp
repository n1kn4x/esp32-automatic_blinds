#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <AccelStepper.h>
#include <time.h>

// =====================================================
// Automatic Blinds — ESP32 + 2x 28BYJ-48 + ULN2003
// Initial single-file sketch
//
// Features:
// - Two blinds: left and right
// - Persistent Wi-Fi settings
// - Startup configuration AP for 5 minutes
// - Optional permanent AP/self-hosted mode
// - Web UI for movement, calibration, positions, Wi-Fi, schedules
// - Persistent blind positions and current state
// - Weekday open/close schedule
//
// Important limitation:
// 28BYJ-48 steppers have no position feedback. The saved current
// position is only valid if the blind/motor did not move while the
// ESP32 was powered off.
// =====================================================

// =====================================================
// Motor interface type
// =====================================================
#define MotorInterfaceType 4

// =====================================================
// Motor 1 Pins — Left blind
// =====================================================
const int M1_IN1 = 4;
const int M1_IN2 = 18;
const int M1_IN3 = 19;
const int M1_IN4 = 21;

// =====================================================
// Motor 2 Pins — Right blind
// =====================================================
const int M2_IN1 = 27;
const int M2_IN2 = 26;
const int M2_IN3 = 25;
const int M2_IN4 = 33;

// =====================================================
// Wi-Fi / AP settings
// =====================================================
const char *AP_SSID = "Automatic-Blinds-Setup";
const char *AP_PASSWORD = "blinds1234";  // Must be at least 8 chars for WPA2
const unsigned long CONFIG_AP_DURATION_MS = 5UL * 60UL * 1000UL;

// =====================================================
// Motor tuning
// =====================================================
const float DEFAULT_MAX_SPEED = 300.0;
const float DEFAULT_ACCELERATION = 100.0;
const long DEFAULT_JOG_STEPS = 100;
const unsigned long POSITION_SAVE_INTERVAL_MS = 2000;
const unsigned long SCHEDULE_CHECK_INTERVAL_MS = 15000;

// =====================================================
// Time zone: Germany / Berlin with DST
// =====================================================
const char *TZ_BERLIN = "CET-1CEST,M3.5.0,M10.5.0/3";

// =====================================================
// Stepper instances
// Note: order is IN1, IN3, IN2, IN4
// =====================================================
AccelStepper stepperLeft(
  MotorInterfaceType,
  M1_IN1, M1_IN3, M1_IN2, M1_IN4
);

AccelStepper stepperRight(
  MotorInterfaceType,
  M2_IN1, M2_IN3, M2_IN2, M2_IN4
);

WebServer server(80);
Preferences prefs;

// =====================================================
// Data structures
// =====================================================
struct BlindConfig {
  const char *id;
  const char *label;
  AccelStepper *stepper;
  long upPosition;
  long closedPosition;
  long currentPosition;
  bool inverted;
};

struct ScheduleDay {
  bool enabled;
  int openHour;
  int openMinute;
  int closeHour;
  int closeMinute;
};

BlindConfig leftBlind = {
  "left",
  "Left",
  &stepperLeft,
  0,
  2000,
  0,
  false
};

BlindConfig rightBlind = {
  "right",
  "Right",
  &stepperRight,
  0,
  2000,
  0,
  false
};

ScheduleDay scheduleDays[7]; // 0 = Sunday, 1 = Monday, ... 6 = Saturday

String wifiSsid = "";
String wifiPassword = "";
bool selfHostedMode = false;
bool apActive = false;
unsigned long apStartedAt = 0;

unsigned long lastPositionSave = 0;
unsigned long lastScheduleCheck = 0;
int lastScheduleDay = -1;
int lastScheduleMinuteOfDay = -1;

// =====================================================
// Utility helpers
// =====================================================
String htmlEscape(const String &s) {
  String out = s;
  out.replace("&", "&amp;");
  out.replace("<", "&lt;");
  out.replace(">", "&gt;");
  out.replace("\"", "&quot;");
  out.replace("'", "&#39;");
  return out;
}

String checked(bool value) {
  return value ? "checked" : "";
}

String selected(bool value) {
  return value ? "selected" : "";
}

String twoDigits(int value) {
  if (value < 10) return "0" + String(value);
  return String(value);
}

String timeInputValue(int hour, int minute) {
  return twoDigits(hour) + ":" + twoDigits(minute);
}

bool parseTimeInput(const String &value, int &hour, int &minute) {
  if (value.length() != 5 || value.charAt(2) != ':') return false;
  hour = value.substring(0, 2).toInt();
  minute = value.substring(3, 5).toInt();
  return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59;
}

BlindConfig *getBlindById(const String &id) {
  if (id == "left") return &leftBlind;
  if (id == "right") return &rightBlind;
  return nullptr;
}

long applyDirection(const BlindConfig &blind, long steps) {
  return blind.inverted ? -steps : steps;
}

// =====================================================
// Persistence
// =====================================================
void saveBlindConfig(const BlindConfig &blind) {
  String prefix = String(blind.id) + "_";
  prefs.putLong((prefix + "up").c_str(), blind.upPosition);
  prefs.putLong((prefix + "closed").c_str(), blind.closedPosition);
  prefs.putLong((prefix + "current").c_str(), blind.stepper->currentPosition());
  prefs.putBool((prefix + "inverted").c_str(), blind.inverted);
}

void loadBlindConfig(BlindConfig &blind) {
  String prefix = String(blind.id) + "_";
  blind.upPosition = prefs.getLong((prefix + "up").c_str(), blind.upPosition);
  blind.closedPosition = prefs.getLong((prefix + "closed").c_str(), blind.closedPosition);
  blind.currentPosition = prefs.getLong((prefix + "current").c_str(), blind.currentPosition);
  blind.inverted = prefs.getBool((prefix + "inverted").c_str(), blind.inverted);
  blind.stepper->setCurrentPosition(blind.currentPosition);
}

void saveWifiConfig() {
  prefs.putString("wifi_ssid", wifiSsid);
  prefs.putString("wifi_password", wifiPassword);
  prefs.putBool("self_hosted", selfHostedMode);
}

void loadWifiConfig() {
  wifiSsid = prefs.getString("wifi_ssid", "");
  wifiPassword = prefs.getString("wifi_password", "");
  selfHostedMode = prefs.getBool("self_hosted", false);
}

void saveSchedule() {
  for (int d = 0; d < 7; d++) {
    String prefix = "sch_" + String(d) + "_";
    prefs.putBool((prefix + "enabled").c_str(), scheduleDays[d].enabled);
    prefs.putInt((prefix + "oh").c_str(), scheduleDays[d].openHour);
    prefs.putInt((prefix + "om").c_str(), scheduleDays[d].openMinute);
    prefs.putInt((prefix + "ch").c_str(), scheduleDays[d].closeHour);
    prefs.putInt((prefix + "cm").c_str(), scheduleDays[d].closeMinute);
  }
}

void loadSchedule() {
  for (int d = 0; d < 7; d++) {
    String prefix = "sch_" + String(d) + "_";
    scheduleDays[d].enabled = prefs.getBool((prefix + "enabled").c_str(), false);
    scheduleDays[d].openHour = prefs.getInt((prefix + "oh").c_str(), 8);
    scheduleDays[d].openMinute = prefs.getInt((prefix + "om").c_str(), 0);
    scheduleDays[d].closeHour = prefs.getInt((prefix + "ch").c_str(), 20);
    scheduleDays[d].closeMinute = prefs.getInt((prefix + "cm").c_str(), 0);
  }
}

void saveCurrentPositionsIfNeeded() {
  unsigned long now = millis();
  if (now - lastPositionSave < POSITION_SAVE_INTERVAL_MS) return;
  lastPositionSave = now;

  prefs.putLong("left_current", leftBlind.stepper->currentPosition());
  prefs.putLong("right_current", rightBlind.stepper->currentPosition());
}

// =====================================================
// Motor control
// =====================================================
void configureMotors() {
  leftBlind.stepper->setMaxSpeed(DEFAULT_MAX_SPEED);
  leftBlind.stepper->setAcceleration(DEFAULT_ACCELERATION);

  rightBlind.stepper->setMaxSpeed(DEFAULT_MAX_SPEED);
  rightBlind.stepper->setAcceleration(DEFAULT_ACCELERATION);
}

void moveBlindTo(BlindConfig &blind, long targetPosition) {
  blind.stepper->moveTo(targetPosition);
}

void jogBlind(BlindConfig &blind, long steps) {
  blind.stepper->move(applyDirection(blind, steps));
}

void stopBlind(BlindConfig &blind) {
  blind.stepper->stop();
}

void openBlind(BlindConfig &blind) {
  moveBlindTo(blind, blind.upPosition);
}

void closeBlind(BlindConfig &blind) {
  moveBlindTo(blind, blind.closedPosition);
}

void openAllBlinds() {
  openBlind(leftBlind);
  openBlind(rightBlind);
}

void closeAllBlinds() {
  closeBlind(leftBlind);
  closeBlind(rightBlind);
}

void runMotors() {
  leftBlind.stepper->run();
  rightBlind.stepper->run();
}

// =====================================================
// Wi-Fi handling
// =====================================================
void startConfigAP() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  apActive = true;
  apStartedAt = millis();
}

void stopConfigAPIfExpired() {
  if (!apActive) return;
  if (selfHostedMode) return;
  if (millis() - apStartedAt < CONFIG_AP_DURATION_MS) return;

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

  if (wifiSsid.length() == 0) {
    return;
  }

  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
}

void setupTimeIfConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    configTzTime(TZ_BERLIN, "pool.ntp.org", "time.nist.gov");
  }
}

String wifiStatusHtml() {
  String html;
  html += "<p><b>AP:</b> ";
  html += apActive ? "active" : "inactive";
  html += "</p>";
  html += "<p><b>AP SSID:</b> ";
  html += AP_SSID;
  html += "</p>";
  html += "<p><b>AP IP:</b> ";
  html += WiFi.softAPIP().toString();
  html += "</p>";
  html += "<p><b>Station:</b> ";
  html += WiFi.status() == WL_CONNECTED ? "connected" : "not connected";
  html += "</p>";
  html += "<p><b>Station SSID:</b> ";
  html += htmlEscape(wifiSsid);
  html += "</p>";
  html += "<p><b>Station IP:</b> ";
  html += WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "-";
  html += "</p>";
  html += "<p><b>Self-hosted mode:</b> ";
  html += selfHostedMode ? "enabled" : "disabled";
  html += "</p>";
  return html;
}

// =====================================================
// Schedule handling
// =====================================================
bool getLocalTimeSafe(tm &timeInfo) {
  return getLocalTime(&timeInfo, 50);
}

void handleSchedule() {
  unsigned long nowMs = millis();
  if (nowMs - lastScheduleCheck < SCHEDULE_CHECK_INTERVAL_MS) return;
  lastScheduleCheck = nowMs;

  if (WiFi.status() != WL_CONNECTED) return;

  tm timeInfo;
  if (!getLocalTimeSafe(timeInfo)) return;

  int day = timeInfo.tm_wday;
  int minuteOfDay = timeInfo.tm_hour * 60 + timeInfo.tm_min;

  if (day == lastScheduleDay && minuteOfDay == lastScheduleMinuteOfDay) {
    return;
  }

  lastScheduleDay = day;
  lastScheduleMinuteOfDay = minuteOfDay;

  ScheduleDay &s = scheduleDays[day];
  if (!s.enabled) return;

  int openMinuteOfDay = s.openHour * 60 + s.openMinute;
  int closeMinuteOfDay = s.closeHour * 60 + s.closeMinute;

  if (minuteOfDay == openMinuteOfDay) {
    openAllBlinds();
  }

  if (minuteOfDay == closeMinuteOfDay) {
    closeAllBlinds();
  }
}

// =====================================================
// Web UI
// =====================================================
String pageHeader(const String &title) {
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
  html += "<h1>" + htmlEscape(title) + "</h1>";
  html += "<p><a href='/'>Home</a> · <a href='/wifi'>Wi-Fi</a> · <a href='/schedule'>Schedule</a></p>";
  return html;
}

String pageFooter() {
  return "</main></body></html>";
}

String blindCard(BlindConfig &blind) {
  long pos = blind.stepper->currentPosition();
  String html;
  html += "<section>";
  html += "<h2>" + String(blind.label) + " blind</h2>";
  html += "<p><b>Current position:</b> " + String(pos) + "</p>";
  html += "<p><b>Up position:</b> " + String(blind.upPosition) + " · <b>Closed position:</b> " + String(blind.closedPosition) + "</p>";
  html += "<form class='row' method='post' action='/action'>";
  html += "<input type='hidden' name='blind' value='" + String(blind.id) + "'>";
  html += "<button name='cmd' value='open'>Open</button>";
  html += "<button name='cmd' value='close'>Close</button>";
  html += "<button class='danger' name='cmd' value='stop'>Stop</button>";
  html += "</form>";

  html += "<h3>Calibration / jog</h3>";
  html += "<form class='row' method='post' action='/action'>";
  html += "<input type='hidden' name='blind' value='" + String(blind.id) + "'>";
  html += "<input type='number' name='steps' value='" + String(DEFAULT_JOG_STEPS) + "'>";
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

  html += "<section>";
  html += "<h2>Both blinds</h2>";
  html += "<form class='row' method='post' action='/action'>";
  html += "<input type='hidden' name='blind' value='all'>";
  html += "<button name='cmd' value='open'>Open both</button>";
  html += "<button name='cmd' value='close'>Close both</button>";
  html += "<button class='danger' name='cmd' value='stop'>Stop both</button>";
  html += "</form>";
  html += "</section>";

  html += blindCard(leftBlind);
  html += blindCard(rightBlind);

  html += "<section><h2>Status</h2>";
  html += wifiStatusHtml();

  tm timeInfo;
  if (getLocalTimeSafe(timeInfo)) {
    html += "<p><b>Local time:</b> ";
    html += String(1900 + timeInfo.tm_year) + "-" + twoDigits(timeInfo.tm_mon + 1) + "-" + twoDigits(timeInfo.tm_mday);
    html += " " + twoDigits(timeInfo.tm_hour) + ":" + twoDigits(timeInfo.tm_min);
    html += "</p>";
  } else {
    html += "<p><b>Local time:</b> not synchronized</p>";
  }
  html += "</section>";

  html += pageFooter();
  server.send(200, "text/html", html);
}

void handleAction() {
  String blindId = server.arg("blind");
  String cmd = server.arg("cmd");
  long steps = server.arg("steps").toInt();
  if (steps <= 0) steps = DEFAULT_JOG_STEPS;

  if (blindId == "all") {
    if (cmd == "open") openAllBlinds();
    else if (cmd == "close") closeAllBlinds();
    else if (cmd == "stop") {
      stopBlind(leftBlind);
      stopBlind(rightBlind);
    }
    server.sendHeader("Location", "/");
    server.send(303);
    return;
  }

  BlindConfig *blind = getBlindById(blindId);
  if (!blind) {
    server.send(400, "text/plain", "Unknown blind");
    return;
  }

  if (cmd == "open") {
    openBlind(*blind);
  } else if (cmd == "close") {
    closeBlind(*blind);
  } else if (cmd == "stop") {
    stopBlind(*blind);
  } else if (cmd == "jog_up") {
    jogBlind(*blind, -steps);
  } else if (cmd == "jog_down") {
    jogBlind(*blind, steps);
  } else if (cmd == "set_up") {
    blind->upPosition = blind->stepper->currentPosition();
    saveBlindConfig(*blind);
  } else if (cmd == "set_closed") {
    blind->closedPosition = blind->stepper->currentPosition();
    saveBlindConfig(*blind);
  } else if (cmd == "zero_here") {
    long oldPos = blind->stepper->currentPosition();
    blind->stepper->setCurrentPosition(0);
    blind->upPosition -= oldPos;
    blind->closedPosition -= oldPos;
    saveBlindConfig(*blind);
  } else if (cmd == "save_direction") {
    blind->inverted = server.hasArg("inverted");
    saveBlindConfig(*blind);
  }

  server.sendHeader("Location", "/");
  server.send(303);
}

void handleWifiPage() {
  String html = pageHeader("Wi-Fi Settings");
  html += "<section>";
  html += "<h2>Current status</h2>";
  html += wifiStatusHtml();
  html += "</section>";

  html += "<section>";
  html += "<h2>Configure Wi-Fi</h2>";
  html += "<form method='post' action='/wifi/save'>";
  html += "<p><label>Wi-Fi SSID<br><input name='ssid' value='" + htmlEscape(wifiSsid) + "'></label></p>";
  html += "<p><label>Wi-Fi password<br><input name='password' type='password' value='" + htmlEscape(wifiPassword) + "'></label></p>";
  html += "<p><label><input type='checkbox' name='self_hosted' value='1' " + checked(selfHostedMode) + "> Stay in self-hosted AP mode</label></p>";
  html += "<p class='muted'>On boot, the setup AP is available for 5 minutes. If self-hosted mode is enabled, it stays available permanently.</p>";
  html += "<button>Save Wi-Fi settings</button>";
  html += "</form>";
  html += "</section>";
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

void handleSchedulePage() {
  const char *dayNames[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  String html = pageHeader("Schedule");
  html += "<section>";
  html += "<h2>Weekday schedule</h2>";
  html += "<p class='muted'>The schedule requires the ESP32 to be connected to Wi-Fi so it can synchronize time using NTP.</p>";
  html += "<form method='post' action='/schedule/save'>";
  html += "<table><tr><th>Day</th><th>Enabled</th><th>Open</th><th>Close</th></tr>";

  for (int d = 0; d < 7; d++) {
    html += "<tr>";
    html += "<td>" + String(dayNames[d]) + "</td>";
    html += "<td><input type='checkbox' name='en_" + String(d) + "' value='1' " + checked(scheduleDays[d].enabled) + "></td>";
    html += "<td><input type='time' name='open_" + String(d) + "' value='" + timeInputValue(scheduleDays[d].openHour, scheduleDays[d].openMinute) + "'></td>";
    html += "<td><input type='time' name='close_" + String(d) + "' value='" + timeInputValue(scheduleDays[d].closeHour, scheduleDays[d].closeMinute) + "'></td>";
    html += "</tr>";
  }

  html += "</table>";
  html += "<p><button>Save schedule</button></p>";
  html += "</form>";
  html += "</section>";
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

void setupRoutes() {
  server.on("/", HTTP_GET, handleHome);
  server.on("/action", HTTP_POST, handleAction);
  server.on("/wifi", HTTP_GET, handleWifiPage);
  server.on("/wifi/save", HTTP_POST, handleWifiSave);
  server.on("/schedule", HTTP_GET, handleSchedulePage);
  server.on("/schedule/save", HTTP_POST, handleScheduleSave);

  server.onNotFound([]() {
    server.sendHeader("Location", "/");
    server.send(303);
  });
}

// =====================================================
// Arduino lifecycle
// =====================================================
void setup() {
  Serial.begin(115200);

  prefs.begin("blinds", false);

  configureMotors();

  loadWifiConfig();
  loadBlindConfig(leftBlind);
  loadBlindConfig(rightBlind);
  loadSchedule();

  startConfigAP();
  connectConfiguredWifi();

  setupRoutes();
  server.begin();

  setupTimeIfConnected();

  Serial.println("Automatic blinds started");
  Serial.print("Setup AP SSID: ");
  Serial.println(AP_SSID);
  Serial.print("Setup AP IP: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();

  runMotors();

  stopConfigAPIfExpired();

  if (WiFi.status() == WL_CONNECTED) {
    static bool timeConfigured = false;
    if (!timeConfigured) {
      setupTimeIfConnected();
      timeConfigured = true;
    }
  }

  handleSchedule();
  saveCurrentPositionsIfNeeded();
}
