// Copyright (c) 2026 Alex313031

// LED control via the WiringPi library (vendored as a git submodule).
// WiringPi memory-maps the SoC's GPIO registers (through /dev/gpiomem0 on
// the Pi 5, /dev/gpiomem earlier), so digitalWrite() is a register poke,
// not a syscall - cheap enough to call every refresh without a thought.
// On Win32 there is no equivalent; PCs simply don't expose user GPIO.

#include "gpio.h"

#include <softPwm.h>
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
    // softPwmCreate() sets the pin mode itself and spawns that pin's PWM
    // thread; a range of 100 makes softPwmWrite() take a straight 0-100
    // percentage. Four threads waking ~100x/second is negligible CPU
    if (softPwmCreate(pin, 0, 100) != 0) {
      return false;
    }
  }
  return true;
}

void SetLedBrightness(TempBand band, int percent) {
  if (percent < 0) {
    percent = 0;
  } else if (percent > 100) {
    percent = 100;
  }
  // Rewrite all four pins every time instead of tracking which one is
  // lit: trivially correct, and four duty-cycle updates cost nothing
  const int lit = PinForBand(band);
  for (const int pin : kLedPins) {
    softPwmWrite(pin, pin == lit ? percent : 0);
  }
}

void LedsAllOff() {
  if (!gpio_ready) {
    return;
  }
  for (const int pin : kLedPins) {
    softPwmWrite(pin, 0);
    // Also force the line low directly: softPwmWrite() only posts the new
    // duty cycle for the PWM thread to act on, and on the _exit() path
    // (signal handler) that thread may never get to run again
    digitalWrite(pin, LOW);
  }
}
