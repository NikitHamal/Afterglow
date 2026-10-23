#include "core/paths.h"
#include "core/log.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

namespace ag::paths {

bool exists(const std::string& p) { std::error_code ec; return !p.empty() && fs::exists(fs::u8path(p), ec); }
bool isDir(const std::string& p) { std::error_code ec; return !p.empty() && fs::is_directory(fs::u8path(p), ec); }
std::string join(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    char last = a.back();
    return (last == '/' || last == '\\') ? a + b : a + "/" + b;
}
std::string dirname(const std::string& p) { return fs::u8path(p).parent_path().u8string(); }

static std::string normalizeDir(const std::string& p) {
    std::error_code ec;
    fs::path c = fs::weakly_canonical(fs::u8path(p), ec);
    return ec ? p : c.u8string();
}

static std::string exeDir() {
    const char* d = GetApplicationDirectory();
    return d ? std::string(d) : std::string(".");
}

static bool looksLikeAssetRoot(const std::string& dir) {
    return isDir(join(dir, "characters")) || isDir(join(dir, "room")) || isDir(join(dir, "audio")) ||
           isDir(join(dir, "legacy")) || isDir(join(dir, "fonts"));
}

const std::string& assetRoot() {
    static std::string root;
    if (!root.empty()) return root;
    std::string ex = exeDir();
    std::string cwd = fs::current_path().u8string();
    const std::vector<std::string> cands = {
        join(ex, "assets"), join(ex, "../assets"), join(ex, "../Resources/assets"),
        join(cwd, "assets"), join(cwd, "native/assets"), join(ex, "../../native/assets"), join(ex, "../../../native/assets"),
    };
    for (auto& c : cands) if (looksLikeAssetRoot(c)) { root = normalizeDir(c); break; }
    if (root.empty()) root = normalizeDir(join(ex, "assets"));
    AG_INFO("AG: asset root %s", root.c_str());
    return root;
}

const std::string& repoRoot() {
    static std::string root; static bool done = false;
    if (done) return root;
    done = true;
    std::vector<std::string> starts = {exeDir(), fs::current_path().u8string()};
    for (auto s : starts) {
        fs::path p = fs::u8path(s);
        for (int up = 0; up < 6 && !p.empty(); ++up) {
            if (exists((p / "assets" / "vivian" / "vivian.glb").u8string())) { root = normalizeDir(p.u8string()); return root; }
            fs::path parent = p.parent_path();
            if (parent == p) break;
            p = parent;
        }
    }
    return root;
}

std::string asset(const std::string& rel) {
    std::string p = join(assetRoot(), rel);
    return exists(p) ? p : std::string();
}

std::string findAsset(const std::vector<std::string>& candidates) {
    for (auto& c : candidates) {
        std::string p = join(assetRoot(), c);
        if (exists(p)) return p;
    }
    if (!repoRoot().empty())
        for (auto& c : candidates) {
            std::string p = join(repoRoot(), c);
            if (exists(p)) return p;
        }
    return {};
}

std::string userDir() {
    std::string base;
#if defined(_WIN32)
    if (const char* a = std::getenv("APPDATA")) base = join(a, "Afterglow");
#elif defined(__APPLE__)
    if (const char* h = std::getenv("HOME")) base = join(h, "Library/Application Support/Afterglow");
#else
    if (const char* x = std::getenv("XDG_CONFIG_HOME")) base = join(x, "afterglow");
    else if (const char* h = std::getenv("HOME")) base = join(h, ".config/afterglow");
#endif
    if (base.empty()) base = join(exeDir(), "userdata");
    std::error_code ec;
    fs::create_directories(fs::u8path(base), ec);
    return base;
}

std::string readText(const std::string& path) {
    std::ifstream f(fs::u8path(path), std::ios::binary);
    if (!f) return {};
    std::ostringstream ss; ss << f.rdbuf();
    return ss.str();
}

bool writeText(const std::string& path, const std::string& text) {
    std::error_code ec;
    fs::create_directories(fs::u8path(path).parent_path(), ec);
    std::ofstream f(fs::u8path(path), std::ios::binary);
    if (!f) return false;
    f << text;
    return (bool)f;
}

bool writeAtomic(const std::string& path, const std::string& text) {
    std::string tmp = path + ".tmp";
    {
        std::error_code ec;
        fs::create_directories(fs::u8path(path).parent_path(), ec);
        std::ofstream f(fs::u8path(tmp), std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f << text;
        f.flush();
        if (!f) return false;
    }
    std::error_code ec;
    fs::rename(fs::u8path(tmp), fs::u8path(path), ec);   // replaces the target atomically
    if (ec) { fs::remove(fs::u8path(tmp), ec); return false; }
    return true;
}

bool removeFile(const std::string& path) { std::error_code ec; return fs::remove(fs::u8path(path), ec); }

long long fileSize(const std::string& path) {
    std::error_code ec;
    auto n = fs::file_size(fs::u8path(path), ec);
    return ec ? -1 : (long long)n;
}

std::string exeDirectory() { return exeDir(); }

std::string tempDir() {
    std::error_code ec;
    fs::path t = fs::temp_directory_path(ec);
    std::string d = ec ? join(userDir(), "tmp") : join(t.u8string(), "afterglow");
    fs::create_directories(fs::u8path(d), ec);
    return d;
}

std::vector<std::string> listFilesRecursive(const std::string& dir, const std::vector<std::string>& exts) {
    std::vector<std::string> out;
    std::error_code ec;
    if (!isDir(dir)) return out;
    for (auto it = fs::recursive_directory_iterator(fs::u8path(dir), ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        std::string ext = it->path().extension().u8string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (exts.empty() || std::find(exts.begin(), exts.end(), ext) != exts.end()) out.push_back(it->path().u8string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace ag::paths
