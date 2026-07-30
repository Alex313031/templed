// Copyright (c) 2026 Alex313031

// Sensor readings, all straight from the kernel's /sys tree - sysfs
// "files" are live views into kernel state, not data on disk, so reading
// them costs a syscall, not disk I/O

#include "sensors.h"

#include "utils.h"

std::optional<double> GetSocTempCelsius() {
  // thermal_zone0 is the SoC's sensor on every Pi generation. The file
  // holds millidegrees C as text: "52900" = 52.9 C
  std::ifstream zone("/sys/class/thermal/thermal_zone0/temp");
  if (!zone.is_open()) {
    return std::nullopt;
  }
  std::string line;
  if (!std::getline(zone, line)) {
    return std::nullopt;
  }
  const std::optional<long long> millidegrees = ParseInt(line);
  if (!millidegrees) {
    return std::nullopt;
  }
  return static_cast<double>(*millidegrees) / 1000.0;
}

std::optional<long long> GetFanRpm() {
  // The fan's tachometer is a text file holding the RPM. The hwmonN
  // directory in the middle of the path varies per boot, so expand the
  // wildcard the way the shell would
  glob_t matches{};
  std::optional<long long> rpm;
  if (glob("/sys/devices/platform/cooling_fan/hwmon/*/fan1_input", 0, nullptr, &matches) == 0 &&
      matches.gl_pathc > 0) {
    std::ifstream fan(matches.gl_pathv[0]);
    std::string line;
    if (fan.is_open() && std::getline(fan, line)) {
      rpm = ParseInt(line);
    }
  }
  globfree(&matches); // required even when glob() found nothing
  return rpm;
}
