#ifndef TEMPLED_GPIO_H_
#define TEMPLED_GPIO_H_

#include "pch.h"

// The four LEDs, one per temperature band, and the BCM GPIO pin each is
// wired to. BCM numbers are what the CanaKit breakout board's labels use
// ("GPIO17"), NOT the physical 1-40 header positions. Wiring, per LED:
//   GPIO pin -> 330 ohm resistor -> LED long leg (anode),
//   LED short leg (cathode) -> ground rail (physical pin 9, 14, or 20)
// The resistor is not optional: a bare LED overdraws the 3.3V pin (Pi 5
// pins default to 8mA drive) and can damage the SoC.
//
//   LED     BCM   physical pin   lights when
//   Blue    17    11             temp <= 45 C
//   Green   27    13             temp <= 65 C
//   Yellow  22    15             temp <= 79 C
//   Red     23    16             temp >= 80 C (throttling territory)
inline constexpr int kBluePin   = 17;
inline constexpr int kGreenPin  = 27;
inline constexpr int kYellowPin = 22;
inline constexpr int kRedPin    = 23;

// Temperature bands, coolest to hottest
enum class TempBand { kBlue, kGreen, kYellow, kRed };

// Which band a SOC temperature falls in
TempBand BandForTemp(double celsius);

// Display name of a band's LED ("Blue", "Green", ...)
const char* BandName(TempBand band);

// Maps the GPIO registers and puts the four LED pins in output mode (all
// off); returns false if GPIO setup failed. Call before anything below
bool InitGpio();

// Lights `band`'s LED and turns the other three off
void SetLedForBand(TempBand band);

// All four LEDs off; called on every path out of the program so an LED
// isn't left burning after exit. Safe to call even if InitGpio() failed
void LedsAllOff();

#endif // TEMPLED_GPIO_H_
