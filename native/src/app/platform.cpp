#include "app/platform.h"
#include "core/paths.h"
#include "raylib.h"
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>

#if defined(_WIN32)
// kernel32/psapi only: NOGDI + NOUSER keep windows.h from clashing with raylib's names
// (window activation lives in platform_win.cpp, which does not include raylib)
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
namespace ag::platform { void winActivateExisting(); }
#else
#include <csignal>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach/mach.h>
#include <malloc/malloc.h>
#elif defined(__GLIBC__) || defined(__linux__)
#include <malloc.h>
#endif
#endif

namespace ag::platform {

namespace {
FILE* gLog = nullptr;
bool gEcho = false;
std::mutex gLogMutex;
std::string gLogPath;
#if defined(_WIN32)
HANDLE gMutex = nullptr;
#else
int gLockFd = -1;
#endif

void logCallback(int level, const char* text, va_list args) {
    char msg[2048];
    std::vsnprintf(msg, sizeof msg, text, args);
    const char* tag = level >= LOG_FATAL ? "FATAL" : level >= LOG_ERROR ? "ERROR" : level >= LOG_WARNING ? "WARN " : level >= LOG_INFO ? "INFO " : "DEBUG";
    std::lock_guard<std::mutex> l(gLogMutex);
    if (gLog) {
        std::time_t t = std::time(nullptr);
        char ts[32];
        std::strftime(ts, sizeof ts, "%H:%M:%S", std::localtime(&t));
        std::fprintf(gLog, "%s %s %s\n", ts, tag, msg);
        if (level >= LOG_WARNING) std::fflush(gLog);
    }
    if (gEcho) std::fprintf(stderr, "%s: %s\n", tag, msg);
}

void crashNote(const char* what) {
    if (gLog) {
        std::fprintf(gLog, "FATAL crash: %s\n", what);
        std::fflush(gLog);
    }
}
}  // namespace

#if !defined(__ANDROID__)
float wheel() { return GetMouseWheelMove(); }
#endif

bool acquireSingleInstance() {
#if defined(_WIN32)
    gMutex = CreateMutexW(nullptr, TRUE, L"AfterglowGameMutex");
    if (gMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        winActivateExisting();
        CloseHandle(gMutex);
        gMutex = nullptr;
        return false;
    }
    return true;
#elif defined(__ANDROID__)
    return true;   // one activity per process already
#else
    std::string p = paths::join(paths::userDir(), ".instance.lock");
    gLockFd = ::open(p.c_str(), O_RDWR | O_CREAT, 0644);
    if (gLockFd < 0) return true;   // can't lock: don't block the player
    if (flock(gLockFd, LOCK_EX | LOCK_NB) != 0) { ::close(gLockFd); gLockFd = -1; return false; }
    return true;
#endif
}

void releaseSingleInstance() {
#if defined(_WIN32)
    if (gMutex) { ReleaseMutex(gMutex); CloseHandle(gMutex); gMutex = nullptr; }
#else
    if (gLockFd >= 0) { flock(gLockFd, LOCK_UN); ::close(gLockFd); gLockFd = -1; }
#endif
}

void openLog(bool echo) {
    gEcho = echo;
    std::string dir = paths::join(paths::userDir(), "logs");
    gLogPath = paths::join(dir, "afterglow.log");
    std::string prev = paths::join(dir, "afterglow.prev.log");
    paths::writeText(paths::join(dir, ".keep"), "");
    paths::removeFile(prev);
    std::rename(gLogPath.c_str(), prev.c_str());
    gLog = std::fopen(gLogPath.c_str(), "w");
    SetTraceLogCallback(logCallback);
}

void closeLog() {
    std::lock_guard<std::mutex> l(gLogMutex);
    if (gLog) { std::fclose(gLog); gLog = nullptr; }
}

std::string logPath() { return gLogPath; }

#if defined(_WIN32)
static LONG WINAPI onCrash(EXCEPTION_POINTERS* ep) {
    char b[96];
    std::snprintf(b, sizeof b, "exception 0x%08lx at %p", ep ? ep->ExceptionRecord->ExceptionCode : 0ul,
                  ep ? ep->ExceptionRecord->ExceptionAddress : nullptr);
    crashNote(b);
    return EXCEPTION_CONTINUE_SEARCH;
}
void installCrashHandler() { SetUnhandledExceptionFilter(onCrash); }
#else
static void onSignal(int sig) {
    char b[48];
    std::snprintf(b, sizeof b, "signal %d", sig);
    crashNote(b);
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}
void installCrashHandler() {
    for (int s : {SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS}) std::signal(s, onSignal);
}
#endif

namespace { volatile std::sig_atomic_t gQuit = 0; }
#if defined(_WIN32)
void installQuitSignals() {}
#else
static void onQuitSignal(int) { gQuit = 1; }
void installQuitSignals() {
    struct sigaction sa{};
    sa.sa_handler = onQuitSignal;
    sigemptyset(&sa.sa_mask);
    for (int s : {SIGTERM, SIGINT, SIGHUP}) sigaction(s, &sa, nullptr);
}
#endif
bool quitRequested() { return gQuit != 0; }

void trimHeap() {
#if defined(__ANDROID__)
    // bionic has no malloc_trim; the allocator returns pages on its own
#elif defined(_WIN32)
    HeapCompact(GetProcessHeap(), 0);
#elif defined(__APPLE__)
    malloc_zone_pressure_relief(nullptr, 0);
#elif defined(__GLIBC__)
    malloc_trim(0);
#endif
}

long long residentBytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc)) return (long long)pmc.WorkingSetSize;
    return 0;
#elif defined(__APPLE__)
    mach_task_basic_info info{};
    mach_msg_type_number_t n = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &n) == KERN_SUCCESS) return (long long)info.resident_size;
    return 0;
#else
    long long pages = 0, rss = 0;
    if (FILE* f = std::fopen("/proc/self/statm", "r")) {
        if (std::fscanf(f, "%lld %lld", &pages, &rss) != 2) rss = 0;
        std::fclose(f);
    }
    return rss * (long long)sysconf(_SC_PAGESIZE);
#endif
}

long long peakResidentBytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc)) return (long long)pmc.PeakWorkingSetSize;
    return 0;
#else
    struct rusage ru{};
    getrusage(RUSAGE_SELF, &ru);
#if defined(__APPLE__)
    return (long long)ru.ru_maxrss;
#else
    return (long long)ru.ru_maxrss * 1024;
#endif
#endif
}

}  // namespace ag::platform
