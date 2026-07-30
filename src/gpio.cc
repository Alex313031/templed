// Copyright (c) 2026 Alex313031

// LED control via the WiringPi library (vendored as a git submodule).
// WiringPi memory-maps the SoC's GPIO registers (through /dev/gpiomem0 on
// the Pi 5, /dev/gpiomem earlier), so digitalWrite() is a register poke,
// not a syscall - cheap enough to call every refresh without a thought.
// On Win32 there is no equivalent; PCs simply don't expose user GPIO.

#include "gpio.h"

#include <wiringPi.h>

namespace {
  // All four pins in one place, for the set/clear loops below
  constexpr std::array<int, 4> kLedPins{kBluePin, kGreenPin, kYellowPin, kRedPin};

  // Guards LedsAllOff(): digitalWrite() before wiringPiSetupGpio() would
  // poke unmapped memory. Lets the signal handler call LedsAllOff()
  // unconditionally
  bool gpio_ready = false;

  int PinForBand(TempBand band) {
    switch (band) {
      case TempBand::kBlue:
        return kBluePin;
      case TempBand::kGreen:
        return kGreenPin;
      case TempBand::kYellow:
        return kYellowPin;
      case TempBand::kRed:
        return kRedPin;
    }
    return kRedPin; // unreachable, satisfies the compiler
  }
} // namespace

TempBand BandForTemp(double celsius) {
  if (celsius <= 45.0) {
    return TempBand::kBlue;
  }
  if (celsius <= 65.0) {
    return TempBand::kGreen;
  }
  if (celsius <= 79.0) {
    return TempBand::kYellow;
  }
  return TempBand::kRed; // 80 C+ is where the firmware starts throttling
}

const char* BandName(TempBand band) {
  switch (band) {
    case TempBand::kBlue:
      return "Blue";
    case TempBand::kGreen:
      return "Green";
    case TempBand::kYellow:
      return "Yellow";
    case TempBand::kRed:
      return "Red";
  }
  return "?"; // unreachable
}

bool InitGpio() {
  // wiringPiSetupGpio() selects BCM ("GPIO17") numbering, matching the
  // breakout board's labels. The other setup flavors use WiringPi's own
  // legacy pin numbers and are a classic source of "wrong LED lit" bugs.
  // Returns 0 on success and prints its own diagnostics on failure
  if (wiringPiSetupGpio() != 0) {
    return false;
  }
  gpio_ready = true;
  for (const int pin : kLedPins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
  return true;
}

void SetLedForBand(TempBand band) {
  // Rewrite all four pins every time instead of tracking which one is
  // lit: trivially correct, and four register pokes cost nothing
  const int lit = PinForBand(band);
  for (const int pin : kLedPins) {
    digitalWrite(pin, pin == lit ? HIGH : LOW);
  }
}

void LedsAllOff() {
  if (!gpio_ready) {
    return;
  }
  for (const int pin : kLedPins) {
    digitalWrite(pin, LOW);
  }
}
