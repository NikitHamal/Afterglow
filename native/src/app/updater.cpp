#include "app/updater.h"
#include "app/platform.h"
#include "core/json.h"
#include "core/log.h"
#include "core/paths.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#undef DrawText
#undef PlaySound
#undef LoadImage
#undef CloseWindow
#undef ShowCursor
#else
#include "raylib.h"   // OpenURL
#include <sys/wait.h>
#endif

#ifndef AG_VERSION
#define AG_VERSION "0.0.0-dev"
#endif

namespace ag {

// ------------------------------------------------------------------ SHA-256 (FIPS 180-4)
namespace {
struct Sha256 {
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    uint8_t buf[64];
    uint64_t len = 0;
    size_t n = 0;
    static uint32_t rotr(uint32_t x, int r) { return (x >> r) | (x << (32 - r)); }
    void block(const uint8_t* p) {
        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
            0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
            0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
            0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
            0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
            0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16 | (uint32_t)p[i * 4 + 2] << 8 | p[i * 4 + 3];
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25), ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + S1 + ch + k[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22), mj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + mj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    void update(const uint8_t* p, size_t m) {
        len += m;
        while (m) {
            size_t take = std::min(m, (size_t)64 - n);
            std::memcpy(buf + n, p, take);
            n += take; p += take; m -= take;
            if (n == 64) { block(buf); n = 0; }
        }
    }
    std::string hex() {
        uint64_t bits = len * 8;
        uint8_t one = 0x80, zero = 0;
        update(&one, 1);
        while (n != 56) update(&zero, 1);
        uint8_t lb[8];
        for (int i = 0; i < 8; ++i) lb[i] = (uint8_t)(bits >> (56 - 8 * i));
        update(lb, 8);
        char out[65];
        for (int i = 0; i < 8; ++i) std::snprintf(out + i * 8, 9, "%08x", h[i]);
        return std::string(out, 64);
    }
};
}  // namespace

std::string sha256File(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    Sha256 s;
    std::vector<char> b(1 << 16);
    while (f) {
        f.read(b.data(), (std::streamsize)b.size());
        std::streamsize got = f.gcount();
        if (got > 0) s.update((const uint8_t*)b.data(), (size_t)got);
    }
    return s.hex();
}

// ------------------------------------------------------------------ HTTP
#if defined(_WIN32)
namespace {
std::wstring widen(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w((size_t)std::max(0, n - 1), L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}
// streams a GET response into `sink`; follows redirects (WinHTTP default policy)
bool winGet(const std::string& url, const std::function<bool(const char*, DWORD)>& sink, long long* contentLength, std::string& err) {
    std::wstring wurl = widen(url);
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof uc;
    wchar_t host[256], path[2048];
    uc.lpszHostName = host; uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) { err = "bad url"; return false; }
    HINTERNET ses = WinHttpOpen(L"Afterglow-Updater/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) ses = WinHttpOpen(L"Afterglow-Updater/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) { err = "WinHttpOpen failed"; return false; }
    WinHttpSetTimeouts(ses, 10000, 15000, 30000, 60000);
    HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0);
    HINTERNET req = con ? WinHttpOpenRequest(con, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                              uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : nullptr;
    bool ok = false;
    if (req && WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(req, nullptr)) {
        DWORD status = 0, sz = sizeof status;
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
        if (status == 200) {
            wchar_t lenbuf[32]; DWORD lsz = sizeof lenbuf;
            if (contentLength && WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, lenbuf, &lsz, WINHTTP_NO_HEADER_INDEX))
                *contentLength = _wtoi64(lenbuf);
            std::vector<char> b(1 << 16);
            ok = true;
            for (;;) {
                DWORD got = 0;
                if (!WinHttpReadData(req, b.data(), (DWORD)b.size(), &got)) { ok = false; err = "read failed"; break; }
                if (got == 0) break;
                if (!sink(b.data(), got)) { ok = false; err = "cancelled"; break; }
            }
        } else err = "HTTP " + std::to_string(status);
    } else if (err.empty()) err = "request failed (" + std::to_string(GetLastError()) + ")";
    if (req) WinHttpCloseHandle(req);
    if (con) WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return ok;
}
}  // namespace

bool httpGet(const std::string& url, std::string& body, std::string& err) {
    body.clear();
    return winGet(url, [&](const char* p, DWORD n) { body.append(p, n); return body.size() < (8u << 20); }, nullptr, err);
}

bool httpDownload(const std::string& url, const std::string& file, std::atomic<long long>& got, std::atomic<long long>& total,
                  std::atomic<bool>& cancel, std::string& err) {
    std::ofstream f(widen(file), std::ios::binary | std::ios::trunc);
    if (!f) { err = "cannot write " + file; return false; }
    long long len = 0;
    got = 0;
    bool ok = winGet(url, [&](const char* p, DWORD n) {
        f.write(p, n);
        got += n;
        return !cancel.load() && (bool)f;
    }, &len, err);
    if (len > 0) total = len;
    return ok && (bool)f;
}
#else
namespace {
std::string shellQuote(const std::string& s) {
    std::string o = "'";
    for (char c : s) { if (c == '\'') o += "'\\''"; else o += c; }
    return o + "'";
}
}  // namespace

bool httpGet(const std::string& url, std::string& body, std::string& err) {
    body.clear();
    std::string cmd = "curl -fsSL --max-time 20 -A Afterglow-Updater " + shellQuote(url) + " 2>/dev/null";
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) { err = "curl unavailable"; return false; }
    char b[4096];
    size_t n;
    while ((n = fread(b, 1, sizeof b, p)) > 0) { body.append(b, n); if (body.size() > (8u << 20)) break; }
    int rc = pclose(p);
    if (rc != 0 || body.empty()) { err = "download failed (curl " + std::to_string(rc) + ")"; return false; }
    return true;
}

bool httpDownload(const std::string& url, const std::string& file, std::atomic<long long>& got, std::atomic<long long>& total,
                  std::atomic<bool>& cancel, std::string& err) {
    // curl runs in the background; progress is the growing file size against the size from latest.json
    std::string cmd = "curl -fsSL --max-time 1800 -A Afterglow-Updater -o " + shellQuote(file) + " " + shellQuote(url) + " 2>/dev/null";
    std::atomic<bool> done{false};
    int rc = -1;
    std::thread t([&] { rc = std::system(cmd.c_str()); done = true; });
    while (!done.load()) {
        long long sz = paths::fileSize(file);
        if (sz > 0) got = sz;
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        if (cancel.load()) break;
    }
    t.join();
    long long sz = paths::fileSize(file);
    if (sz > 0) got = sz;
    if (rc != 0) { err = "download failed"; return false; }
    if (total.load() <= 0) total = sz;
    return sz > 0;
}
#endif

// ------------------------------------------------------------------ Updater
std::string Updater::current() { return AG_VERSION; }
bool Updater::isDevBuild() { return std::strstr(AG_VERSION, "dev") != nullptr; }

const char* Updater::platformKey() {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return "linux";
#endif
}

bool Updater::newer(const std::string& a, const std::string& b) {
    int x[3] = {0, 0, 0}, y[3] = {0, 0, 0};
    std::sscanf(a.c_str(), "%d.%d.%d", &x[0], &x[1], &x[2]);
    std::sscanf(b.c_str(), "%d.%d.%d", &y[0], &y[1], &y[2]);
    for (int i = 0; i < 3; ++i) if (x[i] != y[i]) return x[i] > y[i];
    return false;
}

void Updater::fail(const std::string& why) {
    { std::lock_guard<std::mutex> l(m_); error_ = why; }
    AG_WARN("AG: updater: %s", why.c_str());
    state_ = (int)State::Failed;
}

void Updater::checkAsync(const std::string& feed) {
    if (state() == State::Checking || state() == State::Downloading) return;
    if (th_.joinable()) th_.join();
    state_ = (int)State::Checking;
    th_ = std::thread([this, feed] {
        std::string body, err;
        if (!httpGet(feed, body, err)) { fail("update check: " + err); return; }
        Json j = Json::parse(body);
        std::string v = j["version"].str();
        const Json& a = j["assets"][platformKey()];
        if (v.empty() || !a.isObject()) { fail("update check: unexpected feed"); return; }
        {
            std::lock_guard<std::mutex> l(m_);
            version_ = v;
            notes_ = j["notes"].str();
            url_ = a["url"].str();
            sha_ = a["sha256"].str();
            size_ = (long long)a["size"].num(0);
            page_ = j["page"].str(kPageUrl);
        }
        bool avail = !isDevBuild() && newer(v, current());
        AG_INFO("AG: updater: current %s, latest %s%s", current().c_str(), v.c_str(), avail ? " (update available)" : "");
        state_ = (int)(avail ? State::Available : State::UpToDate);
    });
}

void Updater::downloadAsync() {
    if (state() != State::Available && state() != State::Failed) return;
    if (th_.joinable()) th_.join();
    std::string url, sha, ver;
    long long size;
    {
        std::lock_guard<std::mutex> l(m_);
        url = url_; sha = sha_; ver = version_; size = size_;
    }
    if (url.empty()) { fail("no package for this platform"); return; }
    std::string name = url.substr(url.find_last_of('/') + 1);
    std::string file = paths::join(paths::tempDir(), ver + "-" + name);
    { std::lock_guard<std::mutex> l(m_); file_ = file; }
    got_ = 0; total_ = size; cancel_ = false;
    state_ = (int)State::Downloading;
    th_ = std::thread([this, url, sha, file] {
        std::string err;
        if (!httpDownload(url, file, got_, total_, cancel_, err)) { fail("download: " + err); return; }
        std::string have = sha256File(file);
        if (!sha.empty() && have != sha) {
            paths::removeFile(file);
            fail("download: checksum mismatch");
            return;
        }
        AG_INFO("AG: updater: downloaded %s (sha256 ok)", file.c_str());
        state_ = (int)State::Ready;
    });
}

bool Updater::install() {
    std::string file, page;
    {
        std::lock_guard<std::mutex> l(m_);
        file = file_; page = page_.empty() ? std::string(kPageUrl) : page_;
    }
#if defined(_WIN32)
    if (state() != State::Ready || file.empty()) return false;
    std::wstring exe = widen(file);
    std::wstring cmd = L"\"" + exe + L"\" /VERYSILENT /SUPPRESSMSGBOXES /CLOSEAPPLICATIONS /NORESTART /RELAUNCH";
    STARTUPINFOW si{}; si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> line(cmd.begin(), cmd.end());
    line.push_back(0);
    if (!CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, DETACHED_PROCESS, nullptr, nullptr, &si, &pi)) {
        fail("could not start the installer");
        return false;
    }
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    // setup checks AppMutex on start: let go of it now, the game exits right after this
    platform::releaseSingleInstance();
    AG_INFO("AG: updater: installer started, quitting");
    return true;
#else
    (void)file;
    OpenURL(page.c_str());
    return false;
#endif
}

void Updater::shutdown() {
    cancel_ = true;
    if (th_.joinable()) th_.join();
}

}  // namespace ag
