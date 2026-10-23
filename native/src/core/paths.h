// Afterglow engine — asset discovery and per-user data directory.
#pragma once
#include <string>
#include <vector>

namespace ag::paths {

// Directory holding the shipped assets (characters/, room/, audio/, fonts/). Resolved once.
const std::string& assetRoot();

// Absolute path of an asset relative to assetRoot(); empty when it does not exist.
std::string asset(const std::string& rel);

// First existing file among candidates (relative to assetRoot(), then to the legacy repo assets).
std::string findAsset(const std::vector<std::string>& candidates);

// Repository root when running from a source checkout (for legacy fallback assets); may be empty.
const std::string& repoRoot();

// Writable per-user directory (settings, saves, screenshots). Created on demand.
std::string userDir();

bool exists(const std::string& path);
bool isDir(const std::string& path);
std::string join(const std::string& a, const std::string& b);
std::string dirname(const std::string& path);
std::string readText(const std::string& path);
bool writeText(const std::string& path, const std::string& text);
std::vector<std::string> listFilesRecursive(const std::string& dir, const std::vector<std::string>& exts);

}  // namespace ag::paths
