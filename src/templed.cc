// Copyright (c) 2026 Alex313031

// templed is a small command line program for the Raspberry Pi that displays temperature readings,
// and strobes four LEDS (Blue, Green, Yellow, Red), depending on temperature. Intended as an
// exercise in accessing the GPIO pins from C++.

#include "templed.h"

#include "gpio.h"
#include "sensors.h"
#include "utils.h"

// Whether to display temperatures in Fahrenheit (-f)
static bool use_fahrenheit = false;

// Whether the LEDs ignore the temperature and strobe at random (-b)
static bool use_blinkenlights = false;

namespace {
  // Refresh delay, in ms. (1 sec. default)
  constexpr std::chrono::milliseconds kDefaultDelayMs{kDefaultDelay};

  // The escape code matching each LED's literal color, so the status
  // line's "LED: Blue" prints in blue
  const char* BandColor(TempBand band) {
    switch (band) {
      case TempBand::kBlue:
        return kColorBlue;
      case TempBand::kGreen:
        return kColorGreen;
      case TempBand::kYellow:
        return kColorYellow;
      case TempBand::kRed:
        return kColorRed;
    }
    return kColorReset; // unreachable
  }
} // namespace

bool RefreshLoop(const std::chrono::milliseconds delay) {
  for (;;) {
    const std::optional<double> celsius = GetSocTempCelsius();
    if (!celsius) {
      std::cout << std::endl; // finish the in-place status line first
      std::cerr << kAppName << ": can't read the SOC temperature" << std::endl;
      return false;
    }
    // Pick the LED from the reading; the strobe loop below drives it
    const TempBand band = BandForTemp(*celsius);

    // The temperature value colors by the same thresholds raspimon uses:
    // green while comfortable, yellow from 60C, red from 80C - where the
    // firmware starts throttling (note: NOT the same cutoffs as the LED
    // bands, which are this program's own scheme)
    const char* temp_color = kColorGreen;
    if (*celsius >= 80.0) {
      temp_color = kColorRed;
    } else if (*celsius >= 60.0) {
      temp_color = kColorYellow;
    }

    std::ostringstream line;
    line << std::fixed << std::setprecision(1);
    line << Color(kColorBold) << "SOC: " << Color(kColorReset) << Color(temp_color);
    if (use_fahrenheit) {
      line << (*celsius * 9.0 / 5.0 + 32.0) << " " << kDegreeSymbol << "F.";
    } else {
      line << *celsius << " " << kDegreeSymbol << "C.";
    }
    line << Color(kColorReset);
    // No fan is normal (Pi 2-4, or a fanless Pi 5): just omit the reading
    if (const std::optional<long long> rpm = GetFanRpm()) {
      line << Color(kColorBold) << "  Fan: " << Color(kColorReset) << Color(kColorCyan) << *rpm
           << " RPM." << Color(kColorReset);
    }
    // The LED word paints itself in its literal color; -b gets magenta,
    // since the LEDs ignore the temperature band in that mode
    line << Color(kColorBold) << "  LED: " << Color(kColorReset);
    if (use_blinkenlights) {
      line << Color(kColorMagenta) << "blinkenlights" << Color(kColorReset);
    } else {
      line << Color(BandColor(band)) << BandName(band) << Color(kColorReset);
    }
    // One status line redrawn in place: '\r' returns the cursor to column
    // 0 so the next print overwrites this one, and ESC[K erases leftovers
    // when the new text is shorter than the old
    std::cout << '\r' << line.str() << "\033[K" << std::flush;

    // One "strobe" per refresh: the LED starts at full brightness and
    // fades to dark across the delay, updated in ~40ms slices (25 updates
    // a second reads as a continuous fade). The eye's response to duty
    // cycle is roughly logarithmic, so the linear fraction is squared to
    // make the fade look even instead of hovering near-bright; the quit
    // check runs every slice, keeping Q/Esc instant
    int slices = static_cast<int>(delay.count() / 40);
    if (slices < 1) {
      slices = 1;
    }
    const std::chrono::milliseconds slice = delay / slices;
    bool quit                             = false;
    for (int i = 0; i < slices && !quit; ++i) {
      if (use_blinkenlights) {
        // -b: the LEDs run their own random show; the readings above are
        // still taken and printed as normal
        BlinkenlightsTick(static_cast<int>(slice.count()));
      } else {
        const double fraction = 1.0 - (static_cast<double>(i) / slices);
        SetLedBrightness(band, static_cast<int>(100.0 * fraction * fraction));
      }
      quit = WaitForQuit(slice);
    }
    if (quit) {
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
               "  -t, --time <seconds>   Refresh and strobe every <seconds> seconds (default 1)\n"
               "  -f, --fahrenheit       Display temperatures in Fahrenheit\n"
               "  -b, --blinkenlights    Strobe the LEDs at random (1/4s-2s each), for fun\n"
               "  -d, --debug            Print extra debug output to stderr\n"
               "  -n, --no-color         Disable colored output (NO_COLOR env works too)\n"
               "  -v, --version          Show program version\n"
               "  -h, --help             Show this help message\n"
               "\n"
               "Press Q or Esc while running to quit.\n";
}

void ShowVersion() {
  static constexpr char app_ver[] = VERSION_STRING;
  std::cout << kAppName << " v" << app_ver << std::endl;
  std::cout << "Copyright " << kCopyrightSymbol << " " << COPYRIGHT_YEAR << " Alex313031."
            << std::endl;
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
      {"blinkenlights", no_argument, nullptr, 'b'},
      {"debug", no_argument, nullptr, 'd'},
      {"no-color", no_argument, nullptr, 'n'},
      {"version", no_argument, nullptr, 'v'},
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0}, // all-zeros terminator marks the table's end
  };
  while ((opt = getopt_long(argc, argv, "t:fbdnvh", kLongOptions, nullptr)) != -1) {
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
      case 'n':
        use_color = false;
        break;
      case 'f':
        use_fahrenheit = true;
        break;
      case 'b':
        use_blinkenlights = true;
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
  // Color only when a human is looking: stdout must be a terminal, and
  // the NO_COLOR convention (no-color.org) can veto it via the
  // environment, as can --no-color during parsing below
  use_color = isatty(STDOUT_FILENO) && std::getenv("NO_COLOR") == nullptr;

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
