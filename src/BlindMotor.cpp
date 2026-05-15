#include "BlindMotor.h"

#include "Config.h"

namespace {
AccelStepper stepperLeft(Config::kMotorInterfaceType, Config::kLeftIn1, Config::kLeftIn3, Config::kLeftIn2, Config::kLeftIn4);
AccelStepper stepperRight(Config::kMotorInterfaceType, Config::kRightIn1, Config::kRightIn3, Config::kRightIn2, Config::kRightIn4);

long applyDirection(const BlindMotor& blind, long steps) { return blind.inverted ? -steps : steps; }
}  // namespace

BlindMotor leftBlind{"left", "Left", &stepperLeft, 0, 2000, false};
BlindMotor rightBlind{"right", "Right", &stepperRight, 0, 2000, false};

void configureMotors() {
  leftBlind.stepper->setMaxSpeed(Config::kDefaultMaxSpeed);
  leftBlind.stepper->setAcceleration(Config::kDefaultAcceleration);
  rightBlind.stepper->setMaxSpeed(Config::kDefaultMaxSpeed);
  rightBlind.stepper->setAcceleration(Config::kDefaultAcceleration);
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
void runMotors() { leftBlind.stepper->run(); rightBlind.stepper->run(); }
