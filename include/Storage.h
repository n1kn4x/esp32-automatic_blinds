#pragma once
#include <Preferences.h>
#include "BlindMotor.h"

struct ScheduleDay {
  bool enabled;
  int openHour;
  int openMinute;
  int closeHour;
  int closeMinute;
};

extern Preferences prefs;
extern ScheduleDay scheduleDays[7];
extern String wifiSsid;
extern String wifiPassword;
extern bool selfHostedMode;

void initStorage();
void saveBlindConfig(const BlindMotor& blind);
void loadBlindConfig(BlindMotor& blind);
void saveWifiConfig();
void loadWifiConfig();
void saveSchedule();
void loadSchedule();
void saveCurrentPositionsIfNeeded();
