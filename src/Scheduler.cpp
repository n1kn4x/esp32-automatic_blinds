#include "Scheduler.h"

#include <WiFi.h>
#include <time.h>

#include "BlindMotor.h"
#include "Config.h"
#include "Storage.h"

namespace {
unsigned long lastScheduleCheck = 0;
int lastScheduleDay = -1;
int lastScheduleMinuteOfDay = -1;

bool getLocalTimeSafe(tm& timeInfo) { return getLocalTime(&timeInfo, 50); }
}  // namespace

void handleSchedule() {
  const unsigned long nowMs = millis();
  if (nowMs - lastScheduleCheck < Config::kScheduleCheckIntervalMs) return;
  lastScheduleCheck = nowMs;
  if (WiFi.status() != WL_CONNECTED) return;

  tm timeInfo;
  if (!getLocalTimeSafe(timeInfo)) return;

  const int day = timeInfo.tm_wday;
  const int minuteOfDay = timeInfo.tm_hour * 60 + timeInfo.tm_min;
  if (day == lastScheduleDay && minuteOfDay == lastScheduleMinuteOfDay) return;
  lastScheduleDay = day;
  lastScheduleMinuteOfDay = minuteOfDay;

  ScheduleDay& s = scheduleDays[day];
  if (!s.enabled) return;
  if (minuteOfDay == (s.openHour * 60 + s.openMinute)) openAllBlinds();
  if (minuteOfDay == (s.closeHour * 60 + s.closeMinute)) closeAllBlinds();
}
