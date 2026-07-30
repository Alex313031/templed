// Copyright (c) 2026 Alex313031

// General utility functions

#include "utils.h"

#include "gpio.h"

// Defined here so it lives with its extern declaration in utils.h; set by
// ParseOptions() when -d/--debug is passed
bool want_debug = false;

namespace {
  // The terminal settings from before RawTerminal switched to raw input,
  // kept at file scope so HandleSignal() can also restore them
  termios orig_termios;
  bool termios_saved = false;
} // namespace

bool IsDebugMode() {
  return is_debug || want_debug;
}

std::optional<long long> ParseInt(const std::string& in) {
  try {
    return std::stoll(in);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

// Handles SIGINT (Ctrl+C) and SIGTERM (polite kill) - POSIX signals are
// the rough equivalent of a Win32 console control handler, except the
// handler runs by interrupting the program mid-instruction on its own
// stack. Because of that, only "async-signal-safe" work is allowed here:
// raw syscalls and register pokes, but NOT std::cout (it might be halfway
// through a write, holding its internal lock, when the signal hits)
void HandleSignal(int signum) {
  // LEDs first - leaving one burning is the worst failure mode. WiringPi's
  // digitalWrite() is a memory-mapped register write, which is
  // handler-safe (no locks, no allocation)
  LedsAllOff();
  // tcsetattr() is on POSIX's async-signal-safe list, so undoing raw mode
  // here is allowed
  if (termios_saved) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
  }
  // End the in-place status line so the shell prompt starts on a fresh row
  ssize_t newline = write(STDOUT_FILENO, "\n", 1);
  (void)newline;
  // Exit with the shell convention for "quit by signal N" (128 + N, e.g.
  // Ctrl+C -> 130) so scripts can tell a signal quit from success (0) or
  // a real failure (1). _exit() skips destructors and stream flushing,
  // the unsafe-in-a-handler parts of a normal exit()
  _exit(128 + signum);
}

RawTerminal::RawTerminal() {
  if (!isatty(STDIN_FILENO)) {
    return; // stdin is a pipe/file: nothing to configure, keys can't arrive
  }
  if (tcgetattr(STDIN_FILENO, &orig_termios) != 0) {
    return;
  }
  termios raw   = orig_termios;
  termios_saved = true;
  // ICANON off = deliver bytes as they are typed instead of buffering a
  // whole line until Enter; ECHO off = don't print keys over the display
  raw.c_lflag &= ~(ICANON | ECHO);
  raw.c_cc[VMIN]  = 0; // read() may return with nothing...
  raw.c_cc[VTIME] = 0; // ...and never blocks (poll() does the waiting)
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

RawTerminal::~RawTerminal() {
  if (termios_saved) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    termios_saved = false;
  }
}

bool WaitForQuit(std::chrono::milliseconds delay) {
  if (!termios_saved) {
    // Not a tty (or raw mode failed): keys can't be read sensibly, so
    // just sleep out the refresh delay
    std::this_thread::sleep_for(delay);
    return false;
  }
  // Instead of sleeping, poll() stdin with the refresh delay as timeout:
  // it returns as soon as a key arrives or the time runs out, whichever
  // comes first (like WaitForSingleObject() on the console handle)
  const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + delay;
  for (;;) {
    const std::chrono::milliseconds remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline -
                                                              std::chrono::steady_clock::now());
    if (remaining <= std::chrono::milliseconds::zero()) {
      return false;
    }
    pollfd request{STDIN_FILENO, POLLIN, 0};
    const int ready = poll(&request, 1, static_cast<int>(remaining.count()));
    if (ready < 0) {
      if (errno == EINTR) {
        continue; // a signal interrupted the wait: resume it
      }
      return false;
    }
    if (ready == 0) {
      return false; // timed out: take the next reading
    }
    char key = 0;
    if (read(STDIN_FILENO, &key, 1) != 1) {
      return false; // EOF: stdin closed under us
    }
    if (key == 'q' || key == 'Q') {
      return true;
    }
    if (key == '\033') {
      // A lone Esc quits, but Esc is also how terminals encode special
      // keys (an arrow key arrives as the bytes ESC [ A): if more bytes
      // follow immediately, it's such a sequence - swallow and ignore it
      pollfd more{STDIN_FILENO, POLLIN, 0};
      if (poll(&more, 1, 0) <= 0) {
        return true;
      }
      std::array<char, 8> discard;
      ssize_t drained = read(STDIN_FILENO, discard.data(), discard.size());
      (void)drained;
    }
    // Any other key: ignore it and wait out the remaining time
  }
}
