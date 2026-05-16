#include "BlindMotor.h"

#include <Arduino.h>

#include "Config.h"

namespace {
AccelStepper stepperLeft(Config::kMotorInterfaceType, Config::kLeftIn1, Config::kLeftIn3, Config::kLeftIn2, Config::kLeftIn4);
AccelStepper stepperRight(Config::kMotorInterfaceType, Config::kRightIn1, Config::kRightIn3, Config::kRightIn2, Config::kRightIn4);
constexpr unsigned long kCoilHoldMillis = 10000UL;

struct MotorPowerState {
  bool outputsEnabled = true;
  unsigned long lastMotionMillis = 0;
};

MotorPowerState leftMotorPowerState;
MotorPowerState rightMotorPowerState;

long applyDirection(const BlindMotor& blind, long steps) { return blind.inverted ? -steps : steps; }

bool isMotorMoving(const BlindMotor& blind) {
  return blind.stepper->distanceToGo() != 0 || blind.stepper->speed() != 0.0f;
}

void updateMotorPowerState(BlindMotor& blind, MotorPowerState& state, unsigned long now) {
  if (isMotorMoving(blind)) {
    state.lastMotionMillis = now;
    if (!state.outputsEnabled) {
      blind.stepper->enableOutputs();
      state.outputsEnabled = true;
    }
    return;
  }

  if (state.outputsEnabled && now - state.lastMotionMillis >= kCoilHoldMillis) {
    blind.stepper->disableOutputs();
    state.outputsEnabled = false;
  }
}
}  // namespace

BlindMotor leftBlind{"left", "Left", &stepperLeft, 0, 2000, false};
BlindMotor rightBlind{"right", "Right", &stepperRight, 0, 2000, false};

void configureMotors() {
  leftBlind.stepper->setMaxSpeed(Config::kDefaultMaxSpeed);
  leftBlind.stepper->setAcceleration(Config::kDefaultAcceleration);
  rightBlind.stepper->setMaxSpeed(Config::kDefaultMaxSpeed);
  rightBlind.stepper->setAcceleration(Config::kDefaultAcceleration);
  const unsigned long now = millis();
  leftMotorPowerState.lastMotionMillis = now;
  rightMotorPowerState.lastMotionMillis = now;
}

BlindMotor* getBlindById(const String& id) {
  if (id == "left") return &leftBlind;
  if (id == "right") return &rightBlind;
  return nullptr;
}

void moveBlindTo(BlindMotor& blind, long targetPosition) { blind.stepper->moveTo(targetPosition); }
void jogBlind(BlindMotor& blind, long steps) { blind.stepper->move(applyDirection(blind, steps)); }
void stopBlind(BlindMotor& blind) { blind.stepper->stop(); }
void openBlind(BlindMotor& blind) { moveBlindTo(blind, blind.upPosition); }
void closeBlind(BlindMotor& blind) { moveBlindTo(blind, blind.closedPosition); }
void openAllBlinds() { openBlind(leftBlind); openBlind(rightBlind); }
void closeAllBlinds() { closeBlind(leftBlind); closeBlind(rightBlind); }
void runMotors() {
  leftBlind.stepper->run();
  rightBlind.stepper->run();

  const unsigned long now = millis();
  updateMotorPowerState(leftBlind, leftMotorPowerState, now);
  updateMotorPowerState(rightBlind, rightMotorPowerState, now);
}
