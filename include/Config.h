#pragma once

#include <Arduino.h>

namespace Config {
constexpr int kMotorInterfaceType = 4;

// Left blind motor pins
constexpr int kLeftIn1 = 4;
constexpr int kLeftIn2 = 18;
constexpr int kLeftIn3 = 19;
constexpr int kLeftIn4 = 21;

// Right blind motor pins
constexpr int kRightIn1 = 27;
constexpr int kRightIn2 = 26;
constexpr int kRightIn3 = 25;
constexpr int kRightIn4 = 33;

constexpr const char* kApSsid = "Automatic-Blinds-Setup";
constexpr const char* kApPassword = "blinds1234";
constexpr const char* kMdnsHostname = "blinds";
constexpr const char* kMdnsAddress = "blinds.local";
constexpr unsigned long kConfigApDurationMs = 5UL * 60UL * 1000UL;

constexpr float kDefaultMaxSpeed = 300.0F;
constexpr float kDefaultAcceleration = 100.0F;
constexpr long kDefaultJogSteps = 100;
constexpr unsigned long kPositionSaveIntervalMs = 2000;
constexpr unsigned long kScheduleCheckIntervalMs = 15000;

constexpr const char* kTimezoneBerlin = "CET-1CEST,M3.5.0,M10.5.0/3";
constexpr uint16_t kWebServerPort = 80;
}  // namespace Config
