import pathlib

p = pathlib.Path('native/src/core/paths.h')
s = p.read_text(encoding='utf-8')
s = s.replace(
"""// Writable per-user directory (settings, saves, screenshots). Created on demand.
std::string userDir();""",
"""// Writable per-user directory (settings, saves, screenshots). Created on demand.
std::string userDir();
#if defined(__ANDROID__)
// Java hands the app's files dir over before android_main starts (NativeActivity has no writable cwd).
void setUserDirOverride(const std::string& dir);
#endif""")
p.write_text(s, encoding='utf-8')

p = pathlib.Path('native/src/core/paths.cpp')
s = p.read_text(encoding='utf-8')
s = s.replace('#include <algorithm>\n',
              '#include <algorithm>\n#if defined(__ANDROID__)\n#include "raylib.h"\n#endif\n')
s = s.replace(
"""namespace fs = std::filesystem;

namespace ag::paths {""",
"""namespace fs = std::filesystem;

namespace ag::paths {

#if defined(__ANDROID__)
namespace { std::string gFilesDir; }
void setUserDirOverride(const std::string& dir) { gFilesDir = dir; }
#endif""")
s = s.replace(
"""bool exists(const std::string& p) { std::error_code ec; return !p.empty() && fs::exists(fs::u8path(p), ec); }
bool isDir(const std::string& p) { std::error_code ec; return !p.empty() && fs::is_directory(fs::u8path(p), ec); }""",
"""bool exists(const std::string& p) {
#if defined(__ANDROID__)
    if (!p.empty() && p[0] != '/') return true;   // an APK asset: raylib opens it by name, there is no path to stat
#endif
    std::error_code ec; return !p.empty() && fs::exists(fs::u8path(p), ec);
}
bool isDir(const std::string& p) {
#if defined(__ANDROID__)
    if (!p.empty() && p[0] != '/') return true;   // virtual asset folder
#endif
    std::error_code ec; return !p.empty() && fs::is_directory(fs::u8path(p), ec);
}""")
s = s.replace(
"""static std::string exeDir() {
    const char* d = GetApplicationDirectory();
    return d ? std::string(d) : std::string(".");
}""",
"""static std::string exeDir() {
#if defined(__ANDROID__)
    return {};   // assets live inside the APK, addressed by relative name
#else
    const char* d = GetApplicationDirectory();
    return d ? std::string(d) : std::string(".");
#endif
}""")
s = s.replace(
"""const std::string& assetRoot() {
    static std::string root;
    if (!root.empty()) return root;""",
"""const std::string& assetRoot() {
#if defined(__ANDROID__)
    static std::string root;   // "" = the APK's assets/ directory (raylib resolves bare names there)
    return root;
#else
    static std::string root;
    if (!root.empty()) return root;""")
s = s.replace(
"""    AG_INFO("AG: asset root %s", root.c_str());
    return root;
}""",
"""    AG_INFO("AG: asset root %s", root.c_str());
    return root;
#endif
}""")
s = s.replace(
"""std::string asset(const std::string& rel) {
    std::string p = join(assetRoot(), rel);
    return exists(p) ? p : std::string();
}""",
"""std::string asset(const std::string& rel) {
#if defined(__ANDROID__)
    return rel;   // every shipped asset is in the APK (the loader warns and moves on when one is missing)
#else
    std::string p = join(assetRoot(), rel);
    return exists(p) ? p : std::string();
#endif
}""")
s = s.replace(
"""std::string findAsset(const std::vector<std::string>& candidates) {
    for (auto& c : candidates) {""",
"""std::string findAsset(const std::vector<std::string>& candidates) {
#if defined(__ANDROID__)
    return candidates.empty() ? std::string() : candidates.front();
#else
    for (auto& c : candidates) {""")
s = s.replace(
"""    return {};
}

std::string userDir() {""",
"""    return {};
#endif
}

std::string userDir() {""")
s = s.replace(
"""std::string userDir() {
    std::string base;
#if defined(_WIN32)""",
"""std::string userDir() {
#if defined(__ANDROID__)
    if (gFilesDir.empty()) gFilesDir = "/data/local/tmp/afterglow";   // only if Java never handed a path over
    std::error_code ec;
    fs::create_directories(fs::u8path(gFilesDir), ec);
    return gFilesDir;
#else
    std::string base;
#if defined(_WIN32)""")
s = s.replace(
"""    std::error_code ec;
    fs::create_directories(fs::u8path(base), ec);
    return base;
}""",
"""    std::error_code ec;
    fs::create_directories(fs::u8path(base), ec);
    return base;
#endif
}""")
s = s.replace(
"""std::string readText(const std::string& path) {
    std::ifstream f(fs::u8path(path), std::ios::binary);""",
"""std::string readText(const std::string& path) {
#if defined(__ANDROID__)
    if (!path.empty() && path[0] != '/') {   // APK asset: raylib's Android file layer
        int size = 0;
        unsigned char* data = LoadFileData(path.c_str(), &size);
        std::string out;
        if (data && size > 0) out.assign((const char*)data, (size_t)size);
        if (data) UnloadFileData(data);
        return out;
    }
#endif
    std::ifstream f(fs::u8path(path), std::ios::binary);""")
s = s.replace(
"""bool writeText(const std::string& path, const std::string& text) {
    std::error_code ec;""",
"""bool writeText(const std::string& path, const std::string& text) {
#if defined(__ANDROID__)
    if (!path.empty() && path[0] != '/')   // raylib writes relative names into the app's internal files dir
        return SaveFileData(path.c_str(), (void*)text.data(), (int)text.size());
#endif
    std::error_code ec;""")
s = s.replace(
"""bool writeAtomic(const std::string& path, const std::string& text) {
    std::string tmp = path + ".tmp";""",
"""bool writeAtomic(const std::string& path, const std::string& text) {
#if defined(__ANDROID__)
    if (!path.empty() && path[0] != '/') return writeText(path, text);   // no rename across the asset manager
#endif
    std::string tmp = path + ".tmp";""")
s = s.replace(
"""long long fileSize(const std::string& path) {
    std::error_code ec;""",
"""long long fileSize(const std::string& path) {
#if defined(__ANDROID__)
    if (!path.empty() && path[0] != '/') return -1;   // asset size is not exposed by the manager here
#endif
    std::error_code ec;""")
s = s.replace(
"""std::string tempDir() {
    std::error_code ec;""",
"""std::string tempDir() {
#if defined(__ANDROID__)
    std::string d = join(userDir(), "tmp");
    std::error_code ec;
    fs::create_directories(fs::u8path(d), ec);
    return d;
#else
    std::error_code ec;""")
s = s.replace(
"""    fs::create_directories(fs::u8path(d), ec);
    return d;
}

std::vector<std::string> listFilesRecursive""",
"""    fs::create_directories(fs::u8path(d), ec);
    return d;
#endif
}

std::vector<std::string> listFilesRecursive""")
s = s.replace(
"""std::vector<std::string> listFilesRecursive(const std::string& dir, const std::vector<std::string>& exts) {
    std::vector<std::string> out;
    std::error_code ec;
    if (!isDir(dir)) return out;""",
"""std::vector<std::string> listFilesRecursive(const std::string& dir, const std::vector<std::string>& exts) {
    std::vector<std::string> out;
#if defined(__ANDROID__)
    // APK assets cannot be enumerated: the build writes assets_manifest.json (one path per line)
    if (!dir.empty() && dir[0] != '/') {
        static std::vector<std::string> all;
        if (all.empty()) {
            int size = 0;
            unsigned char* data = LoadFileData("assets_manifest.json", &size);
            if (data && size > 0) {
                std::string text((const char*)data, (size_t)size), line;
                for (char c : text) {
                    if (c == '\\n') { if (!line.empty()) all.push_back(line); line.clear(); }
                    else if (c != '\\r') line.push_back(c);
                }
                if (!line.empty()) all.push_back(line);
            }
            if (data) UnloadFileData(data);
        }
        std::string prefix = dir;
        if (!prefix.empty() && prefix.back() != '/') prefix.push_back('/');
        for (const std::string& p : all) {
            if (p.compare(0, prefix.size(), prefix) != 0) continue;
            std::string ext = fs::u8path(p).extension().u8string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (exts.empty() || std::find(exts.begin(), exts.end(), ext) != exts.end()) out.push_back(p);
        }
        std::sort(out.begin(), out.end());
        return out;
    }
#endif
    std::error_code ec;
    if (!isDir(dir)) return out;""")
p.write_text(s, encoding='utf-8')
print('paths patched')
