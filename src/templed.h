#ifndef TEMPLED_TEMPLED_H_
#define TEMPLED_TEMPLED_H_

#include "pch.h"

// These next few lines are where we control version number
// Adhere to semver -> semver.org
#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define BUILD_VERSION 3

#define COPYRIGHT_YEAR "2026" // For ShowVersion()

// Macro to convert to string
#if !defined(STRINGIZE)
 #define STRINGIZER_(in) #in
 #define STRINGIZE(in)   STRINGIZER_(in)
#endif // !defined(STRINGIZE)

// Main version constants
#ifndef VERSION_
 // Run stringizer above
 #define VERSION_(major, minor, build) STRINGIZE(major.minor.build)
 // Version string
 #define VERSION_STRING VERSION_(MAJOR_VERSION, MINOR_VERSION, BUILD_VERSION)
#endif // VERSION_

inline constexpr char kAppName[] = "templed"; // name of the app

inline constexpr unsigned long kDefaultDelay = 1000UL; // default delay, 1000ms.

// In the UTF-8 encoding Linux terminals speak, characters beyond ASCII
// are multi-byte sequences (the copyright sign is the two bytes 0xC2
// 0xA9), so these must be char arrays - they don't fit in a single `char`
inline constexpr char kCopyrightSymbol[] = "\u00A9"; // The © symbol
inline constexpr char kDegreeSymbol[]    = "\u00B0"; // For temperature output

// Parses the command line, applying flag side effects (like -f) and filling
// in `delay`. Returns std::nullopt if the program should keep running, or
// the process exit code to quit with (after -h/-v, or on an invalid flag)
std::optional<int> ParseOptions(int argc, char* argv[], std::chrono::milliseconds& delay);

// Reads the temperature every `delay`, updates the LED and the status
// line, until Q/Esc (returns true) or a sensor failure (returns false)
bool RefreshLoop(std::chrono::milliseconds delay);

// Shows usage help message.
void ShowHelp();

// Shows version info.
void ShowVersion();

#endif // TEMPLED_TEMPLED_H_
