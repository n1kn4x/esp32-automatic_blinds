#pragma once
#include <Arduino.h>

extern bool apActive;

void startConfigAP();
void stopConfigAPIfExpired();
void connectConfiguredWifi();
void setupTimeIfConnected();
String wifiStatusHtml();
