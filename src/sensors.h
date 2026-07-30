#ifndef TEMPLED_SENSORS_H_
#define TEMPLED_SENSORS_H_

#include "pch.h"

// SOC temperature in Celsius, read from the kernel's thermal zone (the
// same sensor vcgencmd reads, but no firmware mailbox needed);
// std::nullopt on failure
std::optional<double> GetSocTempCelsius();

// Fan tachometer RPM from the kernel's hwmon interface - the fan on the
// Pi 5's dedicated fan header. std::nullopt when no fan is present
// (Pi 2-4, or a fanless Pi 5), which is normal, not an error
std::optional<long long> GetFanRpm();

#endif // TEMPLED_SENSORS_H_
