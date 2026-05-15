#include "Storage.h"
#include "Config.h"

Preferences prefs;
ScheduleDay scheduleDays[7];
String wifiSsid = "";
String wifiPassword = "";
bool selfHostedMode = false;

namespace { unsigned long lastPositionSave = 0; }

void initStorage() { prefs.begin("blinds", false); }

void saveBlindConfig(const BlindMotor& blind) {
  String prefix = String(blind.id) + "_";
  prefs.putLong((prefix + "up").c_str(), blind.upPosition);
  prefs.putLong((prefix + "closed").c_str(), blind.closedPosition);
  prefs.putLong((prefix + "current").c_str(), blind.stepper->currentPosition());
  prefs.putBool((prefix + "inverted").c_str(), blind.inverted);
}

void loadBlindConfig(BlindMotor& blind) {
  String prefix = String(blind.id) + "_";
  blind.upPosition = prefs.getLong((prefix + "up").c_str(), blind.upPosition);
  blind.closedPosition = prefs.getLong((prefix + "closed").c_str(), blind.closedPosition);
  long currentPosition = prefs.getLong((prefix + "current").c_str(), 0);
  blind.inverted = prefs.getBool((prefix + "inverted").c_str(), blind.inverted);
  blind.stepper->setCurrentPosition(currentPosition);
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
  const unsigned long now = millis();
  if (now - lastPositionSave < Config::kPositionSaveIntervalMs) return;
  lastPositionSave = now;
  prefs.putLong("left_current", leftBlind.stepper->currentPosition());
  prefs.putLong("right_current", rightBlind.stepper->currentPosition());
}
