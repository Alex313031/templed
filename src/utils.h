#ifndef TEMPLED_UTILS_H_
#define TEMPLED_UTILS_H_

#include "pch.h"

// Whether to use debug mode, even in a release build, set by -d/--debug.
extern bool want_debug;

// Whether to output extra debug information: true when either DEBUG/_DEBUG
// is defined (debug build) or the -d/--debug flag was passed.
bool IsDebugMode();

// Parses an integer; std::nullopt on malformed input
std::optional<long long> ParseInt(const std::string& in);

// Handles interrupt signals; `signum` is the number of the signal that
// fired. Turns the LEDs off and restores the terminal before exiting
void HandleSignal(int signum);

// RAII wrapper that switches the terminal to non-canonical, no-echo input
// for its lifetime, so single keypresses arrive immediately (no Enter
// needed) and aren't echoed over the status line - the POSIX equivalent of
// Win32 SetConsoleMode() clearing ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT.
// The destructor restores the previous settings; HandleSignal() restores
// them too, covering the Ctrl+C path. Does nothing if stdin isn't a tty
class RawTerminal {
 public:
  RawTerminal();
  ~RawTerminal();

  RawTerminal(const RawTerminal&)            = delete;
  RawTerminal& operator=(const RawTerminal&) = delete;
};

// Waits up to `delay`, returning early with true if Q or Esc was pressed
// (false = the delay elapsed, keep running). Falls back to a plain sleep
// when stdin isn't a tty in raw mode
bool WaitForQuit(std::chrono::milliseconds delay);

#endif // TEMPLED_UTILS_H_
