// Copyright (c) 2026 Alex313031

// templed is a small command line program for the Raspberry Pi that displays temperature readings, and strobes
// four LEDS (Blue, Green, Yellow, Red), depending on temperature. Intended as an exercise
// in accessing the GPIO pins from C++.

#include "templed.h"

#include "gpio.h"
#include "sensors.h"
#include "utils.h"

// Whether to display temperatures in Fahrenheit (-f)
static bool use_fahrenheit = false;

namespace {
  // Refresh delay, in ms. (1 sec. default)
  constexpr std::chrono::milliseconds kDefaultDelayMs{kDefaultDelay};
} // namespace

bool RefreshLoop(const std::chrono::milliseconds delay) {
  for (;;) {
    const std::optional<double> celsius = GetSocTempCelsius();
    if (!celsius) {
      std::cout << std::endl; // finish the in-place status line first
      std::cerr << kAppName << ": can't read the SOC temperature" << std::endl;
      return false;
    }
    // Drive the LED from the reading, then describe both on one line
    const TempBand band = BandForTemp(*celsius);
    SetLedForBand(band);

    std::ostringstream line;
    line << std::fixed << std::setprecision(1) << "SOC: ";
    if (use_fahrenheit) {
      line << (*celsius * 9.0 / 5.0 + 32.0) << " " << kDegreeSymbol << "F.";
    } else {
      line << *celsius << " " << kDegreeSymbol << "C.";
    }
    // No fan is normal (Pi 2-4, or a fanless Pi 5): just omit the reading
    if (const std::optional<long long> rpm = GetFanRpm()) {
      line << "  Fan: " << *rpm << " RPM.";
    }
    line << "  LED: " << BandName(band);
    // One status line redrawn in place: '\r' returns the cursor to column
    // 0 so the next print overwrites this one, and ESC[K erases leftovers
    // when the new text is shorter than the old
    std::cout << '\r' << line.str() << "\033[K" << std::flush;

    if (WaitForQuit(delay)) {
      break; // Q or Esc pressed: a clean, deliberate quit
    }
  }
  std::cout << std::endl; // keep the final reading on screen
  return true;
}

void ShowHelp() {
  std::cout << "Usage: " << kAppName
            << " [ options ]\n"
               "An experimental temperature monitor for Raspberry Pi.\n\n"
               "Displays the SOC temperature, and strobes multiple colored LEDS accordingly.\n\n"
               "Options:\n"
               "  -t, --time <seconds>   Refresh every <seconds> seconds (default 1)\n"
               "  -f, --fahrenheit       Display temperatures in Fahrenheit\n"
               "  -d, --debug            Print extra debug output to stderr\n"
               "  -v, --version          Show program version\n"
               "  -h, --help             Show this help message\n"
               "\n"
               "Press Q or Esc while running to quit.\n";
}

void ShowVersion() {
  static constexpr char app_ver[] = VERSION_STRING;
  std::cout << kAppName << " v" << app_ver << std::endl;
  std::cout << "Copyright " << kCopyrightSymbol << " "
            << COPYRIGHT_YEAR << " Alex313031." << std::endl;
}

std::optional<int> ParseOptions(int argc, char* argv[], std::chrono::milliseconds& delay) {
  int opt;

  // getopt_long() is the GNU extension of getopt(), the standard POSIX
  // command-line parser (no Win32 equivalent - closest is manually walking
  // argv). The "t:fvh" string declares the short options: one letter per
  // flag, and a ':' after a letter means that flag requires a value, which
  // is delivered through the global `optarg` (so "t:" = "-t <seconds>").
  // Each entry in the table below maps a --long spelling to the same
  // character its short option returns, so one switch handles both
  // (--time also accepts "--time=2" and "--time 2"). Returns one option
  // character per call, '?' for anything unrecognized, -1 when done.
  //
  // To add a new flag: add its letter to the string (plus ':' if it takes
  // a value), a row in the table, a case in the switch, and a line in
  // ShowHelp()
  static constexpr struct option kLongOptions[] = {
      {"time", required_argument, nullptr, 't'},
      {"fahrenheit", no_argument, nullptr, 'f'},
      {"debug", no_argument, nullptr, 'd'},
      {"version", no_argument, nullptr, 'v'},
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0}, // all-zeros terminator marks the table's end
  };
  while ((opt = getopt_long(argc, argv, "t:fdvh", kLongOptions, nullptr)) != -1) {
    switch (opt) {
      case 't': {
        double seconds = 0.0;
        try {
          seconds = std::stod(optarg);
        } catch (const std::exception&) {
          // leave seconds at 0.0 so the range check below rejects it
        }
        if (seconds <= 0) {
          std::cerr << kAppName << ": invalid refresh delay '" << optarg << "'" << std::endl;
          return EXIT_FAILURE;
        }
        delay = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::duration<double>(seconds));
        break;
      }
      case 'd':
        want_debug = true;
        break;
      case 'f':
        use_fahrenheit = true;
        break;
      case 'v':
        ShowVersion();
        return EXIT_SUCCESS;
      case 'h':
        ShowHelp();
        return EXIT_SUCCESS;
      default:
        ShowHelp();
        return EXIT_FAILURE;
    }
  }
  return std::nullopt;
}

int main(int argc, char* argv[]) {
  std::chrono::milliseconds delay = kDefaultDelayMs;
  // A returned value means a flag already did its job (-h/-v printed) or
  // the command line was invalid; either way, quit with that exit code
  if (const std::optional<int> exit_code = ParseOptions(argc, argv, delay)) {
    return *exit_code;
  }

  if (!InitGpio()) {
    std::cerr << kAppName << ": GPIO setup failed (is this a Raspberry Pi?)" << std::endl;
    return EXIT_FAILURE;
  }
  // Register the handlers only after InitGpio(): they turn the LEDs off,
  // which needs the GPIO mapping ready
  std::signal(SIGINT, HandleSignal);
  std::signal(SIGTERM, HandleSignal);
  // Raw input for the whole run so Q/Esc arrive without needing Enter;
  // the destructor (or HandleSignal) restores the terminal on the way out
  const RawTerminal raw_terminal;

  const bool ok = RefreshLoop(delay);
  LedsAllOff();
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
