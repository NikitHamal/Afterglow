// Afterglow — platform services: single instance, log file + crash notes in the user folder, process
// memory, and bringing an already running instance to the front.
#pragma once
#include <string>

namespace ag::platform {

// true when this is the only running instance; otherwise asks the running one to come forward.
// The Windows mutex name is shared with the installer (AppMutex) so setup can close the game.
bool acquireSingleInstance();
void releaseSingleInstance();

// routes raylib + engine logs to <user>/logs/afterglow.log (previous run kept as afterglow.prev.log)
void openLog(bool echoToConsole);
void closeLog();
std::string logPath();

// writes a crash note to the log and flushes it before the process dies
void installCrashHandler();

// SIGTERM / SIGINT / SIGHUP (logout, shutdown, kill) ask the main loop to quit gracefully (saves first)
void installQuitSignals();
bool quitRequested();

// resident memory of this process in bytes (0 when unknown)
long long residentBytes();
long long peakResidentBytes();

}  // namespace ag::platform
