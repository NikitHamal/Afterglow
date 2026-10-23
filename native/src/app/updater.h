// Afterglow — in-app updater. Reads latest.json from the rolling GitHub release, compares versions,
// downloads the platform package with progress, verifies its SHA-256 and hands over to the installer
// (Windows: silent Inno Setup upgrade, then relaunch) or opens the release page (Linux / macOS).
#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace ag {

class Updater {
public:
    enum class State { Idle = 0, Checking, UpToDate, Available, Downloading, Ready, Failed };
    static constexpr const char* kFeedUrl = "https://github.com/NikitHamal/Afterglow/releases/download/nightly/latest.json";
    static constexpr const char* kPageUrl = "https://github.com/NikitHamal/Afterglow/releases/tag/nightly";

    ~Updater() { shutdown(); }
    void checkAsync(const std::string& feedUrl = kFeedUrl);
    void downloadAsync();
    // Windows: starts the verified installer silently and returns true (caller quits right away).
    // Elsewhere: opens the release page and returns false.
    bool install();
    void shutdown();

    State state() const { return (State)state_.load(); }
    float progress() const { long long t = total_.load(); return t > 0 ? (float)got_.load() / (float)t : 0.f; }
    std::string latest() const { std::lock_guard<std::mutex> l(m_); return version_; }
    std::string notes() const { std::lock_guard<std::mutex> l(m_); return notes_; }
    std::string error() const { std::lock_guard<std::mutex> l(m_); return error_; }
    static std::string current();
    static bool newer(const std::string& a, const std::string& b);   // a > b (numeric x.y.z)
    static bool isDevBuild();
    static const char* platformKey();

private:
    void fail(const std::string& why);
    std::thread th_;
    std::atomic<int> state_{0};
    std::atomic<long long> got_{0}, total_{0};
    std::atomic<bool> cancel_{false};
    mutable std::mutex m_;
    std::string version_, notes_, error_, url_, sha_, file_, page_;
    long long size_ = 0;
};

// helpers shared with tools/tests
bool httpGet(const std::string& url, std::string& body, std::string& err);
bool httpDownload(const std::string& url, const std::string& file, std::atomic<long long>& got, std::atomic<long long>& total,
                  std::atomic<bool>& cancel, std::string& err);
std::string sha256File(const std::string& path);

}  // namespace ag
