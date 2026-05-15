#pragma once
#include <AccelStepper.h>

struct BlindMotor {
  const char* id;
  const char* label;
  AccelStepper* stepper;
  long upPosition;
  long closedPosition;
  bool inverted;
};

extern BlindMotor leftBlind;
extern BlindMotor rightBlind;

void configureMotors();
BlindMotor* getBlindById(const String& id);
void moveBlindTo(BlindMotor& blind, long targetPosition);
void jogBlind(BlindMotor& blind, long steps);
void stopBlind(BlindMotor& blind);
void openBlind(BlindMotor& blind);
void closeBlind(BlindMotor& blind);
void openAllBlinds();
void closeAllBlinds();
void runMotors();
